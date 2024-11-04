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

#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>

#include <linux/spi/spidev.h>

using namespace std;

class spi_dev
{
    public:
    spi_dev();
    spi_dev(string dev_name, int mode, int word_len, int speed);

    // Disable copy constructor because the fd should be unique among objects
    spi_dev(const spi_dev &dev) = delete;

    // Move contructor: copy fd and set origin fd to -1
    spi_dev(spi_dev &&dev);

    // Disable copy assignment because the fd should be unique among objects
    spi_dev& operator=(const spi_dev &) = delete;

    // Move assignment: copy fd and set origin fd to -1
    spi_dev& operator=(spi_dev &&);
    ~spi_dev();

    /** Reads n bytes from the spi bus
     * @param rx buffer in which the n bytes read will be written
     * @param n number of bytes to read
     *
     * @returns the number of bytes read, 0 for EOF or -1 for errors.
     */
    int recv(uint8_t *rx, const size_t n) const;

     /** Reads n bytes from the spi bus
     * @param tx buffer containing the n bytes to send
     * @param n number of bytes to send
     *
     * @returns the number of bytes sent or -1 for errors.
     */
    int send(const uint8_t *tx, const size_t n) const;

    /** Performs a full-duplex exchange on the SPI bus, simultaneously sending and receiving n bytes.
     * @param rx buffer in which the n bytes read will be written
     * @param tx buffer containing the n bytes to send
     * @param n number of bytes to send/receive
     *
     * @remark It is allowed for rx and tx to point to the same buffer although a specific function
     *          is provided for that usecase.
     *
     * @return 0 on success, -1 on error
     */
    int send_recv(uint8_t *rx, const uint8_t *tx, const size_t n) const;

    /** Performs a full-duplex exchange on the SPI bus, simultaneously sending and receiving n bytes.
     * @param buff buffer containing the n bytes to send. When returning, this buffer will contain the n bytes read.
     * @param n number of bytes to send/receive
     *
     * @return 0 on success, -1 on error
     */
    int send_recv(uint8_t *buff, const size_t n) const;

    private:
    int fd;
    unsigned long speed;
    unsigned char mode;
    unsigned char endianness;
    unsigned char word_len;
};