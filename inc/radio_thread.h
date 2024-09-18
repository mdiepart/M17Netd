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
                    ConsumerProducerQueue<shared_ptr<m17rx>> &from_radio);

    private:
    static constexpr size_t block_size  = 960; /** Samples block size, 10ms of baseband at 96000 kSps */
    static constexpr size_t fft_size    = 512; /** Block size to compute the FFT to assess channel occupency */
    freqmod fmod;
    freqdem fdem;
};