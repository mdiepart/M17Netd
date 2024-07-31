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
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/ioctl.h>
//#include <linux/string.h>
#include <linux/kernelcapi.h>

#include "tuntap.h"

TunDevice::TunDevice(std::string name)
{
    const char *dev = name.c_str();
    struct ifreq ifr;
    int err;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
    {
        return;
    }
       

    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TUN;
    if( *dev )
       strscpy_pad(ifr.ifr_name, dev, IFNAMSIZ);
    
    err = ioctl(fd, TUNSETIFF, (void *) &ifr)
    if( err < 0 ){
       close(fd);
       fd = -1;
       return;
    }
    strcpy(dev, ifr.ifr_name);
    return;
}




int tun_alloc(const char *dev)
{

}