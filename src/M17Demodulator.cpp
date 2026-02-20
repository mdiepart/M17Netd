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
#include <algorithm>

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
    post_rrcos.open("./post_rrcos.raw", ios_base::binary | ios_base::trunc);
    samp_pts.open("./sampling_points.pts", ios_base::binary | ios_base::trunc);
    corr_thresh.open("./correlation_threshold.pts", ios_base::binary | ios_base::trunc);
    lsf_corr.open("./lsf_correlation.pts", ios_base::binary | ios_base::trunc);
    pkt_corr.open("./pkt_correlation.pts", ios_base::binary | ios_base::trunc);
    eot_corr.open("./eot_correlation.pts", ios_base::binary | ios_base::trunc);
    sync_thresh.open("./sync_thresh.pts", ios_base::binary | ios_base::trunc);
    dev_p3.open("./dev_p3.pts", ios_base::binary | ios_base::trunc);
    dev_p1.open("./dev_p1.pts", ios_base::binary | ios_base::trunc);
    dev_n1.open("./dev_n1.pts", ios_base::binary | ios_base::trunc);
    dev_n3.open("./dev_n3.pts", ios_base::binary | ios_base::trunc);
#endif
}

void M17Demodulator::terminate()
{
    demodFrame.reset();
    readyFrame.reset();

#if M17DEMOD_DEBUG_OUT
    post_demod.close();
    post_rrcos.close();
    samp_pts.close();
    corr_thresh.close();
    lsf_corr.close();
    pkt_corr.close();
    eot_corr.close();
    sync_thresh.close();
    dev_p3.close();
    dev_p1.close();
    dev_n1.close();
    dev_n3.close();
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

int M17Demodulator::update(float *samples, const size_t N)
{
    if(samples != nullptr)
    {
#if M17DEMOD_DEBUG_OUT
        post_demod.write(reinterpret_cast<const char*>(samples), N*sizeof(float));
#endif

        if(demodState == DemodState::UNLOCKED)
        {
            lastSyncWord = SyncWord::NONE;
        }

        // Apply RRC on the baseband sample
        firfilt_rrrf_execute_block(rrcos_filt, samples, N, samples);
#if M17DEMOD_DEBUG_OUT
        post_rrcos.write(reinterpret_cast<const char *>(&elem), sizeof(float));
#endif
        // Process samples
        for(size_t i = 0; i < N; i++)
        {
#if M17DEMOD_DEBUG_OUT
            total_cnt++;
#endif
            int16_t sample = static_cast<int16_t>(samples[i]*500); // Scale up

            // Update correlator and sample filter for correlation thresholds
            correlator.sample(sample);
            int32_t syncThresh = 280000;

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
                    static int waiting = 2500;
                    lsfSync.update(correlator, syncThresh, -syncThresh);
                    packetSync.update(correlator, syncThresh, -syncThresh);

#if M17DEMOD_DEBUG_OUT
                    // Write corr value to file
                    float lsf_corr_val = static_cast<float>(lsfSync.getLastCorr())/100000;
                    float pkt_corr_val = static_cast<float>(packetSync.getLastCorr())/100000;
                    float st = static_cast<float>(syncThresh)/100000;
                    float tmp = total_cnt;
                    lsf_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    lsf_corr.write(reinterpret_cast<const char*>(&lsf_corr_val), 4);
                    pkt_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    pkt_corr.write(reinterpret_cast<const char*>(&pkt_corr_val), 4);
                    sync_thresh.write(reinterpret_cast<const char*>(&tmp), 4);
                    sync_thresh.write(reinterpret_cast<const char*>(&st), 4);
#endif
                    if( abs(lsfSync.getLastCorr()) < 90000 )
                        waiting--;
                    else
                        waiting = 2500;

                    if(waiting <= 0)
                    {
                        demodState = DemodState::ARMED;
                        waiting = 2500;
                    }


                }
                    break;
                case DemodState::ARMED:
                {
                    int8_t lsfSyncStatus = lsfSync.update(correlator, syncThresh, -syncThresh);
                    int8_t bertSyncStatus = -packetSync.update(correlator, syncThresh, -syncThresh);

#if M17DEMOD_DEBUG_OUT
                    // Write corr value to file
                    float lsf_corr_val = static_cast<float>(lsfSync.getLastCorr())/100000;
                    float pkt_corr_val = static_cast<float>(packetSync.getLastCorr())/100000;
                    float st = static_cast<float>(syncThresh)/10000;
                    float tmp = total_cnt;
                    lsf_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    lsf_corr.write(reinterpret_cast<const char*>(&lsf_corr_val), 4);
                    pkt_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    pkt_corr.write(reinterpret_cast<const char*>(&pkt_corr_val), 4);
                    sync_thresh.write(reinterpret_cast<const char*>(&tmp), 4);
                    sync_thresh.write(reinterpret_cast<const char*>(&st), 4);
#endif

                    if(lsfSyncStatus == 1)
                    {
                        //cout << "Found LSF. Unlocked -> Synced" << endl;
                        lastSyncWord = SyncWord::LSF;
                        demodState = DemodState::SYNCED;
                    }
                    else if(bertSyncStatus == 1)
                    {
                        lastSyncWord = SyncWord::BERT;
                        demodState = DemodState::SYNCED;
                    }
                }
                    break;

                case DemodState::SYNCED:
                {
                    // Set sampling point and deviation, zero frame symbol count
                    size_t peak = 0;
                    if(lastSyncWord == SyncWord::LSF)
                    {
                        peak = lsfSync.samplingIndex();

                    }
                    else if(lastSyncWord == SyncWord::BERT)
                    {
                        peak = packetSync.samplingIndex();
                    }
                    else
                    {
                        cerr << "Unknown lastSyncWord" << endl;
                    }

                    outerDeviation = correlator.maxDeviation(peak);
                    int32_t devSpacing = (outerDeviation.first-outerDeviation.second)/3;
                    innerDeviation.first = outerDeviation.first - devSpacing; // Deviation for +1
                    innerDeviation.second = outerDeviation.second + devSpacing; // deviation for -1
                    frameIndex = 0;

                    // correlator.index() is the index where the last sample was written in correlator memory
                    // samplingPoint is the index where the peak correlation occured
                    size_t shift = (correlator.index() + correlator.bufferSize() - peak) % correlator.bufferSize(); // how many samples ago was the peak found

#if M17DEMOD_DEBUG_OUT
                    size_t tmp = total_cnt; // Save current total_cnt
                    total_cnt -= (SYNCWORD_SAMPLES + shift - SAMPLES_PER_SYMBOL);
#endif

                    // Quantize the syncword taking data from the correlator memory.
                    for(ssize_t i = -(SYNCWORD_SAMPLES-SAMPLES_PER_SYMBOL); i <= 0; i += SAMPLES_PER_SYMBOL)
                    {
                        ssize_t  pos = (peak + correlator.bufferSize() + i) % correlator.bufferSize();

                        int16_t val = correlator.data()[pos];

                        updateFrame(val);

#if M17DEMOD_DEBUG_OUT
                        total_cnt += SAMPLES_PER_SYMBOL;
#endif
                    }

#if M17DEMOD_DEBUG_OUT
                    total_cnt = tmp;
#endif

                    float hd;
                    if(lastSyncWord == SyncWord::LSF)
                    {
                        hd = softHammingDistance( 16, demodFrame->data(), SOFT_LSF_SYNC_WORD.data());
                    }
                    else if(lastSyncWord == SyncWord::BERT)
                    {
                        hd = softHammingDistance( 16, demodFrame->data(), SOFT_BERT_SYNC_WORD.data());
                    }
                    else
                    {
                        cerr << "Unknown lastSyncWord" << endl;
                        hd = +INFINITY;
                    }

                    if(hd <= 1)
                    {
                        locked     = true;
                        demodState = DemodState::LOCKED;
                        sampleIndex = shift;
                        cout << "M17Demodulator: Received " << ((lastSyncWord == SyncWord::LSF)? "LSF":"BERT") << " sync with hd=" << hd << ": Synced -> Locked" << endl;
                    }
                    else
                    {
                        demodState = DemodState::UNLOCKED;
                        //cout << "M17Demodulator: LSF sync not recognized. hd=" << hd << ", Synced -> Unlocked" << endl;
                    }
                }
                    break;

                case DemodState::LOCKED:
                {
                    // Quantize and update frame at each sampling point
                    if(sampleIndex == 0)
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
                    if(sampleIndex == 0)
                        updateFrame(sample);

                    int8_t  packetSyncStatus = packetSync.update(correlator, syncThresh, -syncThresh);
                    int8_t  eotSyncStatus = EOTSync.update(correlator, syncThresh, syncThresh);

#if M17DEMOD_DEBUG_OUT
                    // Write corr value to file
                    float pkt = static_cast<float>(packetSync.getLastCorr())/100000;
                    float eot = static_cast<float>(EOTSync.getLastCorr())/100000;
                    float tmp = total_cnt;
                    pkt_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    pkt_corr.write(reinterpret_cast<const char*>(&pkt), 4);
                    eot_corr.write(reinterpret_cast<const char*>(&tmp), 4);
                    eot_corr.write(reinterpret_cast<const char*>(&eot), 4);
#endif

                    // Correlation has to coincide with a syncword!
                    if(frameIndex == M17_SYNCWORD_SYMBOLS*2)
                    {
                        // Find the new correlation peak

                        if(packetSyncStatus == 1)
                        {
                            float hd  = softHammingDistance(16, demodFrame->data(), SOFT_PACKET_SYNC_WORD.data());

                            // Valid sync found: update deviation and sample
                            // point, then go back to locked state
                            if(hd <= 1.7)
                            {
                                size_t pkt_peak = packetSync.samplingIndex();
                                outerDeviation = correlator.maxDeviation(pkt_peak);
                                int32_t devSpacing = (outerDeviation.first-outerDeviation.second)/3;
                                innerDeviation.first = outerDeviation.first-devSpacing; // Deviation for +1
                                innerDeviation.second = outerDeviation.second + devSpacing; // deviation for -1
                                cout << "pkt_peak=" << pkt_peak << ", correlator.index()=" << correlator.index() << endl;
                                sampleIndex = (correlator.index() - pkt_peak);

                                if(sampleIndex > correlator.bufferSize())
                                    sampleIndex += correlator.bufferSize();

                                missedSyncs    = 0;
                                demodState     = DemodState::LOCKED;
                                lastSyncWord   = SyncWord::PACKET;
                                //cout << "M17 Demodulator: Received packet sync: Sync Update -> Locked" << endl;
                                break;
                            }
                        }
                        else if(packetSyncStatus == -1)
                        {
                            float hd  = softHammingDistance(16, demodFrame->data(), SOFT_BERT_SYNC_WORD.data());

                            // Valid sync found: update deviation and sample
                            // point, then go back to locked state
                            if(hd <= 1.7)
                            {
                                size_t bert_peak = packetSync.samplingIndex();
                                outerDeviation = correlator.maxDeviation(bert_peak);
                                int32_t devSpacing = (outerDeviation.first-outerDeviation.second)/3;
                                innerDeviation.first = outerDeviation.first-devSpacing; // Deviation for +1
                                innerDeviation.second = outerDeviation.second + devSpacing; // deviation for -1
                                //cout << "bert_peak=" << bert_peak << ", correlator.index()=" << correlator.index() << endl;
                                sampleIndex = (correlator.index() - bert_peak);

                                if(sampleIndex > correlator.bufferSize())
                                    sampleIndex += correlator.bufferSize();

                                missedSyncs    = 0;
                                demodState     = DemodState::LOCKED;
                                lastSyncWord   = SyncWord::BERT;
                                //cout << "M17 Demodulator: Received bert sync: Sync Update -> Locked" << endl;
                                break;
                            }
                        }
                        else if(eotSyncStatus == 1)
                        {
                            float hd  = softHammingDistance(16, demodFrame->data(), SOFT_EOT_SYNC_WORD.data());

                            // Valid EOT sync found: unlock demodulator
                            if(hd <= 1.7)
                            {
                                missedSyncs = 0;
                                demodState     = DemodState::UNLOCKED;
                                locked = false;
                                lastSyncWord = SyncWord::EOT;
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
                            //cout << "M17 Demodulator: Did not receive any sync word, staying locked anyway." << endl;
                            // Checking which sync word is the most probable
                            float hd_lsf = softHammingDistance(16, demodFrame->data(), SOFT_LSF_SYNC_WORD.data());
                            float hd_pkt = softHammingDistance(16, demodFrame->data(), SOFT_PACKET_SYNC_WORD.data());
                            float hd_bert = softHammingDistance(16, demodFrame->data(), SOFT_BERT_SYNC_WORD.data());
                            float hd_eot = softHammingDistance(16, demodFrame->data(), SOFT_EOT_SYNC_WORD.data());

                            float hd_min = min({hd_lsf, hd_pkt, hd_bert, hd_eot});

                            demodState = DemodState::LOCKED;

                            if( hd_min == hd_lsf)
                                lastSyncWord = SyncWord::LSF;
                            else if( hd_min == hd_pkt )
                                lastSyncWord = SyncWord::PACKET;
                            else if( hd_min == hd_bert )
                                lastSyncWord = SyncWord::BERT;
                            else if( hd_min == hd_eot)
                            {
                                lastSyncWord = SyncWord::EOT;
                                demodState = DemodState::UNLOCKED;
                                locked = false;
                            }
                            else
                                lastSyncWord = SyncWord::NONE;
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

    return (lastSyncWord==SyncWord::EOT)?-1:newFrame;
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
    float tmp2 = static_cast<float>(sample)/500;
    float p3 = static_cast<float>(outerDeviation.first)/500,
          p1 = static_cast<float>(innerDeviation.first)/500,
          n1 = static_cast<float>(innerDeviation.second)/500,
          n3 = static_cast<float>(outerDeviation.second)/500;

    samp_pts.write(reinterpret_cast<const char*>(&tmp), 4);
    samp_pts.write(reinterpret_cast<const char*>(&tmp2), 4);
    dev_p3.write(reinterpret_cast<const char*>(&tmp), 4);
    dev_p3.write(reinterpret_cast<const char*>(&p3), 4);
    dev_p1.write(reinterpret_cast<const char*>(&tmp), 4);
    dev_p1.write(reinterpret_cast<const char*>(&p1), 4);
    dev_n1.write(reinterpret_cast<const char*>(&tmp), 4);
    dev_n1.write(reinterpret_cast<const char*>(&n1), 4);
    dev_n3.write(reinterpret_cast<const char*>(&tmp), 4);
    dev_n3.write(reinterpret_cast<const char*>(&n3), 4);
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