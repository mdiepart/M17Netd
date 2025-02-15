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

#include <iostream>
#include <string>
#include <array>
#include <iostream>

#include "sx1255.h"

sx1255_drv::sx1255_drv(const string dev_name) : spi(dev_name, SPI_MODE_0, 0, 500000)
{
    unsigned ver = read_version();
    cout << "SX1255 Hardware version is 0x" << hex << ver << dec << endl;
}

int sx1255_drv::init()
{
    array<uint8_t, 8> buffer = {0};

    // Mode register
    buffer[0] = MODE_ADDR | REG_WRITE;
    buffer[1] = MODE(0, 0, 0, 1); // Enable Power Distribution System
    int ret = spi.send(buffer.data(), 2);
    if(ret < 0)
        return -1;

    // Front-ends registers
    buffer = {
        TXFE1_ADDR | REG_WRITE,         // Write start at TXFE1
        TXFE1(DAC_GAIN_MAX_min3, 0x0E), // Default values
        TXFE2(),
        TXFE3(),
        TXFE4(),
        RXFE1(LNA_GAIN_MAX, 0x0F),      // default values
        RXFE2(),
        RXFE3()
    };
    ret = spi.send(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    // CK_SEL register
    buffer[0] = CK_SEL_ADDR | REG_WRITE;
    buffer[1] = CK_SEL();
    ret = spi.send(buffer.data(), 2);
    if(ret < 0)
        return -1;

    // IISM and DIG_BRIDGE registers
    buffer[0] = IISM_ADDR | REG_WRITE;
    buffer[1] = IISM();
    buffer[2] = DIG_BRIDGE();
    ret = spi.send(buffer.data(), 3);
    if(ret < 0)
        return -1;

    return 0;
}

int sx1255_drv::set_tx_freq(unsigned long freq)
{
    if((freq < 400e6) | (freq > 510e6))
        return -1;

    uint32_t val = sx1255_calc_freq(freq);

    // Message is composed of [addr, val, addr, val, ...]
    const uint8_t val1 = static_cast<uint8_t>(val);
    const uint8_t val2 = static_cast<uint8_t>(val >> 8);
    const uint8_t val3 = static_cast<uint8_t>(val >> 16);

    array<uint8_t, 4> buffer = {
        FRFH_TX_ADDR | REG_WRITE,
        val3,
        val2,
        val1
    };

    int ret = spi.send(buffer.data(), buffer.size());

    if(ret < 0)
        return -1;

    cout << "Set TX frequency to " << freq/1000 << " kHz." << endl;
    return 0;
}

int sx1255_drv::set_rx_freq(unsigned long freq)
{
    if((freq < 400e6) | (freq > 510e6))
        return -1;

    uint32_t val = sx1255_calc_freq(freq);

    // Message is composed of [addr, val, addr, val, ...]
    const uint8_t val1 = static_cast<uint8_t>(val);
    const uint8_t val2 = static_cast<uint8_t>(val >> 8);
    const uint8_t val3 = static_cast<uint8_t>(val >> 16);

    array<uint8_t, 4> buffer = {
        FRFH_RX_ADDR | REG_WRITE,
        val3,
        val2,
        val1
    };

    int ret = spi.send(buffer.data(), buffer.size());

    if(ret < 0)
        return -1;

    cout << "Set RX frequency to " << freq/1000 << " kHz." << endl;
    return 0;
}

int sx1255_drv::set_dac_gain(dac_gain gain)
{

    array<uint8_t, 2> buffer = {TXFE1_ADDR, 0x00};

    // First querry the current value of TXFE1
    int ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    // Modify the value and send
    buffer[1] = (buffer[1] & 0x0F) | gain;
    buffer[0] = TXFE1_ADDR | REG_WRITE;
    ret = spi.send(buffer.data(), buffer.size());

    if(ret < 0)
        return -1;

    return 0;
}

int sx1255_drv::set_lna_gain(lna_gain gain)
{
    array<uint8_t, 2> buffer = {RXFE1_ADDR, 0x00};

    // First querry the current value of RXFE1
    int ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    // Modify the value and send
    buffer[1] = (buffer[1] & 0x1F) | (gain << 5);
    buffer[0] = RXFE1_ADDR | REG_WRITE;
    ret = spi.send(buffer.data(), buffer.size());

    if(ret < 0)
        return -1;

    return 0;
}

int sx1255_drv::set_rx_pga_gain(unsigned char gain)
{
    if(gain > 0x0F)
        return -1;

    array<uint8_t, 2> buffer = {RXFE1_ADDR, 0x00};

    int ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    buffer[1] = (buffer[1] & 0xE1) | gain;
    buffer[0] = RXFE1_ADDR | REG_WRITE;
    ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    return 0;
}

int sx1255_drv::set_tx_mix_gain(unsigned char gain)
{
    if(gain > 0x0F)
        return -1;

    array<uint8_t, 2> buffer = {TXFE1_ADDR, 0x00};

    int ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    buffer[1] = (buffer[1] & 0xF0) | gain;
    buffer[0] = TXFE1_ADDR | REG_WRITE;
    ret = spi.send_recv(buffer.data(), buffer.size());
    if(ret < 0)
        return -1;

    return 0;
}

uint8_t sx1255_drv::read_version()
{
    array<uint8_t, 2> buffer = {VERSION_ADDR, 0x00};

    int ret = spi.send_recv(buffer.data(), buffer.size());

    if(ret < 0)
        return 0;

    return buffer[1];
}

int sx1255_drv::switch_rx()
{
    array<uint8_t, 2> buffer = {MODE_ADDR | REG_WRITE, MODE(0, 0, 1, 1)};

    int ret = spi.send(buffer.data(), buffer.size());

    buffer[0] = STAT_ADDR;
    bool rx_pll_locked = false;
    int iter = 20; // This number was just deemed reasonable, not tested
    do
    {
        buffer[1] = 0x00;
        ret = spi.send_recv(buffer.data(), buffer.size());
        if(ret < 0)
            return -1;

        rx_pll_locked = buffer[1] & 0x02;
        iter--;
    }while(!rx_pll_locked && iter > 0);

    return rx_pll_locked?0:-1; // In case iter is -1 but the pll did lock
}

int sx1255_drv::switch_tx()
{
    array<uint8_t, 2> buffer = {MODE_ADDR | REG_WRITE, MODE(1, 1, 0, 1)};

    int ret = spi.send(buffer.data(), buffer.size());

    if(ret < 0)
        return -1;

    buffer[0] = STAT_ADDR;
    bool tx_pll_locked = false;
    int iter = 20; // This number was just deemed reasonable, not tested
    do
    {
        buffer[1] = 0x00;
        ret = spi.send_recv(buffer.data(), buffer.size());
        if(ret < 0)
            return -1;

        tx_pll_locked = buffer[1] & 0x01;
        iter--;
    }while(!tx_pll_locked && iter > 0);

    return tx_pll_locked?0:-1; // In case iter is -1 but the pll did lock
}

void sx1255_drv::dump_regs(ostream &strout)
{
    // Collect all registers
    constexpr array<uint8_t, 19> regs = {MODE_ADDR, FRFH_RX_ADDR, FRFM_RX_ADDR, FRFL_RX_ADDR,
                                         FRFH_TX_ADDR, FRFM_TX_ADDR, FRFL_TX_ADDR, VERSION_ADDR,
                                         TXFE1_ADDR, TXFE2_ADDR, TXFE3_ADDR, TXFE4_ADDR,
                                         RXFE1_ADDR, RXFE2_ADDR, RXFE3_ADDR,
                                         CK_SEL_ADDR, STAT_ADDR, IISM_ADDR, DIG_BRIDGE_ADDR};

    array<uint8_t, 2> buffer;
    strout << "SX1255 internal registers: {\n\t" << hex;
    for(const auto &reg: regs)
    {
        buffer[0] = reg;
        spi.send_recv(buffer.data(), 2);
        strout << "{0x" << static_cast<uint16_t>(reg) << ": 0x" << static_cast<uint16_t>(buffer[1]) << "},\n\t";
    }
    strout << dec << "}" << endl;
}
