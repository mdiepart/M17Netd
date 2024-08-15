#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include "ConsumerProducer.h"

#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>

#include "tunthread.h"
#include "radio_thread.h"

std::atomic<bool> running; // Signals to the threads that program must stop and exit

int parse_tun_config(const toml::table &toml_cfg, tunthread_cfg &tun);

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
    toml::table config_tbl;
    tunthread_cfg net_if_cfg;
    radio_thread_cfg radio_cfg;

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
            for(std::size_t i = 2; i < argc; i++)
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

    // Try to parse config file
    try
    {
        config_tbl = toml::parse_file(config_file);
    }
    catch(const toml::parse_error& e)
    {
        std::cerr << "Parsing config file failed:\n" << e << std::endl;
        return EXIT_FAILURE;
    }

    // Extract configs
    parse_tun_config(config_tbl, net_if_cfg);
    
    std::size_t txQueueSize = config_tbl["general"]["tx_queue_size"].value_or(64);
    std::size_t rxQueueSize = config_tbl["general"]["rx_queue_size"].value_or(64);
    ConsumerProducerQueue<std::shared_ptr<std::vector<uint8_t>>> toRadio(txQueueSize);
    ConsumerProducerQueue<std::shared_ptr<std::vector<uint8_t>>> toNet(rxQueueSize);

    
    // Start threads
    running = true;

    std::thread tuntap = std::thread(tunthread(), std::ref(running), std::ref(net_if_cfg), std::ref(toRadio));
    std::thread radio = std::thread(radio_simplex(), std::ref(running), std::ref(radio_cfg), std::ref(toRadio), std::ref(toNet));

    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, 0);

    // Wait for threads to terminate
    tuntap.join();
    std::cout << "tuntap thread stopped" << std::endl;
    radio.join();
    std::cout << "radio thread stopped" << std::endl;

    return EXIT_SUCCESS;
}

int parse_tun_config(const toml::table &toml_cfg, tunthread_cfg &tun)
{
    tun.name = toml_cfg["general"]["net_if"]["name"].value_or("m17d");
    tun.mtu = toml_cfg["general"]["net_if"]["mtu"].value_or(822);
    tun.ip = toml_cfg["general"]["net_if"]["ip"].value_or("172.16.0.128");
    tun.peers = std::vector<tunthread_peer>();

    const toml::array &peers = *toml_cfg.get_as<toml::array>("peers");

    if(&peers == nullptr){
        std::cerr << "No peers found in configuration file" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Config file contains " << peers.size() << " peers." <<  std::endl;

    //for(std::size_t i = 0; i < peers.size(); i++)
    for(auto it = peers.begin(); it < peers.end(); it++)
    {
        //const toml::table *table = peers[i].as_table();
        const toml::table *peer = it->as_table();

        tunthread_peer p;
        std::optional<std::string_view> callsign = peer->at_path("callsign").value<std::string_view>();
        if(callsign.has_value())
        {
            p.callsign = callsign.value();
            std::cout << "Added peer with callsign " << p.callsign << std::endl;
        }
        else
        {
            std::cerr << "Missing callsign in peer " << *peer << std::endl;
            continue;
        }

        std::optional<std::string_view> ip = peer->at_path("ip").value<std::string_view>();
        if(ip.has_value())
        {
            p.ip = ip.value();
            std::cout << "Added ip " << p.ip << " for peer " << p.callsign <<  std::endl;
        }
        else
        {
            std::cerr << "Missing IP in peer " << *peer << std::endl;
            continue;
        }

        const toml::array *routes = peer->at_path("routes").as_array();
        if(routes == nullptr)
        {
            std::cerr << "Missing routes in peer " << *peer << std::endl;
        }
        else
        {
            p.routes = std::vector<std::string_view>();

            for(auto r = routes->cbegin(); r < routes->cend(); r++)
            {
                p.routes.push_back(r->value_or(""));
                std::cout << "added route " << p.routes.back() << " for peer " << p.callsign << std::endl;
            }
        }

        tun.peers.push_back(p);
    }


    return EXIT_SUCCESS;    
}

