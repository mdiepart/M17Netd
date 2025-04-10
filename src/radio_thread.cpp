/****************************************************************************
 * M17Netd                                                                  *
 * Copyright (C) 2024 by Morgan Diepart ON4MOD                              *
 *                       SDR-Engineering SRL                                *
 *                                                                          *
 * This program is free software: you can redistribute it and/or modify     *
 * it under the terms of the GNU Affero General Public License as published *
 * by the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                      *
 *                                                                          *
 * This program is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 * GNU Affero General Public License for more details.                      *
 *                                                                          *
 * You should have received a copy of the GNU Affero General Public License *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ****************************************************************************/

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <complex>

#include <netinet/ip.h>

#include <liquid/liquid.h>
#include <m17.h>
#include <fftw3.h>

#include "ConsumerProducer.h"
#include "sdrnode.h"
#include "M17Demodulator.hpp"
#include "m17rx.h"
#include "m17tx.h"
#include "radio_thread.h"
#include "config.h"

using namespace std;



void radio_simplex::operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<m17tx_pkt>> &to_radio,
                    ConsumerProducerQueue<shared_ptr<m17rx>> &from_radio)
{
    radio_thread_cfg radio_cfg;
    cfg.getRadioConfig(radio_cfg);

    if(radio_cfg.device.compare("sdrnode") != 0)
    {
        cerr << "Unsupported radio device: " << radio_cfg.device << endl;
        return;
    }

    // Read configuration for SDRNode
    sdrnode_cfg sdrnode_cfg;
    cfg.getSDRNodeConfig(sdrnode_cfg);

    // Initialize frequency modulator / demodulator
    fmod = freqmod_create(radio_cfg.k);
    fdem = freqdem_create(radio_cfg.k);
    // For now, this threads gets packets from the network and display them

    shared_ptr<m17tx_pkt> packet;

    // Allocations
    // Allocate the RX samples with fftw so that it is aligned for SIMD
    complex<float>                      *rx_samples     = reinterpret_cast<complex<float>*>(fftwf_alloc_complex(block_size));
    array<complex<float>, block_size>   *tx_samples     = new array<complex<float>, block_size>();
    array<float, block_size>            *rx_baseband    = new array<float, block_size>();

    // FFT: we only compute the FFT of the first 512 points
    complex<float> *rx_samples_fft = reinterpret_cast<complex<float>*>(fftwf_alloc_complex(fft_size));
    fftwf_plan fft_plan = fftwf_plan_dft_1d(fft_size, reinterpret_cast<fftwf_complex*>(rx_samples), reinterpret_cast<fftwf_complex*>(rx_samples_fft), FFTW_FORWARD, FFTW_MEASURE);

    // M17 Demodulator
    M17::M17Demodulator demodulator;
    demodulator.init();

    // Create and initialize the radio
    sdrnode radio = sdrnode(radio_cfg.rx_freq, radio_cfg.tx_freq, radio_cfg.ppm);
    radio.set_rx_gain(sdrnode_cfg.lna_gain);
    radio.set_tx_gain(sdrnode_cfg.mix_gain);

    bool channel_bsy = true;
    while(running)
    {
        // While the channel is busy or while there is nothing to send
        // We keep receiving and (attempting to) demodulate
        shared_ptr<m17rx> rx_packet = make_shared<m17rx>();
        radio.switch_rx();

        while(running && (to_radio.isEmpty() || channel_bsy))
        {
            int read = radio.receive(rx_samples, block_size);

            // Frequency demodulation
            freqdem_demodulate_block(fdem, reinterpret_cast<liquid_float_complex *>(rx_samples),
                                     read, rx_baseband->data());

            // Use OpenRTX demodulator
            int new_frame = demodulator.update(rx_baseband->data(), read);

            if(new_frame == 1)
            {
                array<uint16_t, 2*SYM_PER_FRA> frame = demodulator.getFrame();
                array<uint8_t, 2> sync_word = demodulator.getFrameSyncWord();
                uint16_t sync_word_packed = (static_cast<uint16_t>(sync_word[0]) << 8) + sync_word[1];

                rx_packet->add_frame(sync_word_packed, frame);

                if(rx_packet->is_error())
                {
                    // If the packet is in error state, discard it
                    rx_packet = make_shared<m17rx>();
                }
                else if(rx_packet->is_complete())
                {
                    // If this frame completes the packet, push it to the output queue
                    from_radio.add(rx_packet);
                    rx_packet = make_shared<m17rx>();
                }
            }
            else if(new_frame == -1)
            {
                rx_packet = make_shared<m17rx>();
            }

            if(!demodulator.isLocked())
            {
                fftwf_execute(fft_plan);

                // Re-use rx_samples to store the module of the array
                complex<float> *in = reinterpret_cast<complex<float> *>(rx_samples_fft);
                float *out = reinterpret_cast<float *>(rx_samples);
                for(size_t i = 0; i < fft_size; i++)
                {
                    out[i] = abs(in[i]);
                }

                // measure the energy inside the channel, avoiding the DC component
                float chan = 0, noise = 0;
                for(size_t i = 1; i < half_chan_width; i++)
                {
                    chan += out[i];
                }
                for(size_t i = fft_size-half_chan_width; i < fft_size; i++)
                {
                    chan += out[i];
                }

                for(size_t i = half_chan_width; i < fft_size-half_chan_width; i++)
                {
                    noise += out[i];
                }

                chan /= 2*half_chan_width-1;
                noise /= (fft_size-2*half_chan_width);

                // Check if there is more energy in the channel than elsewhere in the spectrum
                if( (chan >= 5*noise) && !channel_bsy )
                {
                    // Channel is busy
                    cout << "Channel now busy (chan=" << chan << ", noise=" << noise << ", ratio=" << chan/noise << ")" << endl;
                    channel_bsy = true;

                }
                else if( (chan < 5*noise) && channel_bsy)
                {
                    // Channel is free
                    cout << "Channel now free (chan=" << chan << ", noise=" << noise << ", ratio=" << chan/noise << ")" << endl;
                    channel_bsy = false;
                }
                /*else
                {
                    static size_t counter = 0;
                    if(counter >= 2000)
                    {
                        cout << "channel is " << (channel_bsy?"busy":"free") << ": chan=" << chan << ", noise=" << noise << ", ratio=" << chan/noise << endl;
                        counter = 0;
                    }
                    counter++;
                }*/ // Uncomment to display channel status every 2000 iters
            }
        }

        if(running)
            radio.switch_tx();

        while(running && (!to_radio.isEmpty()))
        {
            int ret = to_radio.consume(packet);
            if(ret < 0)
                break;

            cout << "Fetched packet for radio." << endl;
            do
            {
                vector<float> tx_baseband = packet->get_baseband_samples(block_size);
                freqmod_modulate_block(fmod, tx_baseband.data(), tx_baseband.size(), reinterpret_cast<liquid_float_complex*>(tx_samples->data()));
                radio.transmit(tx_samples->data(), tx_samples->size());
            }
            while(running && (packet->baseband_samples_left() > 0));
        }
    }


    fftwf_destroy_plan(fft_plan);
    fftwf_free(rx_samples);
    fftwf_free(rx_samples_fft);
    fftwf_cleanup();
    delete(rx_baseband);
    delete(tx_samples);

    freqmod_destroy(fmod);
    freqdem_destroy(fdem);
}
