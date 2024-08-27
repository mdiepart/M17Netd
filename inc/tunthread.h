#pragma once

#include <atomic>
#include <cstdbool>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include "ConsumerProducer.h"
#include <memory>

#include "config.h"

class tunthread {
    public:
    void operator()(std::atomic_bool &running, const config &cfg,
                    ConsumerProducerQueue<std::shared_ptr<std::vector<uint8_t>>> &from_net);
};
