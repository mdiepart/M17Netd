/****************************************************************************
 * M17Netd                                                                  *
 * Copyright (C) 2024 by Morgan Diepart ON4MOD                              *
 *                       SDR-Engineering SRL                                *
 *                                                                          *
 * This program is free software: you can redistribute it and/or modify     *
 * it under the terms of the GNU Affero General Public License as published *
 * by the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                      *
 *                                                                          *
 * This program is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 * GNU Affero General Public License for more details.                      *
 *                                                                          *
 * You should have received a copy of the GNU Affero General Public License *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ****************************************************************************/

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include "ConsumerProducer.h"

#include "tun_threads.h"
#include "radio_thread.h"
#include "m17tx_thread.h"
#include "m17tx.h"
#include "m17rx.h"

std::atomic<bool> running; // Signals to the threads that program must stop and exit

void sigint_catcher(int signum)
{
    if(signum == SIGINT)
    {
        std::cout << "Ctrl-C caught, stopping all threads." << std::endl;
        running = false;
    }
}

int main(int argc, char *argv[])
{
    std::string config_file;

    std::cout << "Starting M17Netd" << std::endl;

    // Parse input arguments
    if(argc < 2)
    {
        std::cerr << "No configuration file provided. Exiting." << std::endl;
        return EXIT_FAILURE;
    }
    else
    {
        config_file = std::string(argv[1]);
        std::cout << "Using configuration file \"" << config_file << "\"." << std::endl;

        if(argc > 2)
        {
            std::cerr << "Ignoring additional options ";
            for(int i = 2; i < argc; i++)
            {
                std::cerr << "\"" << argv[i] << "\"";
                if(i < argc-1)
                {
                    std::cerr << ",";
                }
            }
            std::cerr << "." << std::endl;
        }
    }

    // Parse config file
    config cfg = config(config_file);

    std::size_t txQueueSize = cfg.getTxQueueSize();
    std::size_t rxQueueSize = cfg.getRxQueueSize();
    ConsumerProducerQueue<std::shared_ptr<std::vector<uint8_t>>> from_net(txQueueSize);
    ConsumerProducerQueue<std::shared_ptr<m17tx_pkt>> to_radio(txQueueSize);
    ConsumerProducerQueue<std::shared_ptr<m17rx>> from_radio(rxQueueSize);


    // Start threads
    running = true;

    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, 0);

    std::thread tun_read = std::thread(tun_thread(), std::ref(running), std::ref(cfg), std::ref(from_net), std::ref(from_radio));
    std::thread radio = std::thread(radio_simplex(), std::ref(running), std::ref(cfg), std::ref(to_radio), std::ref(from_radio));
    std::thread m17tx = std::thread(m17tx_thread(), std::ref(running), std::ref(cfg), std::ref(from_net), std::ref(to_radio));

    // Wait for threads to terminate
    tun_read.join();
    std::cout << "tun read thread stopped" << std::endl;
    radio.join();
    std::cout << "radio thread stopped" << std::endl;
    m17tx.join();
    std::cout << "M17 tx thread stopped" << std::endl;

    return EXIT_SUCCESS;
}
