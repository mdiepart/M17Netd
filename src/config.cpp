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

#include <string>
#include <string_view>
#include <vector>
#include <iostream>

#include <toml.hpp>

#include "sx1255.h"

#include "config.h"

using namespace std;

config::config(const string_view &file)
{
    config_tbl = toml::parse_file(file);
}

int config::getTunConfig(tunthread_cfg &tun_cfg) const
{
    tun_cfg.name = config_tbl["general"]["net_if"]["name"].value_or("m17d");
    tun_cfg.mtu = config_tbl["general"]["net_if"]["mtu"].value_or(822);
    tun_cfg.ip = config_tbl["general"]["net_if"]["ip"].value_or("172.16.0.128");
    tun_cfg.peers = getPeers();


    return EXIT_SUCCESS;
}

int config::getRadioConfig(radio_thread_cfg &radio_cfg) const
{
    radio_cfg.device    = config_tbl["radio"]["device"].value_or("");
    radio_cfg.rx_freq   = config_tbl["radio"]["rx_frequency"].value_or(0UL);
    radio_cfg.tx_freq   = config_tbl["radio"]["tx_frequency"].value_or(0UL);
    radio_cfg.k         = config_tbl["radio"]["k_mod"].value_or(0.0f);
    radio_cfg.ppm       = config_tbl["radio"]["ppm"].value_or(0);

    return EXIT_SUCCESS;
}

int config::getSDRNodeConfig(sdrnode_cfg &cfg) const
{
    cfg.spi_dev = config_tbl["sdrnode"]["spi_dev"].value_or("");
    cfg.i2s_rx = config_tbl["sdrnode"]["i2s_rx"].value_or("");
    cfg.i2s_tx = config_tbl["sdrnode"]["i2s_tx"].value_or("");

    int lna_gain = config_tbl["sdrnode"]["lna_gain"].value_or(INT_MIN);
    switch(lna_gain)
    {
        case -48:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min48;
            break;
        case -36:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min36;
            break;
        case -24:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min24;
            break;
        case -12:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min12;
            break;
        case -6:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min6;
            break;
        case 0:
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX;
            break;
        default:
            cerr << "Invalid LNA gain for SDRNode (" << lna_gain << "). Using -24." << endl;
            cfg.lna_gain = sx1255_drv::LNA_GAIN_MAX_min24;
    }

    unsigned mix_gain = config_tbl["sdrnode"]["mix_gain"].value_or(UINT_MAX);
    if(mix_gain > 15)
    {
        cerr << "SDRNode TX mixer gain too high (" << mix_gain << "). Using 15." << endl;
        mix_gain = 15;
    }

    cfg.mix_gain = mix_gain;

    return EXIT_SUCCESS;
}

string_view config::getCallsign() const
{
    optional<string_view> cs = config_tbl["general"]["callsign"].value<string_view>();
    if(cs.has_value())
    {
        return cs.value();
    }
    else
    {
        return string_view();
    }
}

vector<peer_t> config::getPeers() const
{
    vector<peer_t> peers = vector<peer_t>();

    const toml::array &toml_peers = *config_tbl.get_as<toml::array>("peers");

    if(toml_peers.empty()){
        return peers;
    }

    for(auto it = toml_peers.begin(); it < toml_peers.end(); it++)
    {
        const toml::table *peer = it->as_table();

        peer_t p;
        optional<string_view> callsign = peer->at_path("callsign").value<string_view>();
        if(callsign.has_value())
        {
            p.callsign = callsign.value();
        }
        else
        {
            cerr << "Missing callsign in peer " << *peer << endl;
            continue;
        }

        optional<string_view> ip = peer->at_path("ip").value<string_view>();
        if(ip.has_value())
        {
            p.ip = ip.value();
        }
        else
        {
            cerr << "Missing IP in peer " << *peer << endl;
            continue;
        }

        const toml::array *routes = peer->at_path("routes").as_array();
        if(routes == nullptr)
        {
            cerr << "Missing routes in peer " << *peer << endl;
        }
        else
        {
            p.routes = vector<string_view>();

            for(auto r = routes->cbegin(); r < routes->cend(); r++)
            {
                p.routes.push_back(r->value_or(""));
            }
        }

        peers.push_back(p);
    }

    return peers;
}

size_t config::getTxQueueSize() const
{
    return config_tbl["general"]["tx_queue_size"].value_or(4);
}

size_t config::getRxQueueSize() const
{
    return config_tbl["general"]["rx_queue_size"].value_or(4);
}

