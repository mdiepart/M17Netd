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

#include "sx1255.h"
#include <alsa/asoundlib.h>

#include "m17tx.h"
#include "sdrnode.h"
#include "liquid/liquid.h"

#include <cmath>
#include <vector>
#include <complex>
#include <iostream>
#include <array>
#include <csignal>

using namespace std;

volatile bool running = true;

void sigint_catcher(int signum)
{
    if(signum == SIGINT)
    {
        std::cout << "Ctrl-C caught, stopping BERT stream." << std::endl;
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
        cout << "Usage: " << argv[0] << " tx_frequency ppm_correction tx_gain\n"
                << "\ttx_frequency        is the frequency at which to transmit the BERT stream.\n"
                << "\tppm_correction      is the correction to apply to the frequency (in ppm, as an integer).\n"
                << "\ttx_gain             is the gain to use for the transmitter (must be in range [0, 15], each unit is 2 dB).\n"
                << "\tkf                  is the modulation index for the frequency modulator."
                << endl;
        return EXIT_SUCCESS;
    }
    else if(argc != 5)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }


    unsigned int tx_frequency = 0;
    int ppm = 0;
    int tx_gain = 0;
    float kf = 0;

    // Parse TX frequency
    try
    {
        tx_frequency = stoul(argv[1]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid TX frequency: \"" << argv[1] << "\"." << endl;
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

    // Parse TX gain
    try
    {
        tx_gain = stoi(argv[3]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid TX gain: \"" << argv[3] << "\"." << endl;
        return EXIT_FAILURE;
    }

    if(tx_gain < 0 || tx_gain > 15)
    {
        cerr << "TX gain of " << tx_gain << " is outside the valid range (must be between 0 and 15)." << endl;
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

    const size_t block_size = 128;
    array<liquid_float_complex, block_size> tx_samples;

    sdrnode radio(tx_frequency, tx_frequency, ppm);
    radio.set_tx_gain(tx_gain);
    radio.switch_tx();

    // Frequency modulator
    freqmod fmod = freqmod_create(kf);

    // bert stream tx
    m17tx_bert bert;

    while(true)
    {
        if(!running)
            bert.terminate_stream();

        vector<float> baseband = bert.get_baseband_samples(block_size);
        freqmod_modulate_block(fmod, baseband.data(), baseband.size(), reinterpret_cast<liquid_float_complex*>(tx_samples.data()));
        radio.transmit(tx_samples.data(), tx_samples.size());

        if(baseband.size() < block_size)
        {
            cout << "Reached end of BERT stream." << endl;
            break;
        }
    }

    radio.switch_rx();

    return 0;
}