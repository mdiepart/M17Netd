#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <sys/socket.h>

class TunDevice {

    public:
    TunDevice(const std::string &name);
    ~TunDevice();
    
    std::vector<uint8_t> getPacket();
    void setIPV4(const char *ip);
    void setUpDown(bool up);
    void setMTU(int mtu);
    private:

    int tun_fd;
    int sock_fd;
    std::string ifName;
};