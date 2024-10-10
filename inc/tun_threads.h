#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <vector>
#include <memory>

#include "ConsumerProducer.h"
#include "config.h"
#include "m17rx.h"

using namespace std;

class tun_thread {
    public:
    void operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_net,
                    ConsumerProducerQueue<shared_ptr<m17rx>> &to_net);
};
