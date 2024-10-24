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

    while( iq_in_file.good() && running )
    {
        complex<float> iq_samp(i, q);

        freqdem_demodulate(fdem, iq_samp, &msg_samp);

        bool new_frame = m17dem.update(&msg_samp, 1);

        if(new_frame)
        {
            cout << "New frame." << endl;

            array<uint8_t, 48> frame = m17dem.getFrame();
            array<uint16_t, 2*SYM_PER_FRA> soft_bits_frame = {0};

            cout << "Frame's two first bytes: 0x" << hex << static_cast<uint16_t>(frame[0]) << static_cast<uint16_t>(frame[1]) << dec << endl;

            // Inflate frame from hard bits to soft bits
            for(size_t i = 0; i < 2*SYM_PER_FRA; i++)
            {
                size_t byte = i/8;
                size_t bit = 7-(i%8);

                if((frame[byte] >> bit) & 1)
                    soft_bits_frame[i] = 0xFFFF;
            }

            rx_frame->add_frame(soft_bits_frame);

            if(rx_frame->is_error())
            {
                cerr << "M17 frame is in error" << endl;
                rx_frame = make_shared<m17rx>();
            }

            if(rx_frame->is_complete())
            {
                cout << "M17 Frame is complete!!!!" << endl;
                rx_frame = make_shared<m17rx>();
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

    return 0;
}
