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

M17Demodulator::M17Demodulator()
{
    dcr = iirfilt_rrrf_create_dc_blocker(0.0002f);
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

#if M17DEMOD_DEBUG_OUT
    total_cnt = 0;
    post_demod.open("./post_demod.raw", ios_base::binary | ios_base::trunc);
    post_dcr.open("./post_drc.raw", ios_base::binary | ios_base::trunc);
    post_rrcos.open("./post_rrcos.raw", ios_base::binary | ios_base::trunc);
    samp_pts.open("./sampling_points.pts", ios_base::binary | ios_base::trunc);
#endif
}

void M17Demodulator::terminate()
{
    demodFrame.reset();
    readyFrame.reset();

#if M17DEMOD_DEBUG_OUT
    post_demod.close();
    post_dcr.close();
    post_rrcos.close();
    samp_pts.close();
#endif
}

const M17::m17frame_t& M17Demodulator::getFrame()
{
    // When a frame is read is not new anymore
    newFrame = false;
    return *readyFrame;
}

const m17syncw_t M17Demodulator::getFrameSyncWord() const
{
    switch(lastSyncWord)
    {
        case M17Demodulator::SyncWord::LSF:
        {
            return LSF_SYNC_WORD;
        }
        case M17Demodulator::SyncWord::PACKET:
        {
            return PACKET_SYNC_WORD;
        }
        case M17Demodulator::SyncWord::BERT:
        {
            return BERT_SYNC_WORD;
        }
        default:
            return {0, 0};
    }
}

bool M17Demodulator::isLocked() const
{
    return locked;
}

