#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include "ConsumerProducer.h"

using namespace std;

typedef struct
{
    unsigned long tx_freq;
    unsigned long rx_freq;
} radio_thread_cfg;

class radio_simplex {
    public:
    void operator()(std::atomic_bool &running, radio_thread_cfg &radio_cfg,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &fromNet,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &fromRadio);
};