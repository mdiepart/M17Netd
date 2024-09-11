/***************************************************************************
 *   Copyright (C) 2021 - 2024 by Federico Amedeo Izzo IU2NUO,             *
 *                                Niccolò Izzo IU2KIN                      *
 *                                Wojciech Kaczmarski SP5WWP               *
 *                                Frederik Saraci IU2NRO                   *
 *                                Silvano Seva IU2KWO                      *
 *                                Morgan Diepart ON4MOD                    *
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

#ifndef M17_DEMODULATOR_H
#define M17_DEMODULATOR_H

#ifndef __cplusplus
#error This header is C++ only!
#endif

#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>
#include <cmath>
#include <Correlator.hpp>
#include <Synchronizer.hpp>
#include <M17Utils.hpp>
#include <iir.hpp>
#include <liquid/liquid.h>

namespace M17
{

using m17frame_t   = std::array< uint16_t, 192*2 >; // Data type for a full M17 data frame, including sync word
using m17syncw_t   = std::array< uint8_t, 2  >; // Data type for a sync word

class M17Demodulator
{
public:

    /**
     * Constructor.
     */
    M17Demodulator();

    /**
     * Destructor.
     */
    ~M17Demodulator();

    /**
     * Allocate buffers for baseband signal sampling and initialise demodulator.
     */
    void init();

    /**
     * Shutdown modulator and deallocate data buffers.
     */
    void terminate();

    /**
     * Returns the a frame decoded from the baseband signal.
     *
     * @return reference to the internal data structure containing the last
     * decoded frame.
     */
    const m17frame_t& getFrame();

    /**
     * Demodulates data from the ADC and fills the idle frame.
     * Everytime this function is called a whole ADC buffer is consumed.
     *
     * @param invertPhase: invert the phase of the baseband signal before decoding.
     * @return true if a new frame has been fully decoded.
     */
    bool update(float *samples, size_t N);

    /**
     * @return true if a demodulator is locked on an M17 stream.
     */
    bool isLocked();

private:

    /**
     * Quantize a given sample to its corresponding symbol and append it to the
     * ongoing frame. When a frame is complete, it swaps the pointers and updates
     * newFrame variable.
     *
     * @param sample: baseband sample.
     * @return quantized symbol.
     */
    int8_t updateFrame(const int16_t sample);

    /**
     * Reset the demodulator state.
     */
    void reset();

    /**
     * M17 baseband signal sampled at 24kHz, half of an M17 frame is processed
     * at each update of the demodulator.
     */
    static constexpr size_t     M17_SYMBOL_RATE         = 4800;
    static constexpr size_t     M17_FRAME_SYMBOLS       = 192;
    static constexpr size_t     RX_SAMPLE_RATE          = 96000;
    static constexpr size_t     M17_SYNCWORD_SYMBOLS    = 8;
    static constexpr size_t     SAMPLES_PER_SYMBOL      = RX_SAMPLE_RATE / M17_SYMBOL_RATE;
    static constexpr size_t     FRAME_SAMPLES           = M17_FRAME_SYMBOLS * SAMPLES_PER_SYMBOL;
    static constexpr size_t     SAMPLE_BUF_SIZE         = FRAME_SAMPLES / 2;
    static constexpr size_t     SYNCWORD_SAMPLES        = SAMPLES_PER_SYMBOL * M17_SYNCWORD_SYMBOLS;
    static constexpr m17syncw_t LSF_SYNC_WORD           = {0x55, 0xF7};  // LSF sync word
    static constexpr m17syncw_t BERT_SYNC_WORD          = {0xDF, 0x55};  // BERT data sync word
    static constexpr m17syncw_t STREAM_SYNC_WORD        = {0xFF, 0x5D};  // Stream data sync word
    static constexpr m17syncw_t PACKET_SYNC_WORD        = {0x75, 0xFF};  // Packet data sync word
    static constexpr m17syncw_t EOT_SYNC_WORD           = {0x55, 0x5D};  // End of transmission sync word
    /**
     * Internal state of the demodulator.
     */
    enum class DemodState
    {
        INIT,       ///< Initializing
        UNLOCKED,   ///< Not locked
        SYNCED,     ///< Synchronized, validate syncword
        LOCKED,     ///< Locked
        SYNC_UPDATE ///< Updating the sampling point
    };

    /**
     * Internal enum representing the type of syncword last received
     */
    enum class SyncWord
    {
        NONE,
        LSF,
        PACKET,
    };

    /**
     * Cofficients of the sample filter
     */
    static constexpr std::array < float, 3 > sfNum = {4.24433681e-05f, 8.48867363e-05f, 4.24433681e-05f};
    static constexpr std::array < float, 3 > sfDen = {1.0f,           -1.98148851f,     0.98165828f};

    DemodState                     demodState;      ///< Demodulator state
    std::unique_ptr< int16_t[] >   baseband_buffer; ///< Buffer for baseband audio handling.
    std::unique_ptr<m17frame_t >   demodFrame;      ///< Frame being demodulated.
    std::unique_ptr<m17frame_t >   readyFrame;      ///< Fully demodulated frame to be returned.
    bool                           locked;          ///< A syncword was correctly demodulated.
    bool                           newFrame;        ///< A new frame has been fully decoded.
    uint16_t                       frameIndex;      ///< Index for filling the raw frame.
    uint32_t                       sampleIndex;     ///< Sample index, from 0 to (SAMPLES_PER_SYMBOL - 1)
    uint32_t                       samplingPoint;   ///< Symbol sampling point
    uint32_t                       sampleCount;     ///< Free-running sample counter
    uint8_t                        missedSyncs;     ///< Counter of missed synchronizations
    uint32_t                       initCount;       ///< Downcounter for initialization
    uint32_t                       syncCount;       ///< Downcounter for resynchronization
    std::pair < int32_t, int32_t > outerDeviation;  ///< Deviation of outer symbols
    float                          corrThreshold;   ///< Correlation threshold
    iirfilt_rrrf                   dcr;             ///< DC removal filter
    firfilt_rrrf                   rrcos_filt;      ///< Root-raised cosine filter for baseband signal
    SyncWord                       lastSyncWord;


    Correlator   < M17_SYNCWORD_SYMBOLS, SAMPLES_PER_SYMBOL > correlator;
    Synchronizer < M17_SYNCWORD_SYMBOLS, SAMPLES_PER_SYMBOL > lsfSync{{ +3, +3, +3, +3, -3, -3, +3, -3 }};
    Synchronizer < M17_SYNCWORD_SYMBOLS, SAMPLES_PER_SYMBOL > packetSync{{ +3, -3, +3, +3, -3, -3, -3, -3 }};
    Iir          < 3 >                                        sampleFilter{sfNum, sfDen};
};

} /* M17 */

#endif /* M17_DEMODULATOR_H */