bool M17Demodulator::update(float *samples, const size_t N)
{
    if(samples != nullptr)
    {
#if M17DEMOD_DEBUG_OUT
        post_demod.write(reinterpret_cast<const char*>(samples), N*sizeof(float));
#endif
        // Apply DC removal filter
        iirfilt_rrrf_execute_block(dcr, samples, N, samples);
#if M17DEMOD_DEBUG_OUT
        post_dcr.write(reinterpret_cast<const char*>(samples), N*sizeof(float));
#endif
        // Process samples
        for(size_t i = 0; i < N; i++)
        {
            // Apply RRC on the baseband sample
            float   elem;
            firfilt_rrrf_push(rrcos_filt, samples[i]);
            firfilt_rrrf_execute(rrcos_filt, &elem);
#if M17DEMOD_DEBUG_OUT
            total_cnt++;
            post_rrcos.write(reinterpret_cast<const char *>(&elem), sizeof(float));
#endif
            int16_t sample = static_cast<int16_t>(elem*500); // Scale up

            // Update correlator and sample filter for correlation thresholds
            correlator.sample(sample);
            corrThreshold = sampleFilter(std::abs(sample));
            int32_t syncThresh = static_cast< int32_t >(corrThreshold * 32.0f);

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
                    int8_t  lsfSyncStatus = lsfSync.update(correlator, syncThresh, -syncThresh);

                    if(lsfSyncStatus == 1)
                    {
                        demodState = DemodState::SYNCED;
                    }
                }
                    break;

                case DemodState::SYNCED:
                {
                    // Set sampling point and deviation, zero frame symbol count
                    samplingPoint = lsfSync.samplingIndex();
                    outerDeviation = correlator.maxDeviation(samplingPoint);
                    int32_t devSpacing = (outerDeviation.first-outerDeviation.second)/3;
                    innerDeviation.first = outerDeviation.first-devSpacing; // Deviation for +1
                    innerDeviation.second = outerDeviation.second + devSpacing; // deviation for -1

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

                    float hd  = softHammingDistance( 16, demodFrame->data(), SOFT_LSF_SYNC_WORD.data());
                    if(hd <= 1)
                    {
                        locked     = true;
                        demodState = DemodState::LOCKED;
                        lastSyncWord = SyncWord::LSF;
                        //cout << "M17Demodulator: Received LSF sync: Synced -> Locked" << endl;
                    }
                    else
                    {
                        demodState = DemodState::UNLOCKED;
                        //cout << "M17Demodulator: LSF sync not recognized. hd=" << static_cast<uint16_t>(hd) << ", Synced -> Unlocked" << endl;
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
                        if(frameIndex == (2*M17_FRAME_SYMBOLS - M17_SYNCWORD_SYMBOLS))
                        {
                            demodState = DemodState::SYNC_UPDATE;
                            syncCount  = SYNCWORD_SAMPLES * 2;
                            //cout << "M17 Demodulator: Locked -> Sync Update" << endl;
                        }
                    }
                }
                    break;

                case DemodState::SYNC_UPDATE:
                {
                    // Keep filling the ongoing frame!
                    if(sampleIndex == samplingPoint)
                        updateFrame(sample);

                    // Correlation has to coincide with a syncword!
                    if(frameIndex == M17_SYNCWORD_SYMBOLS*2)
                    {
                        // Find the new correlation peak
                        int8_t  packetSyncStatus = packetSync.update(correlator, syncThresh, -syncThresh);
                        int8_t  eotSyncStatus = EOTSync.update(correlator, 0.87*syncThresh, -0.87*syncThresh);

                        if(packetSyncStatus == 1)
                        {
                            float hd  = softHammingDistance(16, demodFrame->data(), SOFT_PACKET_SYNC_WORD.data());

                            // Valid sync found: update deviation and sample
                            // point, then go back to locked state
                            if(hd <= 1)
                            {
                                outerDeviation = correlator.maxDeviation(samplingPoint);
                                int32_t devSpacing = (outerDeviation.first-outerDeviation.second)/3;
                                innerDeviation.first = outerDeviation.first-devSpacing; // Deviation for +1
                                innerDeviation.second = outerDeviation.second + devSpacing; // deviation for -1
                                samplingPoint  = packetSync.samplingIndex();
                                missedSyncs    = 0;
                                demodState     = DemodState::LOCKED;
                                lastSyncWord   = SyncWord::PACKET;
                                //cout << "M17 Demodulator: Received packet sync: Sync Update -> Locked" << endl;
                                break;
                            }
                        }
                        else if(eotSyncStatus == 1)
                        {
                            float hd  = softHammingDistance(16, demodFrame->data(), SOFT_EOT_SYNC_WORD.data());

                            // Valid EOT sync found: unlock demodulator
                            if(hd <= 1)
                            {
                                missedSyncs = 0;
                                demodState     = DemodState::UNLOCKED;
                                locked = false;
                                lastSyncWord = SyncWord::NONE;
                                cout << "M17Demodulator: Received EOT sync: -> Unlocked" << endl;
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
                            //cout << "M17 Demodulator: Missed too many syncs: Sync Update -> Unlocked" << endl;

                            locked     = false;
                        }
                        else
                        {
                            //cout << "M17 Demodulator: Did not receive any sync word, staying locked anyway" << endl;
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

void M17Demodulator::updateFrame(int16_t sample)
{
    /**
     * Dibit    Symbol
     * M   L
     * 0   1    +3
     * 0   0    +1
     * 1   0    -1
     * 1   1    -3
     *
     * We map the value of each bit with a linear interpolation as such:
     *
     */

    if(sample >= outerDeviation.first)
    {
        // Sample >= +3
        demodFrame->at(frameIndex++) = 0x0000; // MSB
        demodFrame->at(frameIndex++) = 0xFFFF; // LSB
    }
    else if(sample >= innerDeviation.first)
    {
        // +3 > Sample >= +1
        demodFrame->at(frameIndex++) = 0x0000; // MSB
        demodFrame->at(frameIndex++) = mapRange(sample, innerDeviation.first, outerDeviation.first, 0, 0xFFFF); // LSB
    }
    else if(sample >= innerDeviation.second)
    {
        // +1 > sample >= -1
        demodFrame->at(frameIndex++) = mapRange(sample, innerDeviation.second, innerDeviation.first, 0xFFFF, 0); // MSB
        demodFrame->at(frameIndex++) = 0x0000; // LSB
    }
    else if(sample > outerDeviation.second)
    {
        // -1 > sample > -3
        demodFrame->at(frameIndex++) = 0xFFFF; // MSB
        demodFrame->at(frameIndex++) = mapRange(sample, outerDeviation.second, innerDeviation.second, 0xFFFF, 0); // LSB
    }
    else
    {
        // sample <= -3
        demodFrame->at(frameIndex++) = 0xFFFF; // MSB
        demodFrame->at(frameIndex++) = 0xFFFF; // LSB
    }

#if M17DEMOD_DEBUG_OUT
    float tmp = total_cnt;
    float tmp2 = sample;
    samp_pts.write(reinterpret_cast<const char*>(&tmp), 4);
    samp_pts.write(reinterpret_cast<const char*>(&tmp2), 4);
#endif

    if(frameIndex >= 2*M17_FRAME_SYMBOLS)
    {
        std::swap(readyFrame, demodFrame);
        frameIndex = 0;
        newFrame   = true;
        //cout << "M17Demodulator: completed a frame" << endl;
    }
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
