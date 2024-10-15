#include <complex>
#include <iostream>
#include <csignal>
#include <fstream>
#include <cstring>
#include "sdrnode.h"
#include <alsa/asoundlib.h>

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
        cout << "Usage: " << argv[0] << " rx_frequency ppm_correction number_of_samples output_file\n"
             << "\trx_frequency        is the frequency at which to do the acquisition (in Hz).\n"
             << "\tppm_correction      is the correction to apply to the frequency (in ppm, as an integer).\n"
             << "\tnumber_of_samples   is the number of samples to acquire.\n"
             << "\toutput_file         is the file to which to write the acquired I/Q data (as float pairs, in binary)."
             << endl;
        return EXIT_SUCCESS;
    }else if(argc != 5)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }

    size_t acq_size = 0;
    ofstream iq_out_file;
    int ppm = 0;
    unsigned long rx_frequency;

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

    // Parse number of samples
    try
    {
        acq_size = stoul(argv[3]);
    }
    catch(const std::exception& e)
    {
        cerr << "Invalid number of samples: \"" << argv[1] << "\"." << endl;
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
