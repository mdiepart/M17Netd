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

#include <complex>
#include <iostream>
#include <fstream>
#include <csignal>
#include <cstring>
#include <ctime>

#include "liquid/liquid.h"

#include "m17rx.h"
#include "M17Demodulator.hpp"
#include "sdrnode.h"

using namespace std;
using namespace M17;

volatile bool running = true;

void sigint_catcher(int signum)
{
    if(signum == SIGINT)
    {
        std::cout << "Ctrl-C caught, stopping all threads." << std::endl;
        running = false;
    }
}

int main(int argc, char *argv[])
{
    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, 0);

    // Parse first argument (number of samples to acquire)
    if(argc == 2 && strcmp(argv[1], "help") == 0)
    {
        cout << "Usage: " << argv[0] << " rx_frequency ppm_correction rx_gain kf\n"
             << "\trx_frequency        is the frequency at which to do the acquisition (in Hz).\n"
             << "\tppm_correction      is the correction to apply to the frequency (in ppm, as an integer).\n"
             << "\trx_gain             is the LNA gain (must be one of {0, -6, -12, -24, -36, -48})\n."
             << "\tkf                  is the modulation index to use for the frequency demoduation."
             << endl;
        return EXIT_SUCCESS;
    }else if(argc != 5)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }

    unsigned long rx_frequency;
    int ppm;
    int rx_gain;
    float kf;

    // Parse RX frequency
    try
    {
        rx_frequency = stoul(argv[1]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid RX frequency: \"" << argv[1] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Parse PPM
    try
    {
        ppm = stoi(argv[2]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid ppm correction: \"" << argv[2] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Parse RX gain
    try
    {
        rx_gain = stoi(argv[3]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid rx_gain: \"" << argv[3] << "\"." << endl;
        return EXIT_FAILURE;
    }
    sx1255_drv::lna_gain lna_rx_gain;

    switch(rx_gain)
    {
        case 0:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX;
            break;
        case -6:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX_min6;
            break;
        case -12:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX_min12;
            break;
        case -24:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX_min24;
            break;
        case -36:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX_min36;
            break;
        case -48:
            lna_rx_gain = sx1255_drv::LNA_GAIN_MAX_min48;
            break;
        default:
            cerr << "rx_gain of " << rx_gain << " is not in not one of {0, -6, -12, -24, -36, -48}." << endl;
            return EXIT_FAILURE;
    }

    // Parse modulation index
    try
    {
        kf = stof(argv[4]);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Invalid modulation index: \"" << argv[4] << "\"." << endl;
        return EXIT_FAILURE;
    }

    if(kf <= 0)
    {
        cerr << "Invalid modulation index: kf=" << kf << endl;
        return EXIT_FAILURE;
    }

    sdrnode radio = sdrnode(rx_frequency, rx_frequency, ppm);
    radio.set_rx_gain(lna_rx_gain);
    radio.switch_rx();

    const size_t block_size = 128;

    complex<float> buffer[block_size] = {0};
    float baseband[block_size] = {0};

    M17Demodulator m17demod;
    m17demod.init();

    freqdem fdem = freqdem_create(kf);

    m17rx bert_rx;
    size_t counter = 0;
    size_t frame_counter = 0;
    size_t last_bert_errcnt = 0;
    time_t start_time = 0;

    while( running )
    {
        size_t read = radio.receive(buffer, block_size);

        freqdem_demodulate_block(fdem, buffer, read, baseband);

        int new_frame = m17demod.update(baseband, read);

        if(new_frame == 1)
        {
            m17syncw_t sw = m17demod.getFrameSyncWord();
            uint16_t sw_packed = (static_cast<uint16_t>(sw[0]) << 8) + sw[1];

            if(sw_packed != SYNC_BER)
            {
                cerr << "Received unexpected non-BERT syncword." << endl;
                continue;
            }

            if(frame_counter == 0)
            {
                start_time = time(nullptr);
                cout << "first frame received at " << asctime(localtime(&start_time)) << endl;
            }

            m17frame_t frame = m17demod.getFrame();
            bert_rx.add_frame(sw_packed, frame);

            if(bert_rx.get_bert_errcnt() != last_bert_errcnt)
            {
                size_t errs = bert_rx.get_bert_errcnt() - last_bert_errcnt;
                cout << "frame no " << frame_counter << " contained " << errs << " incorrect bits." << endl;
                last_bert_errcnt = bert_rx.get_bert_errcnt();
            }
            if(counter >= 25)
            {
                cout << "BERT status: " << bert_rx.get_bert_errcnt() << " errors over " << bert_rx.get_bert_totcnt() << " total received bits (BER=" <<
                    (bert_rx.get_bert_errcnt()*100.0f) / (bert_rx.get_bert_totcnt() * 1.0f) << "%)." << endl;
                counter = 0;
            }
            counter++;
            frame_counter++;

        }
        else if(new_frame == -1)
        {
            cout << "Received EOT." << endl;
            break;
        }
    }

    if(start_time != 0)
    {
           time_t end_time = time(nullptr);
           time_t diff = end_time - start_time;
           cout << argv[0] << " stopped at " << asctime(localtime(&end_time)) << "and ran for " << diff << " seconds." << endl;
    }
    else{
        cout << argv[0] << " did not receive any BERT frame." << endl;
    }


    return EXIT_SUCCESS;
}
