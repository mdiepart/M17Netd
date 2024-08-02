#include <iostream>
#include <thread>
#include <atomic>

#include "signal.h"

#include "tunthread.h"

std::atomic<bool> running;

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
    std::cout << "Starting M17Netd" << std::endl;
    running = true;

    signal(SIGINT, sigint_catcher);

    std::thread tuntap = std::thread(tunthread(), std::ref(running), std::string("m17d%d"));

    tuntap.join();
    std::cout << "tuntap thread stopped" << std::endl;

    return EXIT_SUCCESS;
}

