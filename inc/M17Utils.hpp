/***************************************************************************
 *   Copyright (C) 2021 - 2023 by Federico Amedeo Izzo IU2NUO,             *
 *                                Niccolò Izzo IU2KIN                      *
 *                                Frederik Saraci IU2NRO                   *
 *                                Silvano Seva IU2KWO                      *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef M17_UTILS_H
#define M17_UTILS_H

#ifndef __cplusplus
#error This header is C++ only!
#endif

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <array>
#include <assert.h>

namespace M17
{

/**
 * Utility function allowing to retrieve the value of a single bit from an array
 * of bytes. Bits are counted scanning from left to right, thus bit number zero
 * is the leftmost bit of array[0].
 *
 * @param array: byte array.
 * @param pos: bit position inside the array.
 * @return value of the indexed bit, as boolean variable.
 */
template < size_t N >
inline bool getBit(const std::array< uint8_t, N >& array, const size_t pos)
{
    size_t i = pos / 8;
    size_t j = pos % 8;
    return (array[i] >> (7 - j)) & 0x01;
}


/**
 * Utility function allowing to set the value of a single bit from an array
 * of bytes. Bits are counted scanning from left to right, thus bit number zero
 * is the leftmost bit of array[0].
 *
 * @param array: byte array.
 * @param pos: bit position inside the array.
 * @param bit: bit value to be set.
 */
template < size_t N >
inline void setBit(std::array< uint8_t, N >& array, const size_t pos,
                   const bool bit)
{
    size_t i     = pos / 8;
    size_t j     = pos % 8;
    uint8_t mask = 1 << (7 - j);
    array[i] = (array[i] & ~mask) |
               (bit ? mask : 0x00);
}


/**
 * Compute the hamming distance between two bytes.
 *
 * @param x: first byte.
 * @param y: second byte.
 * @return hamming distance between x and y.
 */
static inline uint8_t hammingDistance(const uint8_t x, const uint8_t y)
{
    return __builtin_popcount(x ^ y);
}

static inline float softHammingDistance(const size_t length, const uint16_t *a, const uint16_t *b)
{
    uint32_t accum = 0;

    for(size_t i = 0; i < length; i++)
    {
        accum += std::abs(static_cast<int32_t>(a[i]) - b[i]);
    }

    return static_cast<float>(accum)/static_cast<float>(UINT16_MAX);
}

/**
 * Utility function allowing to set the value of a symbol on an array
 * of bytes. Symbols are packed putting the most significant bit first,
 * symbols are filled from the least significant bit pair to the most
 * significant bit pair.
 *
 * @param array: byte array.
 * @param pos: symbol position inside the array.
 * @param symbol: symbol to be set, either -3, -1, +1, +3.
 */
template < size_t N >
inline void setSymbol(std::array< uint8_t, N >& array, const size_t pos,
                      const int8_t symbol)
{
    switch(symbol)
    {
        case +3:
            setBit<N> (array, 2 * pos    , 0);
            setBit<N> (array, 2 * pos + 1, 1);
            break;
        case +1:
            setBit<N> (array, 2 * pos    , 0);
            setBit<N> (array, 2 * pos + 1, 0);
            break;
        case -1:
            setBit<N> (array, 2 * pos    , 1);
            setBit<N> (array, 2 * pos + 1, 0);
            break;
        case -3:
            setBit<N> (array, 2 * pos    , 1);
            setBit<N> (array, 2 * pos + 1, 1);
            break;
        default:
            assert("Error: unknown M17 symbol!");
    }
}


/**
 * Utility function to encode a given byte of data into 4FSK symbols. Each
 * byte is encoded in four symbols.
 *
 * @param value: value to be encoded in 4FSK symbols.
 * @return std::array containing the four symbols obtained by 4FSK encoding.
 */
inline std::array< int8_t, 4 > byteToSymbols(uint8_t value)
{
    static constexpr int8_t LUT[] = { +1, +3, -1, -3};
    std::array< int8_t, 4 > symbols;

    symbols[3] = LUT[value & 0x03];
    value >>= 2;
    symbols[2] = LUT[value & 0x03];
    value >>= 2;
    symbols[1] = LUT[value & 0x03];
    value >>= 2;
    symbols[0] = LUT[value & 0x03];

    return symbols;
}

}      // namespace M17


class dc_remover
{
    private:
    static constexpr float alpha = 0.999f;
    bool initialized;
    float x_prev;
    float y_prev;

    public:
    dc_remover()
    {
        reset();
    }

    void reset()
    {
        x_prev = 0.0f;
        y_prev = 0.0f;
        initialized = false;
    }

    void process_samples(int16_t *samples, std::size_t length)
    {
        if(length < 2)
            return;

        std::size_t pos = 0;

        if(!initialized)
        {
            x_prev = static_cast<float>(samples[0]);
            initialized = true;
            pos = 1;
        }

        for(; pos < length; pos++)
        {
            float x = static_cast<float>(samples[pos]);
            float y = x - x_prev + alpha * y_prev;

            x_prev = x;
            y_prev = y;
            samples[pos] = static_cast<int16_t>(y + 0.5f);
        }
    }


};

/**
 * @brief Map a value from input range to output range.
 * @param input Value to map
 * @param inLow input range lower bound
 * @param inHigh input range upper bound
 * @param outLow output range lower bound
 * @param outHigh output range higher bound
 * @return input value mapped to the output range
 */
static inline float mapRange(float input, float inLow, float inHigh, float outLow, float outHigh)
{
    return outLow + ((input-inLow)/(inHigh-inLow))*(outHigh-outLow);
}
#endif // M17_UTILS_H