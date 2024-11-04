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

#include <toml.hpp>
#include <string>

#include "sx1255.h"

using namespace std;

typedef struct
{
    string_view callsign;
    string_view ip;
    vector<string_view> routes;
} peer_t;

typedef struct
{
    string_view    name;
    string_view    ip;
    size_t         mtu;
    vector<peer_t> peers;
} tunthread_cfg;

typedef struct
{
    string_view             spi_dev;    /* Path to the SPI device */
    string_view             i2s_tx;     /* Name of the i2s TX device */
    string_view             i2s_rx;     /* Name of the i2s RX device */
    sx1255_drv::lna_gain    lna_gain;   /* LNA gain (-48/-36/-24/-12/-6/max) */
    unsigned                mix_gain;   /* TX mixer gain (0 -> 15) */
} sdrnode_cfg;


typedef struct
{
    string_view   device;  /* Radio device to use */
    unsigned long tx_freq; /* TX Frequency */
    unsigned long rx_freq; /* RX Frequency */
    float         k;       /* FM Modulation index */
    float         ppm;     /* Frequency correction in ppm */
} radio_thread_cfg;

class config
{
    public:
    config(const string_view &file);
    int getTunConfig(tunthread_cfg &tun_cfg) const;
    int getRadioConfig(radio_thread_cfg &radio_cfg) const;
    int getSDRNodeConfig(sdrnode_cfg &cfg) const;
    vector<peer_t> getPeers() const;
    string_view getCallsign() const;
    size_t getTxQueueSize() const;
    size_t getRxQueueSize() const;

    private:
    toml::table config_tbl;
};