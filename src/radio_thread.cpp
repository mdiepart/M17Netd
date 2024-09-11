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

#include <netinet/ip.h>

#include <liquid/liquid.h>
#include <m17.h>

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

    // Block of samples
    array<float, 2*block_size>  *rx_samples     = new array<float, 2*block_size>();
    array<float, 2*block_size>  *tx_samples     = new array<float, 2*block_size>();
    array<float, block_size>    *rx_baseband    = new array<float, block_size>();

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
            int read = radio.receive(rx_samples->data(), rx_samples->size());

            // Frequency demodulation
            freqdem_demodulate_block(fdem, reinterpret_cast<liquid_float_complex *>(rx_samples->data()), 
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

        /*string filename_bb = "./" + to_string(i) + ".csv";
        string filename_sym = "./" + to_string(i) + "_sym.csv";
        ofstream my_file_bb(filename_bb);
        ofstream my_file_sym(filename_sym);
        float t_bb = 0;
        float t_sym = 0;
        constexpr size_t chunk = 1000;

        my_file_bb << "Time(s), Value" << endl;
        my_file_sym << "Time(s), Symbol" << endl;
        while(true)
        {
            vector<float> samples = packet->getBasebandSamples(chunk);
            //cout << "Got " << to_string(samples.size()) << " baseband samples." << endl;

            for(auto const &s: samples)
            {
                my_file_bb << t_bb << ", " << s << endl;
                t_bb += (1.0f/96000.0f);
            }

            if(samples.size() < chunk)
            {
                break;
            }
        }
        for(auto &sym: packet->getSymbols())
        {
            my_file_sym << t_sym << ", " << sym << endl;
            t_sym += (1.0f/4800.0f);
        }
        i++;
        my_file_bb.close();
        my_file_sym.close();*/
    }

    delete[](rx_samples);
    delete[](rx_baseband);
    delete[](tx_samples);

    freqmod_destroy(fmod);
    freqdem_destroy(fdem);
}