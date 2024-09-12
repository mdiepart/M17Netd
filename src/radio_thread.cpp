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
                    ConsumerProducerQueue<shared_ptr<m17tx>> &to_radio,
                    ConsumerProducerQueue<shared_ptr<m17rx>> &from_radio)
{
    radio_thread_cfg radio_cfg;
    cfg.getRadioConfig(radio_cfg);
    
    // Initialize frequency modulator / demodulator
    fmod = freqmod_create(radio_cfg.k);
    fdem = freqdem_create(radio_cfg.k);
    // For now, this threads gets packets from the network and display them

    shared_ptr<m17tx> packet;

    // Allocations
    // Allocate the RX samples with fftw so that it is aligned for SIMD
    float *rx_samples = reinterpret_cast<float*>(fftwf_alloc_complex(block_size));
    array<float, 2*block_size>  *tx_samples     = new array<float, 2*block_size>();
    array<float, block_size>    *rx_baseband    = new array<float, block_size>();

    // FFT: we only compute the FFT of the first 512 points
    constexpr size_t fft_N = 512;
    fftwf_complex *rx_samples_fft = reinterpret_cast<fftwf_complex *>(fftwf_alloc_complex(512));
    fftwf_plan fft_plan = fftwf_plan_dft_1d(fft_N, reinterpret_cast<fftwf_complex*>(rx_samples), rx_samples_fft, FFTW_FORWARD, FFTW_MEASURE);
    
    // M17 Demodulator
    M17::M17Demodulator demodulator;
    demodulator.init();

    // Create and initialize the radio
    sdrnode radio = sdrnode(radio_cfg.rx_freq, radio_cfg.tx_freq);
    radio.switch_rx();

    bool channel_bsy = true;
    while(running)
    {
        // While the channel is busy or while there is nothing to send
        // We keep receiving and (attempting to) demodulate
        shared_ptr<m17rx> rx_packet = make_shared<m17rx>();
        while(running && (to_radio.isEmpty() || channel_bsy))
        {
            int read = radio.receive(rx_samples, block_size);

            // Frequency demodulation
            freqdem_demodulate_block(fdem, reinterpret_cast<liquid_float_complex *>(rx_samples), 
                                     read, rx_baseband->data());

            // Use OpenRTX demodulator
            bool new_frame = demodulator.update(rx_baseband->data(), read);

            if(new_frame)
            {
                rx_packet->add_frame(demodulator.getFrame());

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

            if(!demodulator.isLocked())
            {
                fftwf_execute(fft_plan);

                // Re-use rx_samples to store the module of the array
                complex<float> *in = reinterpret_cast<complex<float> *>(rx_samples_fft);
                float *out = reinterpret_cast<float *>(rx_samples);
                for(size_t i = 0; i < 512; i++)
                {
                    out[i] = abs(in[i]);
                }

                float chan = 0, noise = 0;
                for(size_t i = 0; i < 25; i++)
                {
                    chan += out[i];
                }
                for(size_t i = 512-25; i < 512; i++)
                {
                    chan += out[i];
                }

                for(size_t i = 25; i < 512-25; i++)
                {
                    noise += out[i];
                }

                chan /= 50;
                noise /= (512-50);

                // Check if there is more energy in the channel than elsewhere in the spectrum
                if(chan >= 10*noise)
                {
                    // Channel is busy
                    if(!channel_bsy)
                    {
                        cout << "Channel now busy" << endl;
                    }
                    channel_bsy = true;
                }
                else
                {
                    // Channel is free
                    if(channel_bsy)
                    {
                        cout << "Channel now free" << endl;
                    }
                    channel_bsy = false;
                }

            }

            // If demodulator synced, channel is busy. 
            // If demodulator is unsynced, check if channel busy? 
        }

        while(running && (!to_radio.isEmpty()))
        {
            int ret = to_radio.consume(packet);
            if(ret < 0)
                break;

            cout << "Received packet for radio." << endl;
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