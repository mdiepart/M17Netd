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

    ifstream iq_in_file("../iq_out.raw", ios_base::binary);

    freqdem fdem = freqdem_create(0.0375);
    static constexpr int32_t scale = (1 < 24);
    M17Demodulator m17dem;

    m17dem.init();
    int32_t i, q;
    uint64_t cnt = 0;
    while( iq_in_file >> i >> q )
    {
        cnt++;
        complex<float> iq_samp(static_cast<float>(i)/scale, static_cast<float>(q)/scale);
        float msg_samp;

        freqdem_demodulate(fdem, iq_samp, &msg_samp);
        bool new_frame = m17dem.update(&msg_samp, 1);

        if(new_frame)
        {
            cout << "New frame." << endl;
        }
    }

    iq_in_file.close();

    cout << "Read " << cnt << " samples." << endl;

    return 0;
}
