#pragma once

#include <string>
#include <vector>
#include <cstdint>

class TunDevice {

    public:
    TunDevice(std::string name);

    std::vector<uint8_t> getPacket();

    private:

    int fd;
};