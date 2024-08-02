#pragma once

#include <string>
#include <vector>
#include <cstdint>

class TunDevice {

    public:
    TunDevice(const std::string &name);
    ~TunDevice();
    
    std::vector<uint8_t> getPacket();

    private:

    int fd;
    std::string ifName;
};