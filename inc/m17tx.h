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

#include <string_view>
#include <vector>
#include <array>
#include <m17.h>

using namespace std;

class m17tx
{
private:
    static constexpr size_t N = 20; // Interpolation factor
    static constexpr size_t nb_taps = 161; // Taps in RRC filter

    vector<float> *symbols;
    static const array<float, nb_taps> taps;
    size_t bb_samples;
    size_t bb_idx;
    array<float, (nb_taps+N)> filt_buff;
    size_t filt_offset;

public:
    m17tx(const string_view &src, const string_view &dst, const shared_ptr<vector<uint8_t>> ip_pkt);
    ~m17tx();

    vector<float> get_baseband_samples(size_t n);
    vector<float> get_symbols() const;
    size_t baseband_samples_left() const;
};
