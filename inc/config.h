#pragma once

#include <toml.hpp>
#include <string>

#include "sx1255.h"

using namespace std;

typedef struct
{
    string_view callsign;
    string_view ip;
    vector<string_view> routes;
} peer_t;

typedef struct
{
    string_view    name;
    string_view    ip;
    size_t         mtu;
    vector<peer_t> peers;
} tunthread_cfg;

typedef struct
{
    string_view             spi_dev;    /* Path to the SPI device */
    string_view             i2s_tx;     /* Name of the i2s TX device */
    string_view             i2s_rx;     /* Name of the i2s RX device */
    sx1255_drv::lna_gain    lna_gain;   /* LNA gain (-48/-36/-24/-12/-6/max) */
    unsigned                mix_gain;   /* TX mixer gain (0 -> 15) */
} sdrnode_cfg;


typedef struct
{
    string_view   device;  /* Radio device to use */
    unsigned long tx_freq; /* TX Frequency */
    unsigned long rx_freq; /* RX Frequency */
    float         k;       /* FM Modulation index */
    float         ppm;     /* Frequency correction in ppm */
} radio_thread_cfg;

class config
{
    public:
    config(const string_view &file);
    int getTunConfig(tunthread_cfg &tun_cfg) const;
    int getRadioConfig(radio_thread_cfg &radio_cfg) const;
    int getSDRNodeConfig(sdrnode_cfg &cfg) const;
    vector<peer_t> getPeers() const;
    string_view getCallsign() const;
    size_t getTxQueueSize() const;
    size_t getRxQueueSize() const;

    private:
    toml::table config_tbl;
};