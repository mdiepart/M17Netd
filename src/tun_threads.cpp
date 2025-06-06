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
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdbool>
#include <memory>
#include <chrono>
#include <thread>

#include <errno.h>
#include <linux/if_tun.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "tun_threads.h"
#include "tuntap.h"
#include "ConsumerProducer.h"
#include "config.h"

#include "m17.h"

using namespace std;

void to_net_monitor(atomic_bool &running, ConsumerProducerQueue<shared_ptr<m17rx>> &to_net, int event_fd)
{
    std::chrono::milliseconds timeout(1000);
    uint64_t write_val = 1;

    while(running)
    {
        if(to_net.wait_for_non_empty(timeout))
        {
            ssize_t ret = write(event_fd, &write_val, 8);
            if(ret < 0)
            {
                cout << "write() error: returned " << errno << "(" << strerror(errno) << ")" << endl;
            }

            while(!to_net.isEmpty())
            {
                usleep(100);
            }
        }
    }
}



void tun_thread::operator()(atomic_bool &running, const config &cfg,
                                 ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_net,
                                 ConsumerProducerQueue<shared_ptr<m17rx>> &to_net)
{
    tunthread_cfg if_cfg;
    cfg.getTunConfig(if_cfg);
    std::string_view radio_callsign = cfg.getCallsign();

    std::cout << "Tun thread starting. Configuration:"
    << "\n\tInterface name: " << if_cfg.name << "%d"
    << "\n\tInterface IP: " << if_cfg.ip
    << "\n\tInterface MTU: " << if_cfg.mtu << std::endl;

    // Append "%d" to the name
    std::string name = std::string(if_cfg.name) + "%d";

    // Setting-up tun device
    tun_device interface(name);

    // Set IP address
    interface.set_IPV4(if_cfg.ip);

    // Set MTU
    interface.set_MTU(if_cfg.mtu);

    // Bring interface UP
    interface.set_up_down(true); // Up

    // Process peers
    for(auto const &p : if_cfg.peers)
    {
        interface.add_routes_to_peer(p);
    }

    std::shared_ptr<std::vector<uint8_t>> from_net_packet;
    std::shared_ptr<m17rx> to_net_packet;

    struct timespec read_timeout = {.tv_sec = 1, .tv_nsec = 0}; // 1 sec timeout
    fd_set read_fdset;

    int tun_fd = interface.get_tun_fd();
    int data_avail_fd = eventfd(0, EFD_NONBLOCK);
    int nfds = max(tun_fd, data_avail_fd) + 1;

    // Start to_net monitoring thread
    std::thread monitoring_thread = std::thread(to_net_monitor, std::ref(running), std::ref(to_net), data_avail_fd);

    // Thread loop
    while( running )
    {
        // Check if we can either read from tun or if data is available
        FD_ZERO(&read_fdset);
        FD_SET(interface.get_tun_fd(), &read_fdset);
        FD_SET(data_avail_fd, &read_fdset);
        int ret = pselect(nfds, &read_fdset, nullptr, nullptr, &read_timeout, nullptr);
        if(ret < 0)
        {
            std::cout << "Tun thread: pselect error (" << strerror(errno) << ")." << std::endl;
            continue;
        }
        else if(ret == 0)
        {
            // Pselect timed out, retry
            continue;
        }
        else
        {
            if(FD_ISSET(tun_fd, &read_fdset))
            {
                from_net_packet = interface.get_packet();

                if(from_net_packet->empty())
                {
                    std::cerr << "getPacket returned an empty vector. Errno = " << errno << std::endl;
                }
                else{
                    struct ip *pkt = reinterpret_cast<struct ip *>(from_net_packet->data());

                    if(pkt->ip_v != 4)
                    {
                        std::cerr << "Received an IP packet which is not ip V4" << std::endl;
                    }else{
                        from_net.add(from_net_packet);
                    }
                }
            }

            if(FD_ISSET(data_avail_fd, &read_fdset))
            {
                // Data can be read from to_net queue
                while(!to_net.isEmpty())
                {
                    to_net.consume(to_net_packet);

                    if(!to_net_packet->is_valid())
                    {
                        continue;
                    }

                    // check if the dst callsign matches our callsign
                    array<uint8_t, 30> lsf = to_net_packet->get_lsf();
                    lsf_t *m17_lsf = reinterpret_cast<lsf_t *>(&lsf);
                    char dst_call[10];

                    decode_callsign_bytes(dst_call, m17_lsf->dst);
                    if(radio_callsign == dst_call)
                    {
                        // Check if payload is intact
                        vector<uint8_t> payload = to_net_packet->get_payload();

                        // Check if payload is at least 1 byte + specifier + CRC (4 bytes total)
                        // Check if the specifier corresponds to IPV4
                        if(payload.size() >= 4 && payload[0] == 0x04)
                        {
                            if(CRC_M17(payload.data()+1, payload.size()-1) == 0)
                            {
                                payload.erase(payload.cbegin()); // Remove type specifier
                                payload.erase(payload.cend() - 2, payload.cend()); // Remove CRC
                                interface.send_packet(payload); // Send packet
                            }
                            else
                            {
                                cerr << "The CRC check of the payload failed" << endl;
                            }
                        }
                    }
                }

                // Clear data_avail_fd eventfd
                uint64_t tmp;
                read(data_avail_fd, &tmp, 8);
            }
        }
    }

    monitoring_thread.join();
}