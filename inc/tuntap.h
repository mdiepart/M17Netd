#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "config.h"

class TunDevice {

    public:
    TunDevice(const std::string_view &name);
    ~TunDevice();

    std::shared_ptr<std::vector<uint8_t>> getPacket(std::atomic<bool> &running);
    int sendPacket(const std::vector<uint8_t> &pkt);
    void setIPV4(std::string_view ip);
    void setUpDown(bool up);
    void setMTU(int mtu);
    int addRoutesForPeer(const peer_t &peer);
    std::string getName() const;

    private:

    int tun_fd;
    int sock_fd;
    std::string ifName;
    int mtu;
};