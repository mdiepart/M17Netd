/****************************************************************************
 * M17Netd                                                                  *
 * Copyright (C) 2024 by Morgan Diepart ON4MOD                              *
 *                       SDR-Engineering SRL                                *
 *                                                                          *
 * This program is free software: you can redistribute it and/or modify     *
 * it under the terms of the GNU Affero General Public License as published *
 * by the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                      *
 *                                                                          *
 * This program is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 * GNU Affero General Public License for more details.                      *
 *                                                                          *
 * You should have received a copy of the GNU Affero General Public License *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ****************************************************************************/

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
    static constexpr size_t block_size  = 128; /** Samples block size, 1.3ms of baseband at 96000 kSps */
    static constexpr size_t fft_size    = 128; /** Block size to compute the FFT to assess channel occupency */
    /** Half the bandwidth of the expected signal in terms of FFT bins */
    static constexpr size_t half_chan_width = (9000*fft_size/96000);
    freqmod fmod;
    freqdem fdem;
};