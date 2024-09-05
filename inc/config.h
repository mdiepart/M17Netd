#pragma once

#include <toml.hpp>
#include <string>

using namespace std;

typedef struct
{
    std::string_view callsign;
    std::string_view ip;
    std::vector<std::string_view> routes;
} peer_t;

typedef struct
{
    std::string_view    name;
    std::string_view    ip;
    std::size_t         mtu;
    std::vector<peer_t> peers;
} tunthread_cfg;

typedef struct
{
    unsigned long tx_freq; /* TX Frequency */
    unsigned long rx_freq; /* RX Frequency */
    float         k;       /* FM Modulation index */
} radio_thread_cfg;

class config
{
    public:
    config(const string_view &file);
    int getTunConfig(tunthread_cfg &tun_cfg) const;
    int getRadioConfig(radio_thread_cfg &radio_cfg) const;
    vector<peer_t> getPeers() const;
    string_view getCallsign() const;
    size_t getTxQueueSize() const;
    size_t getRxQueueSize() const;

    private:
    toml::table config_tbl;
};