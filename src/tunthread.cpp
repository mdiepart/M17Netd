#include <thread>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <fstream>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdbool>

#include <linux/if.h>
#include <linux/if_tun.h>


#include "tunthread.h"
#include "tuntap.h"

void tunthread::operator()(std::atomic_bool &running, const std::string &ifname)
{
    int err;

    // Setting-up thread
    TunDevice interface(ifname);

    // Set IP address
    interface.setIPV4("172.16.0.1");

    // Set MTU
    interface.setMTU(868);


    // Bring interface UP
    interface.setUpDown(true); // Up    


    // Thread loop
    while( running )
    {
        std::vector<uint8_t> packet = interface.getPacket();
        std::stringstream ss;
        ss << "Received packet with content: ";

        for(auto it = packet.begin(); it < packet.end(); it++)
        {
            ss << std::hex << *it << ", ";
        }

        //std::cout << ss.str() << std::endl;
    }
}