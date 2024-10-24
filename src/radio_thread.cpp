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

    shared_ptr<m17tx> packet;

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
            bool new_frame = demodulator.update(rx_baseband->data(), read);

            if(new_frame)
            {
                array<uint8_t, 48> frame = demodulator.getFrame();
                array<uint16_t, 2*SYM_PER_FRA> soft_bits_frame = {0};

                // Inflate frame from hard bits to soft bits
                for(size_t i = 0; i < SYM_PER_FRA; i++)
                {
                    size_t byte = i/4;
                    size_t bits = 6-2*(i%4);

                    if((frame[byte] >> (bits+1)) & 1)
                        soft_bits_frame[2*i] = 0xFFFF;

                    if((frame[byte] >> bits) & 1)
                        soft_bits_frame[2*i+1] = 0xFFFF;
                }

                rx_packet->add_frame(soft_bits_frame);

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
                if( (chan >= 6*noise) && !channel_bsy )
                {
                    // Channel is busy
                    cout << "Channel now busy (chan=" << chan << ", noise=" << noise << ", ratio=" << chan/noise << ")" << endl;
                    channel_bsy = true;

                }
                else if( (chan < 6*noise) && channel_bsy)
                {
                    // Channel is free
                    cout << "Channel now free (chan=" << chan << ", noise=" << noise << ", ratio=" << chan/noise << ")" << endl;
                    channel_bsy = false;
                }

            }
        }

        if(running)
            radio.switch_tx();

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
        usleep(5000);
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
