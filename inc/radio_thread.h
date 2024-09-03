#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <liquid/liquid.h>
#include "ConsumerProducer.h"

#include "m17tx.h"
#include "config.h"

using namespace std;

class radio_simplex {
    public:
    void operator()(std::atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<shared_ptr<m17tx>> &to_radio,
                    ConsumerProducerQueue<shared_ptr<vector<uint8_t>>> &from_radio);

    private:
    freqmod fmod;
    freqdem fdem;
};