#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

typedef struct
{
    std::string_view callsign;
    std::string_view ip;
    std::vector<std::string_view> routes;
} tunthread_peer;

typedef struct
{
    std::string_view    name;
    std::string_view    ip;
    std::size_t         mtu;
    std::vector<tunthread_peer> peers;
} tunthread_cfg;

class tunthread {
    public:
    void operator()(std::atomic_bool &running, tunthread_cfg &if_cfg);
};
