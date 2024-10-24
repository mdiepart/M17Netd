/***************************************************************************
 *   Copyright (C) 2021 - 2024 by Federico Amedeo Izzo IU2NUO,             *
 *                                Niccolò Izzo IU2KIN                      *
 *                                Wojciech Kaczmarski SP5WWP               *
 *                                Frederik Saraci IU2NRO                   *
 *                                Silvano Seva IU2KWO                      *
 *                                Morgan Diepart ON4MOD                    *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <M17Demodulator.hpp>
#include <M17Utils.hpp>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <m17.h>
#include <iostream>

using namespace M17;

#ifdef ENABLE_DEMOD_LOG

#include <ringbuf.hpp>
#include <atomic>
#ifndef PLATFORM_LINUX
#include <usb_vcom.h>
#endif

typedef struct
{
    int16_t sample;
    int32_t conv;
    float   conv_th;
    int32_t sample_index;
    float   qnt_pos_avg;
    float   qnt_neg_avg;
    int32_t symbol;
    int32_t frame_index;
    uint8_t flags;
    uint8_t _empty;
}
__attribute__((packed)) log_entry_t;

#ifdef PLATFORM_LINUX
#define LOG_QUEUE 160000
#else
#define LOG_QUEUE 1024
#endif

static RingBuffer< log_entry_t, LOG_QUEUE > logBuf;
static std::atomic_bool dumpData;
static bool      logRunning;
static bool      trigEnable;
static bool      triggered;
static uint32_t  trigCnt;
static pthread_t logThread;

static void *logFunc(void *arg)
{
    (void) arg;

    #ifdef PLATFORM_LINUX
    FILE *csv_log = fopen("demod_log.csv", "w");
    fprintf(csv_log, "Sample,Convolution,Threshold,Index,Max,Min,Symbol,I,Flags\n");
    #endif

    uint8_t emptyCtr = 0;

    while(logRunning)
    {
        if(dumpData)
        {
            // Log up to four entries filled with zeroes before terminating
            // the dump.
            log_entry_t entry;
            memset(&entry, 0x00, sizeof(log_entry_t));
            if(logBuf.pop(entry, false) == false) emptyCtr++;

            if(emptyCtr >= 100)
            {
                dumpData = false;
                emptyCtr = 0;
                #ifdef PLATFORM_LINUX
                logRunning = false;
                #endif
            }

            #ifdef PLATFORM_LINUX
            fprintf(csv_log, "%d,%d,%f,%d,%f,%f,%d,%d,%d\n",
                    entry.sample,
                    entry.conv,
                    entry.conv_th,
                    entry.sample_index,
                    entry.qnt_pos_avg,
                    entry.qnt_neg_avg,
                    entry.symbol,
                    entry.frame_index,
                    entry.flags);
            fflush(csv_log);
            #else
            vcom_writeBlock(&entry, sizeof(log_entry_t));
            #endif
        }
    }

    #ifdef PLATFORM_LINUX
    fclose(csv_log);
    exit(-1);
    #endif

    return NULL;
}

static inline void pushLog(const log_entry_t& e)
{
    /*
     * 1) do not push data to log while dump is in progress
     * 2) if triggered, increase the counter
     * 3) fill half of the buffer with entries after the trigger, then start dump
     * 4) if buffer is full, erase the oldest element
     * 5) push data without blocking
     */

    if(dumpData) return;
    if(triggered) trigCnt++;
    if(trigCnt >= LOG_QUEUE/2)
    {
        dumpData  = true;
        triggered = false;
        trigCnt   = 0;
    }
    if(logBuf.full()) logBuf.eraseElement();
    logBuf.push(e, false);
}

#endif


M17Demodulator::M17Demodulator()
{
    dcr = iirfilt_rrrf_create_dc_blocker(0.001f);
    float taps[161];
    memcpy(taps, rrc_taps_20, 161*sizeof(float));
    rrcos_filt = firfilt_rrrf_create(taps, 161);

}

M17Demodulator::~M17Demodulator()
{
    terminate();
    iirfilt_rrrf_destroy(dcr);
    firfilt_rrrf_destroy(rrcos_filt);
}

void M17Demodulator::init()
{
    /*
     * Allocate a chunk of memory to contain two complete buffers for baseband
     * audio. Split this chunk in two separate blocks for double buffering using
     * placement new.
     */

    demodFrame      = std::make_unique< m17frame_t >();
    readyFrame      = std::make_unique< m17frame_t >();

    reset();

    #ifdef ENABLE_DEMOD_LOG
    logRunning = true;
    triggered  = false;
    dumpData   = false;
    trigEnable = false;
    trigCnt    = 0;
    pthread_create(&logThread, NULL, logFunc, NULL);
    #endif
    cout << "M17 Demodulator initialized" << endl;
}

