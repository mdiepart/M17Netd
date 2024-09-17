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

class tun_thread_read {
    public:
    void operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_net);
};

class tun_thread_write {
    public:
    void operator()(atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<m17rx>> &to_net);
};
