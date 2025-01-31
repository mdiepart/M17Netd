/***************************************************************************
 *   Copyright (C) 2024 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
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
 **************************************************************************/

#ifndef CORRELATOR_H
#define CORRELATOR_H

#include <cstdint>
#include <array>

using namespace std;

/**
 * Class to construct correlator objects, allowing to compute the cross-correlation
 * between a stream of signed 16-bit samples and a known syncword.
 * The correlator has its internal storage for past samples.
 */
template < size_t SYNCW_SIZE, size_t SAMPLES_PER_SYM >
class Correlator
{
private:
    static constexpr size_t ADDITIONAL_STORAGE = SAMPLES_PER_SYM;
    static constexpr size_t SYNCWORD_SAMPLES = (SYNCW_SIZE-1) * SAMPLES_PER_SYM + 1;
    static constexpr size_t BUFFER_SIZE = SYNCWORD_SAMPLES + ADDITIONAL_STORAGE;

    int16_t samples[BUFFER_SIZE];       ///< Samples' storage (circular buffer)
    size_t  sampIdx;                    ///< Index of the next sample to write
    size_t  prevIdx;                    ///< Index of the last written sample

public:

    /**
     * Constructor.
     */
    Correlator() : sampIdx(0) { }

    /**
     * Destructor.
     */
    ~Correlator() { }

    /**
     * Append a new sample to the correlator memory.
     *
     * @param sample: baseband sample.
     */
    void sample(const int16_t sample)
    {
        samples[sampIdx] = sample;
        prevIdx = sampIdx;
        sampIdx = (sampIdx + 1) % BUFFER_SIZE;
    }

    /**
     * Compute a fast convolution product between the samples stored in the correlator
     * memory and a target syncword. This convolution product computes only the correlation
     * using one sample every SAMPLES_PER_SYM symbols.
     *
     * @param syncword: syncword symbols.
     * @return convolution product.
     */
    int32_t convolve(const std::array< int8_t, SYNCW_SIZE >& syncword) const
    {
        int32_t conv = 0;
        size_t  pos  = prevIdx + ADDITIONAL_STORAGE + 1;

        for(auto& sym : syncword)
        {
            conv += (int32_t) sym * (int32_t) samples[pos % BUFFER_SIZE];
            pos  += SAMPLES_PER_SYM;
        }

        return conv;
    }

    /**
     * Compute a complete convolution product between the samples stored in the correlator
     * memory and a target syncword. This convolution product computes the correlation using
     * all samples available.
     *
     * @param syncword: syncword symbols.
     * @return convolution product.
     */
    int32_t full_convolve(const std::array<int16_t, SYNCWORD_SAMPLES> &array) const
    {
        int64_t conv = 0;
        size_t pos = prevIdx + ADDITIONAL_STORAGE + 1;

        for(auto &x : array)
        {
            pos %= BUFFER_SIZE;
            conv += ( static_cast<int64_t>(x) * samples[pos++] );
        }

        return (conv>>13);
    }

    /**
     * Return the maximum deviation of the samples stored in the correlator
     * memory, starting from a given sampling point. When the sampling point
     * corresponds to a peak of correlation, this function allows to retrieve
     * the outer deviation of a given baseband stream, provided that the target
     * syncword is composed only by outer symbols. This is true in case the
     * syncword is constructed using Barker codes.
     *
     * @param samplePoint: sampling point.
     * @return a std::pair carrying the maximum deviation. First element is
     * positive deviation, second element is negative deviation.
     */
    std::pair< int32_t, int32_t > maxDeviation(const size_t samplePoint) const
    {
        int32_t maxSum = 0;
        int32_t minSum = 0;
        int32_t maxCnt = 0;
        int32_t minCnt = 0;

        for(ssize_t i = -SYNCWORD_SAMPLES+1; i <= 0; i += SAMPLES_PER_SYM)
        {
            // Compute the index of the sample in the circular buffer
            ssize_t pos = (samplePoint + i + BUFFER_SIZE) % BUFFER_SIZE;

            int16_t sample = samples[pos];
            if(sample > 0)
            {
                maxSum += sample;
                maxCnt += 1;
            }
            else if(sample < 0)
            {
                minSum += sample;
                minCnt += 1;
            }
        }

        if((maxCnt == 0) || (minCnt == 0))
            return std::make_pair(0, 0);

        return std::make_pair(maxSum/maxCnt, minSum/minCnt);
    }

    /**
     * Access the internal sample memory.
     *
     * @return a pointer to the correlator memory.
     */
    const int16_t *data() const
    {
        return samples;
    }

    /**
     * Get the buffer index at which the last sample has been written. The index
     * goes from zero to (SYNCW_SIZE * SAMPLES_PER_SYM) + 1
     *
     * @return index of the last stored sample.
     */
    size_t index() const
    {
        return prevIdx;
    }

    /**
     * Get the index at which the last sample has been written, modulo the
     * number of samples a symbol is made of.
     *
     * @return index of the last stored sample.
     */
    size_t sampleIndex() const
    {
        return prevIdx % SAMPLES_PER_SYM;
    }

    /**
     * Return the number of samples in the past samples storage
     *
     * @return number of samples *data() contains
     */
    size_t bufferSize() const
    {
        return BUFFER_SIZE;
    }

};

#endif
