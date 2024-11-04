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

#include <linux/spi/spidev.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "spi.h"

using namespace std;

spi_dev::spi_dev(string dev_name, int mode, int word_len, int speed)
{
    // Open device
    fd = open(dev_name.c_str(), O_RDWR);
    if(fd < 0)
    {
        throw runtime_error("Failed to open" + dev_name + ": "
        + strerror(errno) + ".");
    }

    // Set mode
    this->mode = mode;
    if(ioctl(fd, SPI_IOC_WR_MODE, &(this->mode)) < 0)
    {
        throw runtime_error("Failed to set bus mode on spi device " + dev_name
                            + ": " + strerror(errno) + "." );
    }

    // Set MSB endianness (LSB=0)
    endianness = 0;
    if(ioctl(fd, SPI_IOC_WR_LSB_FIRST, &endianness) < 0)
    {
        throw runtime_error("Failed to set SPI (" + dev_name
        + ") in MSB mode: " + strerror(errno) + ".");
    }

    this->word_len = word_len;
    if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &(this->word_len)) < 0)
    {
        throw runtime_error("Failed to set SPI (" + dev_name
        + ") word length: " + strerror(errno) + ".");
    }

    this->speed = speed;
    if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &(this->speed)) < 0)
    {
        throw runtime_error("Failed to set SPI (" + dev_name
        + ") max speed: " + strerror(errno) + ".");
    }

    cout << "Opened SPI device " << dev_name << endl;
}

spi_dev::spi_dev()
{
    fd = -1;
}

spi_dev::spi_dev(spi_dev &&dev)
{
    fd = dev.fd;
    dev.fd = -1;
    speed = dev.speed;
    mode = dev.mode;
    endianness = dev.endianness;
    word_len = dev.word_len;
}

spi_dev & spi_dev::operator=(spi_dev &&dev)
{
    fd = dev.fd;
    dev.fd = -1;
    speed = dev.speed;
    mode = dev.mode;
    endianness = dev.endianness;
    word_len = dev.word_len;

    return *this;
}

spi_dev::~spi_dev()
{
    if(fd>0)
        close(fd);
}

int spi_dev::recv(uint8_t *rx, const size_t n) const
{
    if(fd < 0)
        return -1;

    return read(fd, rx, n);
}

int spi_dev::send(const uint8_t *tx, const size_t n) const
{
    if(fd < 0)
        return -1;

    return write(fd, tx, n);
}

int spi_dev::send_recv(uint8_t *rx, const uint8_t *tx, const size_t n) const
{
    if(fd < 0)
        return -1;

    spi_ioc_transfer xfer = {0};

    xfer.rx_buf = (unsigned long long)rx;
    xfer.tx_buf = (unsigned long long)tx;
    xfer.len = n;

    // Send one message, xfer
    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &xfer);
    if(ret < 0)
    {
        cerr << "Error in spi send_recv ioctl: " << strerror(errno) << endl;
        return -1;
    }
    return 0;
}

int spi_dev::send_recv(uint8_t *buff, const size_t n) const
{
    return send_recv(buff, buff, n);
}