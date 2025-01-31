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

#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

#include <cstdint>
#include <cstring>
#include <array>
#include "Correlator.hpp"
#include <liquid/liquid.h>
#include <m17.h>
//#include <iostream>

/**
 * Frame syncronizer class. It allows to find the best sampling point for a
 * baseband stream, given a syncword.
 */
template < size_t SYNCW_SIZE, size_t SAMPLES_PER_SYM >
class Synchronizer
{

private:
    static constexpr size_t SAMPLES_PER_SYNCW = SYNCW_SIZE * SAMPLES_PER_SYM;
public:

    /**
     * Constructor.
     *
     * @param sync_word: symbols of the target syncword.
     */
    Synchronizer(std::array< int8_t, SYNCW_SIZE >&& sync_word) :
        syncword(std::move(sync_word)), triggered(false), last_corr(0)
        {
            size_t nb_taps = sizeof(rrc_taps_20)/sizeof(rrc_taps_20[0]);
            float *taps = static_cast<float*>(malloc(nb_taps*sizeof(float)));
            memcpy(taps, rrc_taps_20, nb_taps*sizeof(float));

            firfilt_rrrf rrcos1 = firfilt_rrrf_create(taps, nb_taps);
            firfilt_rrrf rrcos2 = firfilt_rrrf_create(taps, nb_taps);

            size_t purge = nb_taps;
            float out = 0.0f; // Output of rrcos filters
            size_t output_index = 0;

            int64_t cumsum = 0;

            for(const auto sym: syncword)
            {
                firfilt_rrrf_execute_one(rrcos1, static_cast<float>(sym*5000), &out);
                firfilt_rrrf_execute_one(rrcos2, out, &out);

                // Upsampling
                for(size_t i = 0; i < SAMPLES_PER_SYM; i++)
                {
                    if(i > 0)
                    {
                        firfilt_rrrf_execute_one(rrcos1, 0, &out);
                        firfilt_rrrf_execute_one(rrcos2, out, &out);
                    }

                    // If we purged enough samples, store it
                    if(purge == 0)
                    {
                        filtered_syncw[output_index++] = static_cast<int16_t>(out); // Store and increment index
                        cumsum += out;
                    }
                    else
                    {
                        purge--; // One less sample to purge
                    }
                }
            }

            // Finish filling-up the filtered syncword buffer
            while(output_index < filtered_syncw.size())
            {
                firfilt_rrrf_execute_one(rrcos1, 0, &out);
                firfilt_rrrf_execute_one(rrcos2, out, &out);
                filtered_syncw[output_index++] = static_cast<int16_t>(out); // Store and increment index
                cumsum += out;
            }

            firfilt_rrrf_destroy(rrcos1);
            firfilt_rrrf_destroy(rrcos2);

            cumsum /= static_cast<int64_t>(filtered_syncw.size());

            for(auto &x: sync_word)
            {
                x -= cumsum;
            }

            /*cout << "Created synchronizer.\n";
            cout << "sync_word[" << sync_word.size() << "]={";
            for(auto &x: sync_word)
            {
                cout << static_cast<int32_t>(x) << ", ";
            }
            cout << "};\n";

            cout << "filtered_syncword[" << filtered_syncw.size() << "]={";

            for(auto &x: filtered_syncw)
            {
                cout << static_cast<int32_t>(x) << ", ";
            }
            cout << "};" << endl;*/

            free(taps);
        }

    /**
     * Destructor.
     */
    ~Synchronizer() { }

    /**
     * Perform an update step of the syncronizer.
     *
     * @param correlator: correlator object to be used to compute the convolution
     * product with the syncword.
     * @param posTh: threshold to detect a positive correlation peak.
     * @param negTh: threshold to detect a negative correlation peak.
     * @return +1 if a positive correlation peak has been found, -1 if a negative
     * correlation peak has been found an zero otherwise.
     */
    int8_t update(const Correlator< SYNCW_SIZE, SAMPLES_PER_SYM >& correlator,
                  const int32_t posTh, const int32_t negTh)
    {
        int32_t sign    = 0;
        int32_t corr    = correlator.full_convolve(filtered_syncw);
        bool    trigger = false;

        if( (corr > posTh) && (corr >= last_corr) )
        {
            trigger = true;
        }
        else if( (corr < negTh) && (corr <= last_corr) )
        {
            trigger = true;
        }

        last_corr = corr;

        if(trigger == true)
        {
            if(triggered == false)
            {
                values.fill(0);
                triggered = true;
            }

            values[correlator.index()] = corr;
        }
        else
        {
            if(triggered)
            {
                // Calculate the sampling index on the falling edge.
                triggered = false;
                sampIndex = 0;

                int32_t peak  = corr;
                uint8_t index = 0;

                // Find peak value
                for(auto val : values)
                {
                    if(std::abs(val) > std::abs(peak))
                    {
                        peak = val;
                        sampIndex = index;
                    }

                    index += 1;
                }

                if(peak >= 0)
                    sign = 1;
                else
                    sign = -1;
            }
        }

        return sign;
    }

    int32_t getLastCorr() const
    {
        return last_corr;
    }

    /**
     * Get the best sampling index equivalent to the last correlation peak
     * found. This value is meaningful only when the update() function returned
     * a value different from zero.
     *
     * @return the optimal sampling index.
     */
    size_t samplingIndex() const
    {
        return sampIndex;
    }

private:
    std::array< int16_t, (SAMPLES_PER_SYNCW-SAMPLES_PER_SYM+1) >    filtered_syncw; ///< Syncword filtered through rrcos twice
    std::array< int8_t, SYNCW_SIZE >                                syncword;       ///< Target syncword
    std::array< int32_t, SAMPLES_PER_SYNCW >                        values;         ///< Correlation history
    bool                                                            triggered;      ///< Peak found
    uint8_t                                                         sampIndex;      ///< Optimal sampling point
    int32_t                                                         last_corr;      ///< Value of the last corrlation computed
};

#endif
