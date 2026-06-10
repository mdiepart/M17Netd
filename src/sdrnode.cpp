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

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <alsa/asoundlib.h>

#include "spi.h"
#include "type_conversion.hpp"
#include "sdrnode.h"

sdrnode::sdrnode(const unsigned long rx_freq, const unsigned long tx_freq, const int ppm) : sx1255(spi_devname)
{
    long corr = (static_cast<long>(tx_freq)*ppm)/1000000;
    tx_frequency = tx_freq + corr;

    corr = (static_cast<long>(rx_freq)*ppm)/1000000;
    rx_frequency = rx_freq + corr;


    if(tx_frequency < 400e6 || tx_frequency > 510e6)
        throw invalid_argument("TX frequency is outside the [400,510] MHz range.");

    if(rx_frequency < 400e6 || rx_frequency > 510e6)
        throw invalid_argument("RX frequency is outside the [400,510] MHz range.");


    // Reset SX1255
    gpio_set_level(gpio_SX1255_reset, true);
    usleep(100u);
    gpio_set_level(gpio_SX1255_reset, false);
    usleep(5000u);

    sx1255.init();

    sx1255.set_rx_freq(rx_frequency);
    sx1255.set_tx_freq(tx_frequency);
    sx1255.set_lna_gain(sx1255_drv::LNA_GAIN_MAX_min36);
    sx1255.set_tx_mix_gain(12);

    // Open PCM device for RX
    open_pcm_rx();

    // Set gpios for RX
    prepare_rx();

    // Switch SX1255 to RX mode
    sx1255.switch_rx();

    tx_nRx = false;
}

sdrnode::~sdrnode()
{
    close_pcm();
    snd_config_update_free_global();
}

int sdrnode::gpio_set_level(unsigned gpio, bool value)
{
    int fd;
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

int sdrnode::open_pcm_rx()
{
    int err;
    snd_pcm_hw_params_t *pcm_hw_params;

    err = snd_pcm_open(&pcm_hdl, audio_rx_dev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        cerr << "Cannot open audio device " << audio_rx_dev
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return -1;
    }

    snd_pcm_hw_params_malloc(&pcm_hw_params);
    err = snd_pcm_hw_params_any(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "Cannot initialize hardware parameter structure: "
        << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_access(pcm_hdl, pcm_hw_params, pcm_access);
    if (err < 0) {
        cerr << "Cannot set access type: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S24_LE);
    if (err < 0) {
        cerr << "Cannot set sample format: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    pcm_rate = ideal_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_hdl, pcm_hw_params, &pcm_rate, 0);
    if (err < 0) {
        cerr << "cannot set sample rate: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_channels(pcm_hdl, pcm_hw_params, 2);
    if (err < 0) {
        cerr << "cannot set channel count: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "cannot set parameters: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_free(pcm_hw_params);

    //cout << "pcm_hw_params set successfuly" << endl;

    err = snd_pcm_prepare(pcm_hdl);
    if (err < 0) {
        cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return -1;
    }

    return 0;
}

int sdrnode::open_pcm_tx()
{
    int err;
    snd_pcm_hw_params_t *pcm_hw_params;

    err = snd_pcm_open(&pcm_hdl, audio_rx_dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        cerr << "Cannot open audio device " << audio_rx_dev
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_malloc(&pcm_hw_params);
    err = snd_pcm_hw_params_any(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "Cannot initialize hardware parameter structure: "
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_access(pcm_hdl, pcm_hw_params, pcm_access);
    if (err < 0) {
        cerr << "Cannot set access type: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S24_LE);
    if (err < 0) {
        cerr << "Cannot set sample format: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    pcm_rate = ideal_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_hdl, pcm_hw_params, &pcm_rate, 0);
    if (err < 0) {
        cerr << "cannot set sample rate: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_channels(pcm_hdl, pcm_hw_params, 2);
    if (err < 0) {
        cerr << "cannot set channel count: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "cannot set parameters: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_free(pcm_hw_params);

    //cout << "pcm_hw_params set successfuly" << endl;

    err = snd_pcm_prepare(pcm_hdl);
    if (err < 0) {
        cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    return 0;
}

void sdrnode::close_pcm()
{
    if(pcm_hdl == nullptr)
        return;

    snd_pcm_drain(pcm_hdl);
    snd_pcm_close(pcm_hdl);
}

int sdrnode::switch_rx()
{
    // Close pcm device and open it in rx
    close_pcm();
    int ret = open_pcm_rx();
    if(ret < 0)
        return -1;

    prepare_rx(); // Configure GPIOs

    ret = sx1255.switch_rx(); // Switch radio to RX mode
    if(ret < 0)
        return -1;

    tx_nRx = false;

    cout << "SDRNode in RX" << endl;

    return 0;
}

int sdrnode::switch_tx()
{
    // Close pcm device and open it in tx
    close_pcm();

    prepare_tx(); // Configure GPIOs

    int ret = open_pcm_tx();
    if(ret < 0)
    {
        prepare_rx();
        return -1;
    }


    ret = sx1255.switch_tx(); // Switch radio to RX mode
    if(ret < 0)
        return -1;

    tx_nRx = true;

    cout << "SDRNode in TX" << endl;

    return 0;
}

size_t sdrnode::receive(complex<float> *rx, size_t n)
{
    // TODO use snd_pcm_mmap_readi to avoid an unnecessary malloc and copy
    if(!tx_nRx)
    {
        int32_t *buff = new int32_t[n*2];
        if(buff == nullptr)
            return 0;

        snd_pcm_sframes_t read = snd_pcm_readi(pcm_hdl, buff, n);

        if(read < 0)
            read = snd_pcm_recover(pcm_hdl, read, 0);
        if(read < 0)
        {
            cout << "pcm read returned " << read << endl;
            delete[](buff);
            return 0;
        }
        else if(read > 0)
        {
            int32_to_float<24, 8>(buff, reinterpret_cast<float*>(rx), (size_t)read*2);
            delete[](buff);
            return read;
        }
    }

    return 0;
}

int sdrnode::transmit(const complex<float> *tx, size_t n)
{
    if(tx_nRx)
    {
        int32_t *buff = new int32_t[n*2];

        if(buff == nullptr)
            return 0;

        float_to_int32<24>(reinterpret_cast<const float*>(tx), buff, n*2);

        snd_pcm_sframes_t written = 0;
        while(n > 0)
        {
            written = snd_pcm_writei(pcm_hdl, buff, n);
            if(written > 0)
                n -= written;
            else if(written < 0)
            {
                written = snd_pcm_recover(pcm_hdl, written, 0);
                if(written < 0)
                    break;
            }

        }

        delete[](buff);

        return (written < 0)?-1:0;
    }

    return 0;
}

int sdrnode::set_rx_gain(sx1255_drv::lna_gain gain)
{
    return sx1255.set_lna_gain(gain);
}

int sdrnode::set_tx_gain(unsigned gain)
{
    if(gain > 15)
        return -1;

    return sx1255.set_tx_mix_gain(gain);
}

void sdrnode::set_tx_high(const bool high)
{
    tx_high = high;
}