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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "tuntap.h"

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

TunDevice::TunDevice(const std::string &name)
{
    const char *dev = name.c_str();
    struct ifreq ifr;
    int err;

    // Open the clone device
    if( (tun_fd = open("/dev/net/tun", O_RDWR)) < 0 )
    {
        std::cerr << "Unable to open clone device for tuntap interface: error " << tun_fd << std::endl;
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
    err = ioctl(tun_fd, TUNSETIFF, (void *) &ifr);
    if( err < 0 ){
       close(tun_fd);
       tun_fd = -1;
       std::cerr << "Unable to create the tun device." << std::endl;
       return;
    }

    // Store interface name
    ifName = std::string(ifr.ifr_name);

    // Open socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0)
    {
        std::cout << "Could not create socket: error " << sock_fd << std::endl;
        close(tun_fd);
    }

    std::cout << "Created tun interface with fd = " << tun_fd << " and name " << ifName << "\r\n";
    std::cout << "Created a socket with fd = " << sock_fd << std::endl;
    return;
}

TunDevice::~TunDevice()
{
    close(tun_fd);
    close(sock_fd);
}

std::vector<uint8_t> TunDevice::getPacket()
{    
    std::cout << "Not implemented yet." << std::endl;
    sleep(1);

    return std::vector<uint8_t>(5, 0);
}

void TunDevice::setIPV4(const char *ip)
{   
    int err;

    // Init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ);

    // Convert IP from string with dot notation to binary in network order
    struct sockaddr_in sai;
    memset(&sai, 0, sizeof(struct sockaddr));
    sai.sin_family = AF_INET;
    sai.sin_port = htons(0);
    sai.sin_addr.s_addr = inet_addr(ip);

    // Copy ip to ifr struct    
    memcpy(&ifr.ifr_addr, &sai, sizeof(struct sockaddr));

    // Set ip addr
    err = ioctl(sock_fd, SIOCSIFADDR, &ifr);
    if(err < 0)
        std::cout << "setIPV4: ioctl failed, errno=" << errno << std::endl;
}

void TunDevice::setUpDown(bool up)
{
    int err;

    // init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ);

    // Get flags
    ioctl(sock_fd, SIOCGIFFLAGS, (void *)&ifr);
    
    if(up)
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    else
        ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);

    err = ioctl(sock_fd, SIOCSIFFLAGS, (void *)&ifr); // Apply new up/down status

    if(err < 0)
    {
        std::cout << "Could not set interface " << ifName << " Up/Down: errno=" << errno << std::endl;
    }
    else
    {
        std::cout << "Set interface interface " << ifName << (up?" Up":" Down") << std::endl;
    }
}

void TunDevice::setMTU(int mtu)
{
    int err;

    // init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ);
    ifr.ifr_mtu = mtu;

    // Apply new MTU
    err = ioctl(sock_fd, SIOCSIFMTU, (void *)&ifr);

    if(err < 0)
    {
        std::cout << "Could not set MTU of interface " << ifName << ": errno=" << errno << std::endl;
    }
}