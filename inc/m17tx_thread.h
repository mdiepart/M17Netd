#pragma once

#include <atomic>
#include <cstdbool>
#include <vector>
#include <memory>

#include "ConsumerProducer.h"
#include "tunthread.h"
#include "config.h"
using namespace std;

class m17tx_thread
{
    public:
    void operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &fromNet,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &toRadio);
};