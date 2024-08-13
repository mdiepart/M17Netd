#include <thread>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdbool>

#include "tunthread.h"
#include "tuntap.h"


void tunthread::operator()(std::atomic_bool &running, tunthread_cfg &if_cfg)
{
    int err;

    std::cout << "Tun thread starting. Configuration:"
    << "\n\tInterface name: " << if_cfg.name << "%d"
    << "\n\tInterface IP: " << if_cfg.ip
    << "\n\tInterface MTU: " << if_cfg.mtu << std::endl;

    // Parse toml configuration
    std::string_view name = std::string(if_cfg.name) + "%d";

    // Setting-up thread
    TunDevice interface(name);

    // Set IP address
    interface.setIPV4(if_cfg.ip);

    // Set MTU
    interface.setMTU(if_cfg.mtu);

    // Bring interface UP
    interface.setUpDown(true); // Up  

    // Process peers
    std::cout << "TunThread has following peers:" << std::endl;
    for(auto const &p : if_cfg.peers)  
    {
        interface.addRoutesForPeer(p);
    }

    bool reading = true;

    std::stringstream ss;
    std::vector<uint8_t> packet;

    // Thread loop
    while( running )
    {
        packet = interface.getPacket(std::ref(running));
        if(packet.empty())
        {
            reading=false;
            std::cout << "getPacket returned an empty vector. Errno = " << errno << std::endl;
        }
        else{
            ss << "Received packet with content: ";

            for(auto const &v : packet)
            {
                ss << std::setfill('0') << std::setw(sizeof(v) * 2)
                << std::hex << +v << ", ";
            }

            std::cout << ss.str() << "\n" << std::endl;
        }
        
    }
}