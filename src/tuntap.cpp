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
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>


#include "tuntap.h"

TunDevice::TunDevice(const std::string &name)
{
    const char *dev = name.c_str();
    struct ifreq ifr;
    int err;

    // Open the clone device
    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
    {
        std::cerr << "Unable to open clone device for tuntap interface." << std::endl;
        return;
    }
       
    // Init structure memory
    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TUN;

    // if a name was given, copy it in the init structure
    if( *dev )
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }
       
    // Attempt to create the device
    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if( err < 0 ){
       close(fd);
       fd = -1;
       std::cerr << "Unable to create the tun device." << std::endl;
       return;
    }

    ifName = std::string(ifr.ifr_name);
    std::cout << "Created tun interface with fd = " << fd << " and name " << ifName << std::endl;
    return;
}

TunDevice::~TunDevice()
{
    close(fd);
}

std::vector<uint8_t> TunDevice::getPacket()
{    
    std::cout << "Not implemented yet." << std::endl;
    sleep(1);

    return std::vector<uint8_t>(5, 0);
}