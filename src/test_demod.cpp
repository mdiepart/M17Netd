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

#include <cmath>
#include <vector>
#include <complex>
#include <iostream>
#include <array>
#include <csignal>
#include <fstream>
#include "M17Demodulator.hpp"
#include <cstring>
#include <liquid/liquid.h>
#include "m17rx.h"

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
        cout << "Usage: " << argv[0] << " kf iq_in_file\n"
             << "\tkf           is frequency modulation index.\n"
             << "\tiq_in_file   is the file from which to read the samples to feed to the demodulator.\n"
             << endl;
        return EXIT_SUCCESS;
    }
    else if(argc != 3)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }

    ifstream iq_in_file;
    float kf = 0;

    // Try to parse the fm index
    try
    {
        kf = stof(argv[1]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid frequency modulation index \"" << argv[1] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Try to open the output file
    try
    {
        iq_in_file.open(argv[2], ios_base::binary);
    }
    catch(const std::exception &e)
    {
        cerr << "Unable to open output file \"" << argv[2] << "\"." << endl;
        return EXIT_FAILURE;
    }

    freqdem fdem = freqdem_create(kf);
    M17Demodulator m17dem;
    m17dem.init();

    float i, q;
    uint64_t cnt = 0;

    // Read I and Q (stored as floats)
    iq_in_file.read(reinterpret_cast<char *>(&i), 4);
    iq_in_file.read(reinterpret_cast<char *>(&q), 4);

    float msg_samp;
    shared_ptr<m17rx> rx_frame = make_shared<m17rx>();
    size_t demodulated_frames = 0;

    while( iq_in_file.good() && running )
    {
        complex<float> iq_samp(i, q);

        freqdem_demodulate(fdem, iq_samp, &msg_samp);

        bool new_frame = m17dem.update(&msg_samp, 1);

        if(new_frame)
        {
            cout << "New frame." << endl;

            M17::m17frame_t frame = m17dem.getFrame();
            M17::m17syncw_t m17sw = m17dem.getFrameSyncWord();
            uint16_t sw = (static_cast<uint16_t>(m17sw[0] << 8) + m17sw[1]);

            rx_frame->add_frame(sw, frame);

            if(rx_frame->is_error())
            {
                cerr << "M17 frame is in error" << endl;
                rx_frame = make_shared<m17rx>();
            }

            if(rx_frame->is_complete())
            {
                cout << "M17 Frame is complete!!!!" << endl;
                rx_frame = make_shared<m17rx>();
                demodulated_frames++;
            }
        }

        cnt++;

        // Read I and Q (stored as floats)
        iq_in_file.read(reinterpret_cast<char *>(&i), 4);
        iq_in_file.read(reinterpret_cast<char *>(&q), 4);
    }

    freqdem_destroy(fdem);
    iq_in_file.close();

    cout << "Read " << cnt << " samples." << endl;
    cout << "Successfuly demodulated " << demodulated_frames << " frames." << endl;

    return 0;
}