void M17Demodulator::terminate()
{
    demodFrame.reset();
    readyFrame.reset();

    #ifdef ENABLE_DEMOD_LOG
    logRunning = false;
    #endif
}

const M17::m17frame_t& M17Demodulator::getFrame()
{
    // When a frame is read is not new anymore
    newFrame = false;
    return *readyFrame;
}

bool M17Demodulator::isLocked() const
{
    return locked;
}

bool M17Demodulator::update(float *samples, const size_t N)
{
    if(samples != nullptr)
    {
        // Apply DC removal filter
        iirfilt_rrrf_execute_block(dcr, samples, N, samples);

        // Process samples
        for(size_t i = 0; i < N; i++)
        {
            // Apply RRC on the baseband sample
            float   elem;
            firfilt_rrrf_push(rrcos_filt, samples[i]);
            firfilt_rrrf_execute(rrcos_filt, &elem);
            int16_t sample = static_cast<int16_t>(elem*500); // Scale up

            // Update correlator and sample filter for correlation thresholds
            correlator.sample(sample);
            corrThreshold = sampleFilter(std::abs(sample));

            switch(demodState)
            {
                case DemodState::INIT:
                {
                    initCount -= 1;
                    if(initCount == 0){
                        demodState = DemodState::UNLOCKED;
                        cout << "M17 Demodulator: Unlocked" << endl;
                    }

                }
                    break;

                case DemodState::UNLOCKED:
                {
                    int32_t syncThresh = static_cast< int32_t >(corrThreshold * 32.0f);
                    //int8_t  packetSyncStatus = packetSync.update(correlator, syncThresh, -syncThresh);
                    int8_t  lsfSyncStatus = lsfSync.update(correlator, syncThresh, -syncThresh);

                    // We only check for positive correlation peaks because we
                    // are not interrested in streams and BERTs
                    /*if(packetSyncStatus == 1)
                    {
                        demodState = DemodState::SYNCED;
                        lastSyncWord = SyncWord::PACKET;
                        cout << "M17 Demodulator: Received packet sync: Unlock -> Synced" << endl;
                    }
                    else */if(lsfSyncStatus == 1)
                    {
                        demodState = DemodState::SYNCED;
                        lastSyncWord = SyncWord::LSF;
                        cout << "M17 Demodulator: Received LSF sync: Unlock -> Synced" << endl;
                    }
                }
                    break;

                case DemodState::SYNCED:
                {
                    // Set sampling point and deviation, zero frame symbol count
                    if(lastSyncWord == SyncWord::PACKET)
                    {
                        samplingPoint = packetSync.samplingIndex();
                    }
                    else if(lastSyncWord == SyncWord::LSF)
                    {
                        samplingPoint = lsfSync.samplingIndex();
                    }

                    outerDeviation = correlator.maxDeviation(samplingPoint);
                    frameIndex     = 0;

                    // Quantize the syncword taking data from the correlator
                    // memory.
                    for(size_t i = 0; i < SYNCWORD_SAMPLES; i++)
                    {
                        size_t  pos = (correlator.index() + i) % SYNCWORD_SAMPLES;
                        int16_t val = correlator.data()[pos];

                        if((pos % SAMPLES_PER_SYMBOL) == samplingPoint)
                            updateFrame(val);
                    }

                    if(lastSyncWord == SyncWord::PACKET)
                    {
                        uint8_t hd  = hammingDistance( (*demodFrame)[0], PACKET_SYNC_WORD[0]);
                                hd += hammingDistance( (*demodFrame)[1], PACKET_SYNC_WORD[1]);

                        if(hd == 0)
                        {
                            locked     = true;
                            demodState = DemodState::LOCKED;
                            cout << "M17Demodulator: Received packet sync: Synced -> Locked" << endl;
                        }
                        else
                        {
                            demodState = DemodState::UNLOCKED;
                            cout << "M17Demodulator: PACKET sync not recognized. hd=" << static_cast<uint16_t>(hd) << ", Synced -> Unlocked" << endl;
                        }
                    }else if(lastSyncWord == SyncWord::LSF)
                    {
                        uint8_t hd  = hammingDistance( (*demodFrame)[0], LSF_SYNC_WORD[0]);
                                hd += hammingDistance( (*demodFrame)[1], LSF_SYNC_WORD[1]);
                        if(hd == 0)
                        {
                            locked     = true;
                            demodState = DemodState::LOCKED;
                            cout << "M17Demodulator: Received LSF sync: Synced -> Locked" << endl;
                        }
                        else
                        {
                            demodState = DemodState::UNLOCKED;
                            cout << "M17Demodulator: LSF sync not recognized. hd=" << static_cast<uint16_t>(hd) << ", Synced -> Unlocked" << endl;
                        }
                    }
                }
                    break;

                case DemodState::LOCKED:
                {
                    // Quantize and update frame at each sampling point
                    if(sampleIndex == samplingPoint)
                    {
                        updateFrame(sample);

                        // When we have reached almost the end of a frame, switch
                        // to syncpoint update.
                        if(frameIndex == (M17_FRAME_SYMBOLS - M17_SYNCWORD_SYMBOLS/2))
                        {
                            demodState = DemodState::SYNC_UPDATE;
                            syncCount  = SYNCWORD_SAMPLES * 2;
                            cout << "M17 Demodulator: Locked -> Sync Update" << endl;
                        }
                    }
                }
                    break;

                case DemodState::SYNC_UPDATE:
                {
                    // Keep filling the ongoing frame!
                    if(sampleIndex == samplingPoint)
                        updateFrame(sample);

                    // Find the new correlation peak
                    int32_t syncThresh = static_cast< int32_t >(corrThreshold * 32.0f);
                    int8_t  packetSyncStatus = packetSync.update(correlator, syncThresh, -syncThresh);
                    int8_t  lsfSyncStatus = lsfSync.update(correlator, syncThresh, -syncThresh);

                    if(packetSyncStatus != 0)
                    {
                        // Correlation has to coincide with a syncword!
                        if(frameIndex == M17_SYNCWORD_SYMBOLS)
                        {
                            uint8_t hd  = hammingDistance((*demodFrame)[0], PACKET_SYNC_WORD[0]);
                                    hd += hammingDistance((*demodFrame)[1], PACKET_SYNC_WORD[1]);

                            // Valid sync found: update deviation and sample
                            // point, then go back to locked state
                            if(hd <= 1)
                            {
                                outerDeviation = correlator.maxDeviation(samplingPoint);
                                samplingPoint  = packetSync.samplingIndex();
                                missedSyncs    = 0;
                                demodState     = DemodState::LOCKED;
                                cout << "M17 Demodulator: Received packet sync: Sync Update -> Locked" << endl;
                                break;
                            }
                        }
                    }
                    else if(lsfSyncStatus)
                    {
                        // Correlation has to coincide with a syncword!
                        if(frameIndex == M17_SYNCWORD_SYMBOLS)
                        {
                            uint8_t hd  = hammingDistance((*demodFrame)[0], LSF_SYNC_WORD[0]);
                                    hd += hammingDistance((*demodFrame)[1], LSF_SYNC_WORD[1]);

                            // Valid sync found: update deviation and sample
                            // point, then go back to locked state
                            if(hd <= 1)
                            {
                                outerDeviation = correlator.maxDeviation(samplingPoint);
                                samplingPoint  = lsfSync.samplingIndex();
                                missedSyncs    = 0;
                                demodState     = DemodState::LOCKED;
                                cout << "M17 Demodulator: Received LSF sync: Sync Update -> Locked" << endl;
                                break;
                            }
                        }
                    }

                    // No syncword found within the window, increase the count
                    // of missed syncs and choose where to go. The lock is lost
                    // after four consecutive sync misses.
                    if(syncCount == 0)
                    {
                        if(missedSyncs >= 4)
                        {
                            demodState = DemodState::UNLOCKED;
                            cout << "M17 Demodulator: Missed too many syncs: Sync Update -> Unlocked" << endl;

                            locked     = false;
                        }
                        else
                        {
                            cout << "M17 Demodulator: Did not receive any sync word, staying locked anyway" << endl;
                            demodState = DemodState::LOCKED;
                        }

                        missedSyncs += 1;
                    }

                    syncCount -= 1;
                }
                    break;
            }

            sampleIndex  = (sampleIndex + 1) % SAMPLES_PER_SYMBOL;
        }
    }

    return newFrame;
}

int8_t M17Demodulator::updateFrame(int16_t sample)
{
    int8_t symbol;

    if(sample > (2 * outerDeviation.first)/3)
    {
        symbol = +3;
    }
    else if(sample < (2 * outerDeviation.second)/3)
    {
        symbol = -3;
    }
    else if(sample > (outerDeviation.first + outerDeviation.second)/2)
    {
        symbol = +1;
    }
    else
    {
        symbol = -1;
    }

    setSymbol(*demodFrame, frameIndex++, symbol);

    if(frameIndex >= M17_FRAME_SYMBOLS)
    {
        std::swap(readyFrame, demodFrame);
        frameIndex = 0;
        newFrame   = true;
    }

    return symbol;
}

void M17Demodulator::reset()
{
    sampleIndex = 0;
    frameIndex  = 0;
    newFrame    = false;
    locked      = false;
    demodState  = DemodState::INIT;
    initCount   = RX_SAMPLE_RATE / 50;  // 50ms of init time

    iirfilt_rrrf_reset(dcr);
    firfilt_rrrf_reset(rrcos_filt);
}


constexpr std::array < float, 3 > M17Demodulator::sfNum;
constexpr std::array < float, 3 > M17Demodulator::sfDen;
