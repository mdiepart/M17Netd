#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>

class tunthread {
    public:
    void operator()(std::atomic_bool &running, const std::string &ifname);
};
