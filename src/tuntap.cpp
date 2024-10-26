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
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/route.h>

#include "tuntap.h"
#include "config.h"

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

tun_device::tun_device(const std::string_view &name)
{
    const char *dev = name.data();

    struct ifreq ifr;
    int err;

    // Open the clone device in non-block mode
    if( (tun_fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0 )
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
    if_name = std::string(ifr.ifr_name);

    // Open socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0)
    {
        std::cout << "Could not create socket: error " << sock_fd << std::endl;
        close(tun_fd);
    }

    std::cout << "Created tun interface with fd = " << tun_fd << " and name " << if_name << "\r\n";
    std::cout << "Created a socket with fd = " << sock_fd << std::endl;
    return;
}

tun_device::~tun_device()
{
    close(tun_fd);
    close(sock_fd);
}

std::shared_ptr<std::vector<uint8_t>> tun_device::get_packet()
{
    uint8_t storage[mtu];

    // fd is opened in non-block. If there is no packet to be read, -1 will be returned
    // and errno will be EAGAIN
    int n = pread(tun_fd, storage, mtu, 0);

    if(n <= 0)
    {
        return nullptr;
    }
    else
    {
        return std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>(storage, storage+n));
    }
}

int tun_device::send_packet(const std::vector<uint8_t> &pkt)
{
    int written = pwrite(tun_fd, pkt.data(), pkt.size(), 0);

    if(written < 0)
        return -1;
    else if(static_cast<size_t>(written) != pkt.size())
        return -1;

    return 0;
}

void tun_device::set_IPV4(std::string_view ip)
{
    int err;

    // Init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ);

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

void tun_device::set_up_down(bool up)
{
    int err;

    // init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ);

    // Get flags
    ioctl(sock_fd, SIOCGIFFLAGS, (void *)&ifr);

    if(up)
    {
#ifdef IFF_NO_CARRIER
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING | IFF_NO_CARRIER);
#else
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
#endif
    }
    else
    {
#ifdef IFF_NO_CARRIER
        ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING | IFF_NO_CARRIER);
#else
        ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
#endif
    }
    err = ioctl(sock_fd, SIOCSIFFLAGS, (void *)&ifr); // Apply new up/down status

    if(err < 0)
    {
        std::cout << "Could not set interface " << if_name << " Up/Down: errno=" << errno << std::endl;
    }
    else
    {
        std::cout << "Set interface interface " << if_name << (up?" Up":" Down") << std::endl;
    }
}

void tun_device::set_MTU(int size)
{
    int err;
    mtu = size;

    // init ifr struct
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ);
    ifr.ifr_mtu = mtu;

    // Apply new MTU
    err = ioctl(sock_fd, SIOCSIFMTU, (void *)&ifr);

    if(err < 0)
    {
        std::cout << "Could not set MTU of interface " << if_name << ": errno=" << errno << std::endl;
    }
}

int tun_device::add_routes_to_peer(const peer_t &peer)
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
    char if_name_cstr[IFNAMSIZ+1] = {0};
    strncpy(if_name_cstr, if_name.c_str(), IFNAMSIZ);
    rt.rt_dev = if_name_cstr;

    // Add route to peer first (as host route)
    gw.sin_addr.s_addr = inet_addr(peer.ip.data());
    memcpy(&rt.rt_dst, &gw, sizeof(struct sockaddr));
    rt.rt_flags |= RTF_HOST;

    // Syscall to add host
    ret = ioctl(sock_fd, SIOCADDRT, &rt);
    if(ret < 0)
    {
        std::cerr << "Unable to add route to peer " << peer.callsign << " as host. ioctl error (" << strerror(errno) << ")." << std::endl;
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "Added route to peer " << peer.callsign << " as host." << std::endl;
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
        dst.sin_addr.s_addr &= htonl(bin_mask);
        dst.sin_family = AF_INET;
        memcpy(&rt.rt_dst, &dst, sizeof(struct sockaddr));

        std::cout << "Adding route " << r << " to " << if_name << " routes." << std::endl;

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

std::string tun_device::get_if_name() const
{
    return if_name;
}

int tun_device::get_tun_fd() const
{
    return tun_fd;
}

int tun_device::get_sock_fd() const
{
    return sock_fd;
}

