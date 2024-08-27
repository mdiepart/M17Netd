#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

#include <netinet/ip.h>

#include "ConsumerProducer.h"

#include "radio_thread.h"
#include "config.h"

using namespace std;

void radio_simplex::operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<m17tx>> &to_radio,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_radio)
{
    radio_thread_cfg radio_cfg;
    cfg.getRadioConfig(radio_cfg);
    
    // For now, this threads gets packets from the network and display them

    shared_ptr<m17tx> packet;

    while(running)
    {
        int ret = to_radio.consume(packet);
        if(ret < 0)
        {
            // Timed out
            continue;
        }

        struct ip *pkt = reinterpret_cast<struct ip *>(packet->data());
        
        std::cout << "Received packet from network. Length of " << ntohs(pkt->ip_len) << "." << std::endl;
        /*ss.str(std::string());
            ss << "Received packet with content: ";
            for(auto const &v : *packet)
            {
                ss << std::setfill('0') << std::setw(sizeof(v) * 2)
                << std::hex << +v << ", ";
            }

            std::cout << ss.str() << "\n" << std::endl;*/
    }
    
}