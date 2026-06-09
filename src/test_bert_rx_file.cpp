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

#include "liquid/liquid.h"

#include "m17rx.h"
#include "M17Demodulator.hpp"

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
             << "\tfile                is the path to the IQ file with the filtered acquisition\n."
             << "\tkf                  is the modulation index to use for the frequency demoduation."
             << endl;
        return EXIT_SUCCESS;
    }else if(argc != 3)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }

    float kf;
    ifstream iq_in_file;

    // Try to open the output file
    try
    {
        iq_in_file.open(argv[1], ios_base::binary);
    }
    catch(const std::exception &e)
    {
        cerr << "Unable to open output file \"" << argv[1] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Parse modulation index
    try
    {
        kf = stof(argv[2]);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Invalid modulation index: \"" << argv[2] << "\"." << endl;
        return EXIT_FAILURE;
    }

    if(kf <= 0)
    {
        cerr << "Invalid modulation index: kf=" << kf << endl;
        return EXIT_FAILURE;
    }

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

    while( running && !iq_in_file.eof())
    {
        iq_in_file.read(reinterpret_cast<char *>(buffer), block_size*sizeof(complex<float>));

        size_t n = iq_in_file.gcount()/sizeof(complex<float>);

        freqdem_demodulate_block(fdem, buffer, n, baseband);

        int new_frame = m17demod.update(baseband, n);

        if(new_frame == 1)
        {
            m17syncw_t sw = m17demod.getFrameSyncWord();
            uint16_t sw_packed = (static_cast<uint16_t>(sw[0]) << 8) + sw[1];

            if(sw_packed != SYNC_BER)
            {
                cout << "Received unexpected non-BERT syncword (" << sw_packed << ")" << endl;
                continue;
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

    return EXIT_SUCCESS;
}
