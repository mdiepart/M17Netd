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
#include <memory>

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

#include "tunthread.h"
#include "tuntap.h"

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

TunDevice::TunDevice(const std::string_view &name)
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
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

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

std::shared_ptr<std::vector<uint8_t>> TunDevice::getPacket(std::atomic<bool> &running)
{    
    uint8_t storage[mtu];

    struct timespec read_timeout = {.tv_sec = 1, .tv_nsec = 0}; // 1 sec timeout
    bool loop = true;
    std::size_t n = 0;
    fd_set read_fdset;

    while(loop && running)
    {
        FD_ZERO(&read_fdset);
        FD_SET(tun_fd, &read_fdset);
        
        int ret = pselect(tun_fd+1, &read_fdset, nullptr, nullptr, &read_timeout, nullptr);
        if(ret < 0)
        {
            std::cout << "Tun thread: pselect error (" << strerror(errno) << ")." << std::endl;
            loop = false;
        }
        else if(ret == 0)
        {
            // Pselect timed out, retry
            continue;            
        }
        else
        {
            // tun_fd is the only filedescriptor monitored so if we arrive 
            // here it will always be set, no need to check.
            n = read(tun_fd, storage, mtu);
            loop = false;
        }
    }

    if(n <= 0)
    {
        return std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>());
    }
    else
    {
        return std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>(storage, storage+n));
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
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING | IFF_NO_CARRIER);
    else
        ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING | IFF_NO_CARRIER);

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


int TunDevice::addRoutesForPeer(const peer_t &peer)
{
    struct rtentry rt;
    struct sockaddr_in dst;
    struct sockaddr_in gw;
    struct sockaddr_in netmask;
    int ret;

    memset(&rt, 0, sizeof(struct rtentry));
    memset(&dst, 0, sizeof(struct sockaddr));
    memset(&gw, 0, sizeof(struct sockaddr));
    memset(&netmask, 0, sizeof(struct sockaddr));
    
    dst.sin_family = AF_INET;
    gw.sin_family = AF_INET;
    netmask.sin_family = AF_INET;

    // Device name
    char if_name[IFNAMSIZ] = {0};
    strncpy(if_name, ifName.c_str(), IFNAMSIZ);
    rt.rt_dev = if_name;

    std::cout << "Adding routes for peer " << peer.callsign << std::endl;

    // Add route to peer first (as host route)
    gw.sin_addr.s_addr = inet_addr(peer.ip.data());
    memcpy(&rt.rt_dst, &gw, sizeof(struct sockaddr));
    rt.rt_flags |= RTF_HOST;

    // Syscall to add host
    ret = ioctl(sock_fd, SIOCADDRT, &rt);
    if(ret < 0)
    {
        std::cerr << "Unable to add route to peer as host. ioctl error (" << strerror(errno) << ")." << std::endl;
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "Added route to peer as host." << std::endl;
    }

    rt.rt_flags &= ~(RTF_HOST);
    rt.rt_flags |= RTF_GATEWAY;

    // Add all other routes that use this peer as gateway
    memcpy(&rt.rt_gateway, &gw, sizeof(struct sockaddr));
    for(auto const &r : peer.routes)
    {
        // Parse the CIDR netmask length
        std::size_t idx = r.find_first_of('/');
        
        if(idx == std::string::npos){ 
            std::cerr << "No netmask length specified for route " << r << std::endl;
            return EXIT_FAILURE;
        }

        // Check that mask is not malformed
        std::string_view mask = r.substr(idx+1);
        int cidr_mask = 0;
        try
        {
            cidr_mask = std::stoi(mask.data());
        }
        catch(const std::out_of_range& e)
        {
            std::cerr << "Invalid CIDR mask in route " << r << std::endl;
            return EXIT_FAILURE;
        }

        if(cidr_mask < 0 || cidr_mask > 32)
        {
            std::cerr << "Invalid CIDR mask in route " << r << std::endl;
            return EXIT_FAILURE;    
        }

        // Convert from cidr to proper net mask
        uint32_t bin_mask = 0xFFFFFFFF << (32-cidr_mask);
        netmask.sin_addr.s_addr = htonl(bin_mask);
        memcpy(&rt.rt_genmask, &netmask, sizeof(struct sockaddr));

        // Add route as dst
        dst.sin_addr.s_addr = inet_addr(std::string(r.substr(0, idx)).c_str());
        dst.sin_family = AF_INET;
        memcpy(&rt.rt_dst, &dst, sizeof(struct sockaddr));

        std::cout << "Adding route " << r << " to " << ifName << " routes." << std::endl;
    
        int ret = ioctl(sock_fd, SIOCADDRT, &rt);
        if(ret < 0)
        {
            std::cerr << "Could not add route \"" << r 
            << "\": ioctl error (" << strerror(errno) << ")." << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}