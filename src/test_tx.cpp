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

#include <cmath>
#include <vector>
#include <complex>
#include <iostream>
#include <fstream>
#include <array>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

int gpio_set_level(unsigned gpio, bool value);

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

    constexpr unsigned int freq = 445987500;
    constexpr int ppm = -28;
    constexpr unsigned int corrected_freq = freq + (static_cast<long>(freq)*ppm)/1000000;
    char spi_dev[] = "/dev/spidev1.0";

    static constexpr unsigned gpio_PA_enable       = 15;
    static constexpr unsigned gpio_TX_lowpower     = 16;
    static constexpr unsigned gpio_bias_enable     = 17;
    static constexpr unsigned gpio_SX1255_reset    = 54;
    static constexpr unsigned gpio_relay_TX        = 55;

    sx1255_drv sx1255 = sx1255_drv(spi_dev);

    gpio_set_level(gpio_SX1255_reset, 1);
    usleep(100);
    gpio_set_level(gpio_SX1255_reset, 0);
    usleep(5000);

    sx1255.init();
    sx1255.set_tx_freq(corrected_freq);
    sx1255.set_tx_mix_gain(2); // 5 by default

    gpio_set_level(gpio_relay_TX, 1);
    usleep(10000);
    gpio_set_level(gpio_PA_enable, 1);
    gpio_set_level(gpio_bias_enable, 1);

    sx1255.switch_tx();

    cout << "SDRNode now in TX mode. Use \"aplay\" to send samples to transmits" << endl;
    cout << "Example: aplay -D plughw:GDisDACout -r 96000 -t raw -f FLOAT_LE -c 2 ./my_samples.cf" << endl;
    while(running)
    {
    }

    sx1255.switch_rx();

    gpio_set_level(gpio_bias_enable, 0);
    gpio_set_level(gpio_PA_enable, 0);
    gpio_set_level(gpio_relay_TX, 0);

    return 0;
}

int gpio_set_level(unsigned gpio, bool value)
{
    int fd;
    static constexpr const char *sysfs_gpio_val    = "/sys/class/gpio/gpio%d/value";
    static constexpr size_t   sysfs_gpio_max_len   = 30;
    char gpio_filename[sysfs_gpio_max_len];

    snprintf(gpio_filename, sysfs_gpio_max_len, sysfs_gpio_val, gpio);

    fd = open(gpio_filename, O_WRONLY);
    if (fd < 0) {
        cerr << "gpioSetValue unable to open gpio "
             << to_string(gpio) << ": " << strerror(errno)
             << "." << endl;
        return -1;
    }

    if (value) {
        if (write(fd, "1", 2) != 2) {
            cerr << "gpioSetValue ON error : gpio "
                 << to_string(gpio) << ": " << strerror(errno)
                 << "." << endl;
            close(fd);
            return -1;
        }
    }
    else {
        if (write(fd, "0", 2) != 2) {
            cerr << "gpioSetValue OFF error :gpio "
                 <<  to_string(gpio) << ": " << strerror(errno)
                 << "." << endl;
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}
