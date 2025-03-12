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
#include <csignal>
#include <fstream>
#include <cstring>

#include "sdrnode.h"

using namespace std;

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
        cout << "Usage: " << argv[0] << " rx_frequency ppm_correction rx_gain number_of_samples output_file\n"
             << "\trx_frequency        is the frequency at which to do the acquisition (in Hz).\n"
             << "\tppm_correction      is the correction to apply to the frequency (in ppm, as an integer).\n"
             << "\trx_gain             is the LNA gain (must be one of {0, -6, -12, -24, -36, -48})\n."
             << "\tnumber_of_samples   is the number of samples to acquire.\n"
             << "\toutput_file         is the file to which to write the acquired I/Q data (as float pairs, in binary)."
             << endl;
        return EXIT_SUCCESS;
    }else if(argc != 6)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }
    
    unsigned long rx_frequency;
    int ppm;
    int rx_gain;
    size_t acq_size;
    ofstream iq_out_file;

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

    // Parse number of samples
    try
    {
        acq_size = stoul(argv[4]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid number of samples: \"" << argv[4] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Try to open the output file
    try
    {
        iq_out_file.open(argv[4]);
    }
    catch(const std::exception &e)
    {
        cerr << "Unable to open output file \"" << argv[4] << "\"." << endl;
        return EXIT_FAILURE;
    }

    sdrnode radio = sdrnode(rx_frequency, rx_frequency, ppm);
    radio.switch_rx();

    size_t cnt = 0;
    constexpr size_t chunk = 960;
    complex<float> buffer[chunk] = {0};

    while( cnt < acq_size && running)
    {
        size_t n = min(chunk, acq_size - cnt);

        size_t read = radio.receive(buffer, n);
        iq_out_file.write(reinterpret_cast<char *>(buffer), n*sizeof(complex<float>));

        cnt += read;
    }

    iq_out_file.close();

    cout << "Acquired " << cnt << " samples." << endl;

    return 0;
}
