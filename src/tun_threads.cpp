#include <iostream>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdbool>
#include <memory>

#include <linux/if_tun.h>
#include <netinet/ip.h>
#include <unistd.h>

#include "tun_threads.h"
#include "tuntap.h"
#include "ConsumerProducer.h"
#include "config.h"

#include "m17.h"

using namespace std;

atomic_bool tun_dev_ready = false;
string tun_name;

void tun_thread_read::operator()(atomic_bool &running, const config &cfg,
                                 ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_net)
{
    tunthread_cfg if_cfg;
    cfg.getTunConfig(if_cfg);

    std::cout << "Tun thread starting. Configuration:"
    << "\n\tInterface name: " << if_cfg.name << "%d"
    << "\n\tInterface IP: " << if_cfg.ip
    << "\n\tInterface MTU: " << if_cfg.mtu << std::endl;

    // Append "%d" to the name
    std::string name = std::string(if_cfg.name) + "%d";

    // Setting-up tun device
    TunDevice interface(name);

    tun_name = interface.getName();

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
    tun_dev_ready = true;

    // Thread loop
    while( running )
    {
        packet = interface.getPacket(ref(running));

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
                from_net.add(packet);
            }
        }
    }
}

void tun_thread_write::operator()(atomic_bool &running, const config &cfg,
                                  ConsumerProducerQueue<shared_ptr<m17rx>> &to_net)
{
    // Wait for tun device to be ready
    while(!tun_dev_ready)
    {
        usleep(10000);
    }

    tunthread_cfg if_cfg;
    cfg.getTunConfig(if_cfg);

    // Parse toml configuration
    std::string name = std::string(if_cfg.name) + "%d";

    // Setting-up thread
    TunDevice interface(tun_name);

    std::shared_ptr<m17rx> packet;

    std::string_view radio_callsign = cfg.getCallsign();

    while(running)
    {
        if(to_net.consume(packet) != -1)
        {
            if(!packet->is_valid())
            {
                continue;
            }

            // check if the dst callsign matches our callsign
            array<uint8_t, 30> lsf = packet->get_lsf();
            lsf_t *m17_lsf = reinterpret_cast<lsf_t *>(&lsf);
            char dst_call[10];

            decode_callsign_bytes(dst_call, m17_lsf->dst);
            if(radio_callsign == dst_call)
            {
                // Check if payload is intact
                vector<uint8_t> payload = packet->get_payload();

                // Check if payload is at least 1 byte + specifier + CRC (4 bytes total)
                // Check if the specifier corresponds to IPV4
                if(payload.size() < 4 || payload[0] != 0x04)
                {
                    if(CRC_M17(payload.data()+1, payload.size()-1) == 0)
                    {
                        payload.erase(payload.cbegin()); // Remove type specifier
                        payload.erase(payload.cend() - 2, payload.cend()); // Remove CRC
                        interface.sendPacket(payload); // Send packet
                    }
                    else
                    {
                        cerr << "The CRC check of the payload failed" << endl;
                    }
                }

            }
        }

    }

}