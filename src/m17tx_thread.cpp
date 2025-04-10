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

#include "m17tx_thread.h"

#include <vector>
#include <atomic>
#include <cstdbool>
#include <string>
#include <iostream>
#include <map>
#include <thread>
#include <chrono>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <m17.h>
#include "config.h"
#include "m17tx.h"

using namespace std;

class m17_route
{
    public:
    /**
     * Creates a route entry from an IP V4 network using CIDR notation
     * i.e. 172.16.0.0/12
    */
    m17_route(const string_view &route_cidr)
    {
        // Parse the CIDR netmask length
        std::size_t idx = route_cidr.find_first_of('/');

        if(idx == std::string::npos)
        {
            mask_length = 32;
        }
        else
        {
            // Check that mask is not malformed
            std::string_view mask = route_cidr.substr(idx+1);
            int mask_len = 0;
            try
            {
                mask_len = std::stoi(mask.data());
            }
            catch(const std::out_of_range& e)
            {
                throw invalid_argument("Invalid CIDR mask length.");
            }

            if(mask_len < 0 || mask_len > 32)
            {
                throw invalid_argument("Invalid CIDR mask length.");
            }

            mask_length = mask_len;
        }


        route.s_addr = inet_addr(std::string(route_cidr.substr(0, idx)).c_str()) & getMask().s_addr;
    }

    m17_route(const uint32_t ip, const uint32_t mask_len): mask_length(mask_len)
    {
        route.s_addr = htonl(ip) & getMask().s_addr;
    }

    m17_route(const in_addr ip, const uint32_t mask_len): mask_length(mask_len)
    {
        route.s_addr = ip.s_addr & getMask().s_addr;
    }

    m17_route(const in_addr ip): mask_length(32), route(ip) {}

    unsigned short getMaskLength() const
    {
        return mask_length;
    }

    in_addr getNetwork() const
    {
        return route;
    }

    in_addr getMask() const
    {
        uint32_t mask = 0xFFFFFFFF << (32-mask_length);
        in_addr mask_ip = {.s_addr = htonl(mask)};
        return mask_ip;
    }

    // Comparison against other routes
    friend bool operator<(const m17_route &lhs, const m17_route &rhs)
    {
        in_addr shortest_mask = (lhs.getMaskLength() < rhs.getMaskLength())?lhs.getMask():rhs.getMask();
        return (lhs.route.s_addr & shortest_mask.s_addr) < (rhs.route.s_addr & shortest_mask.s_addr);
    }

    friend bool operator==(const m17_route &lhs, const m17_route &rhs)
    {
        return (lhs.mask_length == rhs.mask_length)
                && (lhs.route.s_addr == rhs.route.s_addr);
    }

    // Comparison against an IP address
    friend bool operator<(const m17_route &lhs, const in_addr &ip)
    {
        return lhs.route.s_addr < (ip.s_addr & lhs.getMask().s_addr);
    }

    friend bool operator==(const m17_route &lhs, const in_addr &ip)
    {
        return lhs.route.s_addr == (ip.s_addr & lhs.getMask().s_addr);
    }

    private:
    unsigned short mask_length;
    in_addr route;
};

void m17tx_thread::operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_net,
                    ConsumerProducerQueue<shared_ptr<m17tx_pkt>> &to_radio)
{
    vector<peer_t> peers = cfg.getPeers();
    const string_view src_callsign = cfg.getCallsign();
    map<m17_route, string> callsign_map;

    // Parse peers in a map
    for(auto const &p : peers)
    {
        for(auto const &r: p.routes)
        {
            callsign_map.insert(make_pair(m17_route(r), p.callsign));
        }

        if(callsign_map.find(m17_route(p.ip)) == callsign_map.end())
        {
            std::cout << "Routes do not yet contain IP of peer " << p.callsign <<
            " in the list. Adding a route to this specific peer." << std::endl;
            callsign_map.insert(std::make_pair(m17_route(p.ip), p.callsign));
        }
    }
    std::cout << "Content of callsign map: ";
    for(auto const &r: callsign_map)
    {
        char cstr[INET_ADDRSTRLEN];
        in_addr addr = r.first.getNetwork();
        std::string str = inet_ntop(AF_INET, &addr, cstr, INET_ADDRSTRLEN);
        //std::string str = inet_neta(r.first.getNetwork()), cstr, 16);
        std::cout << "\n\t" << cstr << "/" << r.first.getMaskLength() << "=>" << r.second;
    }

    std::cout << std::endl;


    // Test for a few routes
    /*const char *addrs[8] = {"127.0.0.1",
                     "172.16.0.1", "172.16.0.2",
                     "172.16.0.8", "172.16.0.9",
                     "172.16.0.16", "172.16.0.17",
                     "10.0.0.7"};

    static in_addr ip;
    for(size_t i = 0; i < 8; i++)
    {
        ip.s_addr = inet_addr(addrs[i]);
        auto test = callsign_map.find(ip);
        if(test == callsign_map.end())
        {
            std::cout << "No destination callsign found for address " << addrs[i] << "." << std::endl;
        }
        else{
            std::cout << "Destination callsign for ip " << addrs[i] << " is " << test->second << std::endl;
        }
    }*/

    while(running)
    {
        shared_ptr<vector<uint8_t>> raw;
        if(from_net.consume(raw) < 0)
        {
            continue;
        }

        struct ip *packet = reinterpret_cast<struct ip *>(raw->data());


        auto dst = callsign_map.find(packet->ip_dst);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(packet->ip_dst), ip, INET_ADDRSTRLEN);

        if(dst == callsign_map.end())
        {
            cerr << "Received a packet for \"" << ip << "\" but no route matches this address." << endl;
        }
        else
        {
            cout << "Received a packet (len=" << ntohs(packet->ip_len) << ") for " << ip << ". Sending to " << dst->second << "." << endl;
            shared_ptr<m17tx_pkt> baseband_pkt = shared_ptr<m17tx_pkt>(new m17tx_pkt(src_callsign, dst->second, raw));
            to_radio.add(baseband_pkt);
        }
    }
}