#include <thread>
#include <iostream>
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
        std::cout << "-" << p.callsign << " @ " << p.ip
                  << "\n\tRoutes:\n";
        for(auto const &r : p.routes)
        {
            std::cout << "\t\t" << r;
        }
        std::cout << std::endl;
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

            for(auto it = packet.begin(); it < packet.end(); it++)
            {
                ss << std::hex << *it << ", ";
            }

            std::cout << ss.str() << std::endl;
        }
        
    }
}