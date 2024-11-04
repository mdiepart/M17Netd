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

#include <string>
#include <vector>
#include <cstdint>

#include "config.h"

class tun_device {

    public:
    tun_device(const std::string_view &name);
    ~tun_device();

    /**
     * Reads a packet from the TUN interface
     *
     * @return A shared pointer to a uint8_t vector containing the raw IP packet.
     */
    std::shared_ptr<std::vector<uint8_t>> get_packet();

    /**
     * Sends a packet to the TUN interface
     *
     * @param pkt a uint8_t pointer containing the raw IP packet to send
     *
     * @return 0 if successful, -1 in case of error
     */
    int send_packet(const std::vector<uint8_t> &pkt);

    /**
     * Sets the local IP V4 of the tun interface
     *
     * @param ip a string containing the ip address (in the form aaa.bbb.ccc.ddd)
     */
    void set_IPV4(std::string_view ip);

    /**
     * Sets the interface up or down
     *
     * @param up true to set the interface up, false to set it down
     */
    void set_up_down(bool up);

    /**
     * Sets the MTU of the interface
     *
     * @param mtu the MTU in bytes
     */
    void set_MTU(int mtu);

    /**
     * Add the routes contained in a peer
     *
     * @param peer a peer containing one or several associated routes
     */
    int add_routes_to_peer(const peer_t &peer);

    /**
     * Gets the name of the interface
     *
     * @return A string containing the interface name
     */
    std::string get_if_name() const;

    /**
     * Gets the file descriptor of the TUN interface
     *
     * @return the file descriptor of the TUN interface
     */
    int get_tun_fd() const;

    /**
     * Gets the file descriptior of the socket opened on the TUN interface
     *
     * @return the file descriptor of the socket opened on the TUN interface
     */
    int get_sock_fd() const;

    private:

    int tun_fd;
    int sock_fd;
    std::string if_name;
    int mtu;
};