#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>

#include "tunthread.h"

std::atomic<bool> running; // Signals to the threads that program must stop and exit

int parse_config(const toml::table &toml_cfg, tunthread_cfg &tun);

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
    tunthread_cfg net_if_cfg;
    parse_config(config_tbl, net_if_cfg);

    std::cout << "Config parsed, if_name=" << net_if_cfg.name << std::endl;

    // Start threads
    running = true;

    std::thread tuntap = std::thread(tunthread(), std::ref(running), std::ref(net_if_cfg));

    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;// &= ~(SA_RESTART);
    sigaction(SIGINT, &sigint_handler, 0);

    // Wait for threads to terminate
    tuntap.join();

    std::cout << "tuntap thread stopped" << std::endl;

    return EXIT_SUCCESS;
}

int parse_config(const toml::table &toml_cfg, tunthread_cfg &tun)
{
    tun.name = toml_cfg["general"]["net_if"]["name"].value_or("m17d");
    tun.mtu = toml_cfg["general"]["net_if"]["mtu"].value_or(822);
    tun.ip = toml_cfg["general"]["net_if"]["ip"].value_or("172.16.0.128");
    tun.peers = std::vector<tunthread_peer>();

    const toml::array *peers = toml_cfg["general"]["net_if"]["peers"].as_array();

    for(auto it = peers->cbegin(); it != peers->cend(); it++)
    {
        tunthread_peer p;
        // WIP
        // *it["callsign"]

        // std::cout << "Adding peer with callsign " << it.
    }


    return EXIT_SUCCESS;    
}
