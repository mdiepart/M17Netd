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
#include <sys/types.h>
#include <net/route.h>


#include "tuntap.h"

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

TunDevice::TunDevice(const std::string_view name)
{
    const char *dev = name.data();

    struct ifreq ifr;
    int err;

    // Open the clone device
    if( (tun_fd = open("/dev/net/tun", O_RDWR)) < 0 )
    {
        std::cerr << "Unable to open clone device for tuntap interface: error " << errno << std::endl;
        return;
    }
       
    // Init structure memory
    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TUN | IFF_NO_CARRIER;

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

std::vector<uint8_t> TunDevice::getPacket(std::atomic<bool> &running)
{    
    uint8_t storage[mtu + 32];
    struct timespec read_timeout = {.tv_sec = 1, .tv_nsec = 0}; // 1 sec timeout
    bool loop = true;
    std::size_t n = 0;

    fd_set read_fdset;
    FD_ZERO(&read_fdset);
    FD_SET(tun_fd, &read_fdset);

    while(loop && running)
    {
        int ret = pselect(1, &read_fdset, nullptr, nullptr, &read_timeout, 0);
        if(ret < 0){
            std::cout << "Tun thread: pselect error, errno=" << errno << std::endl;
            loop = false;
        }
        else if(ret == 0)
        {
            // Pselect timed out, retry
            continue;
        }
        else if(ret == 1)
        {
            n = read(sock_fd, storage, mtu + 32);
            loop = false;
        }
    }

    if(n <= 0)
    {
        return std::vector<uint8_t>();
    }
    else
    {
        return std::vector<uint8_t>(storage, storage + n);
    }
    
}

void TunDevice::setIPV4(std::string_view ip)
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
    sai.sin_addr.s_addr = inet_addr(ip.data());

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

void TunDevice::setMTU(int size)
{
    int err;
    mtu = size;

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

void TunDevice::addRoute(std::string_view route)
{
    struct rtentry rt;
}