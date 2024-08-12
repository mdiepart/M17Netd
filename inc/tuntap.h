#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <sys/socket.h>

class TunDevice {

    public:
    TunDevice(const std::string_view name);
    ~TunDevice();
    
    std::vector<uint8_t> getPacket(std::atomic<bool> &running);
    void setIPV4(std::string_view ip);
    void setUpDown(bool up);
    void setMTU(int mtu);
    void addRoute(std::string_view route);

    private:

    int tun_fd;
    int sock_fd;
    std::string ifName;
    int mtu;
};