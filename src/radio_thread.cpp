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

#include "ConsumerProducer.h"

#include "radio_thread.h"
#include "config.h"

using namespace std;

void radio_simplex::operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<m17tx>> &to_radio,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_radio)
{
    radio_thread_cfg radio_cfg;
    cfg.getRadioConfig(radio_cfg);
    
    // Initialize frequency modulator / demodulator
    fmod = freqmod_create(radio_cfg.k);
    fdem = freqdem_create(radio_cfg.k);
    // For now, this threads gets packets from the network and display them

    shared_ptr<m17tx> packet;
    //size_t i = 0;
    while(running)
    {
        int ret = to_radio.consume(packet);
        if(ret < 0)
        {
            // Timed out
            continue;
        }

        std::cout << "Received packet for radio." << std::endl;
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

    freqmod_destroy(fmod);
    freqdem_destroy(fdem);
}