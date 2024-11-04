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
