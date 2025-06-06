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

#include <array>
#include <string>
#include <vector>
#include <cstdbool>

#include <m17.h>

using namespace std;

class m17rx
{
    public:
    m17rx();
    ~m17rx();
    m17rx(const m17rx &origin);
    m17rx(m17rx &&origin);
    m17rx& operator=(const m17rx &origin);
    m17rx& operator=(m17rx &&origin);

    /**
     * Append a frame to the packet
     *
     * @param sync_word the sync_word at the beginning of the frame (as packed bits)
     * @param frame an array of 2*192 (384) soft bits (16 bits unsigned integers, one per bit) including the sync_word
     *
     * @return 0 on success, -1 on error
     */
    int add_frame(uint16_t sync_word, array<uint16_t, 2*SYM_PER_FRA> frame);

    /**
     * Check if the frame received is valid
     *
     * @return true if the packet is complete and lsf is not corrupted, false otherwise
     */
    bool is_valid() const;

    /**
     * Check if the frame is complete
     *
     * @return true if the packet is complete (last frame received), 0 otherwise
     */
    bool is_complete() const;

    /**
     * Returns the number of corrected bits in the packet.
     * Can be called on packets even if they are empty, incomplete or in error condition.
     *
     * @return number of corrected bits
     */
    uint32_t corrected_bits() const;

    /**
     * Check if the packet is in error state (the packet cannot possibly be completed)
     *
     * @return true if the packet cannot possibly be completed (is in error state),
     *         false if the packet is complete or still being completed.
     */
    bool is_error() const;

    /**
     * Returns the LSF frame of the packet
     * This function performs no check on the returned LSF so the returned LSF may be corrupted.
     * user has to check if the packet is valid and/or if the LSF CRC matches the frame content.
     *
     * @return array containing the LSF frame.
     */
    array<uint8_t, 30> get_lsf() const;

    /**
     * Returns the payload of the packet
     *
     * @return vector containing the payload if the packet is valid, an empty packet otherwise
     */
    vector<uint8_t> get_payload() const;

    /**
     * Check if the current superframe is in BERT mode.
     *
     * @return true if the superframe is in BERT mode
     *         false otherwise
     */
    bool is_bert() const;

    /**
     * Returns the total number of bits received in BERT mode once synchronized
     *
     * @return number of bits received in BERT mode
     */
    size_t get_bert_totcnt() const;

    /**
     * Returns the number of error bits received in BERT mode once synchronized
     *
     * @return number of error bits received in BERT mode
     */
    size_t get_bert_errcnt() const;

    /**
     * Returns the sync status of the BERT RX register. This is only valid if the
     * superframe is in bert mode.
     *
     * @return true if the BERT receiver register is synchronized with the incoming stream
     *         false if the BERT receiver register is not synchronized with the incoming stream
     */
    bool is_bert_synced() const;

    private:
    static constexpr uint16_t SYNC_LSF = 0x55F7; /** LSF frame sync word */
    static constexpr uint16_t SYNC_PKT = 0x75FF; /** PKT frame sync word */
    static constexpr uint16_t SYNC_BER = 0xDF55; /** BERT frame sync word */

    enum class packet_status
    {
        EMPTY = 0,      /** Packet is empty */
        LSF_RECEIVED,   /** The LSF frame have been received and PKT frames may be added */
        PKT_COMPLETE,   /** The last PKT frame have been received */
        BERT,           /** Bit Error Rate Testing mode */
        ERROR           /** An error occured (such as a skipped frame number) */
    };

    packet_status       status;                 /** Current status of the packet */
    array<uint8_t, 30>  lsf;                    /** LSF frame content */
    vector<uint8_t>     *pkt_data;              /** raw type-1 bits from the successive packet frames */
    uint32_t            corrected_errors;       /** Number of corrected bits along the full frame */
    int                 received_pkt_frames;    /** Number of packet frames received */

    // BERT variables
    static constexpr unsigned bert_lockcnt = 18;    /** Number of correct bits to receive to lock BERT sync */
    uint16_t                  bert_lfsr;            /** LFSR used to test BER */
    size_t                    bert_errcnt;          /** BERT errors count */
    size_t                    bert_totcnt;          /** BERT total number of bits received */
    size_t                    bert_synccnt;         /** BERT Synchronization counter */
    unsigned __int128         bert_hist;            /** BERT history, a bit set to one means that it was incorrect */
};
