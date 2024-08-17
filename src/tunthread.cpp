#include <iostream>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdbool>
#include <memory>

#include <linux/if_tun.h>
#include <netinet/ip.h>

#include "tunthread.h"
#include "tuntap.h"
#include "ConsumerProducer.h"

void tunthread::operator()(std::atomic_bool &running, tunthread_cfg &if_cfg,
                           ConsumerProducerQueue<std::shared_ptr<std::vector<uint8_t>>> &toRadio)
{
    std::cout << "Tun thread starting. Configuration:"
    << "\n\tInterface name: " << if_cfg.name << "%d"
    << "\n\tInterface IP: " << if_cfg.ip
    << "\n\tInterface MTU: " << if_cfg.mtu << std::endl;

    // Parse toml configuration
    std::string name = std::string(if_cfg.name) + "%d";

    // Setting-up thread
    TunDevice interface(name);

    // Set IP address
    interface.setIPV4(if_cfg.ip);

    // Set MTU
    interface.setMTU(if_cfg.mtu);

    // Bring interface UP
    interface.setUpDown(true); // Up  

    // Process peers
    for(auto const &p : if_cfg.peers)  
    {
        interface.addRoutesForPeer(p);
    }

    std::shared_ptr<std::vector<uint8_t>> packet;

    // Thread loop
    while( running )
    {
        packet = interface.getPacket(std::ref(running));
        
        if(packet->empty())
        {
            std::cerr << "getPacket returned an empty vector. Errno = " << errno << std::endl;
        }
        else{
            struct ip *pkt = reinterpret_cast<struct ip *>(packet->data());
        
            if(pkt->ip_v != 4)
            {
                std::cerr << "Received an IP packet which is not ip V4" << std::endl;
            }else{
                toRadio.add(packet);
            }
        }
    }
}