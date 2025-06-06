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

#include <array>
#include <iostream>
#include <m17.h>
#include <cstring>

#include "m17rx.h"

using namespace std;

m17rx::m17rx(): status(packet_status::EMPTY), lsf(), corrected_errors(0), received_pkt_frames(-1), bert_lfsr(1), bert_errcnt(0), bert_totcnt(0), bert_synccnt(bert_lockcnt), bert_hist(0)
{
    pkt_data = new vector<uint8_t>();
    pkt_data->reserve(25); // Reserve space for at least one frame
}

m17rx::~m17rx()
{
    delete(pkt_data);
}

m17rx::m17rx(const m17rx &origin): status(origin.status), corrected_errors(origin.corrected_errors), received_pkt_frames(origin.received_pkt_frames)
{
    lsf = array<uint8_t, 30>(origin.lsf);
    pkt_data = new vector<uint8_t>(*(origin.pkt_data));
}

m17rx::m17rx(m17rx &&origin): status(origin.status), lsf(move(origin.lsf)), pkt_data(move(origin.pkt_data)),
                              corrected_errors(origin.corrected_errors), received_pkt_frames(origin.received_pkt_frames)
{}

m17rx& m17rx::operator=(const m17rx &origin)
{
    status = origin.status;
    lsf = array<uint8_t, 30>(origin.lsf);
    pkt_data = new vector<uint8_t>(*(origin.pkt_data));
    corrected_errors = origin.corrected_errors;
    received_pkt_frames = origin.received_pkt_frames;

    return *this;
}

m17rx& m17rx::operator=(m17rx &&origin)
{
    status = origin.status;
    lsf = move(origin.lsf);
    pkt_data = move(origin.pkt_data);
    corrected_errors = origin.corrected_errors;
    received_pkt_frames = origin.received_pkt_frames;

    return *this;
}

int m17rx::add_frame(uint16_t sync_word, array<uint16_t, 2*SYM_PER_FRA> frame)
{
    // Check if the packet is ready to receive a frame
    if(status == packet_status::PKT_COMPLETE)
    {
        cerr << "m17rx: cannot add another frame to a completed packet." << endl;
        return -1;
    }

    if(status == packet_status::ERROR)
    {
        cerr << "m17rx: cannot add another frame to a packet in error state." << endl;
        return -1;
    }

    if(sync_word == SYNC_LSF && status != packet_status::EMPTY)
    {
        cerr << "m17rx: can only add an LSF frame to an empty packet." << endl;
        return -1;
    }

    if(sync_word == SYNC_PKT && status != packet_status::LSF_RECEIVED)
    {
        cerr << "m17rx: can only add PKT frames once an LSF frame have been added." << endl;
        return -1;
    }

    if(sync_word == SYNC_BER && (status != packet_status::BERT && status != packet_status::EMPTY))
    {
        cerr << "m17rx: packet not ready to receive BERT frames." << endl;
        return -1;
    }

    array<uint16_t, 2*SYM_PER_PLD> deinterleaved;

    uint16_t *payload = &(frame[16]);

    // De-randomize the last 368 bits
    randomize_soft_bits(payload);

    // de-interleave bits
    reorder_soft_bits(deinterleaved.data(), payload);


    // Decode the frame based on the syncword
    array<uint8_t, 31> buffer; // Buffer to receive the decoded data because somehow,
                               // somebody decided that the output buffer passed as an
                               // argument to viterbi decoding functions needed to be one
                               // byte longer than the actual output data, so this buffer
                               // is 31 bytes for a max length of 30

    switch(sync_word)
    {
        case SYNC_LSF:
        {
            cout << "Adding LSF frame to packet" << endl;
            status = packet_status::LSF_RECEIVED;
            // Decode 368 type-3 soft bits to type-1 bits, stored in the lsf array
            corrected_errors += viterbi_decode_punctured(buffer.data(), deinterleaved.data(),
                                    puncture_pattern_1, deinterleaved.size(), sizeof(puncture_pattern_1));

            memcpy(reinterpret_cast<void *>(&lsf), buffer.data()+1, 30);
        }
        break;

        case SYNC_PKT:
        {
            array<uint8_t, 26> pkt_type1; // 200+6 type-1 bits for pkt

            // Decode 368 type-3 soft bits to type-1 bits
            corrected_errors += viterbi_decode_punctured(buffer.data(), deinterleaved.data(),
                                    puncture_pattern_3, deinterleaved.size(), sizeof(puncture_pattern_3));

            memcpy(pkt_type1.data(), buffer.data()+1, pkt_type1.size());

            // Check that the frame number is consistent with what have been received previously
            if(pkt_type1[25] & (1 << 7)) // Check if this is the last frame
            {
                size_t nb_bytes = (pkt_type1[25] >> 2) & 0x1F;
                pkt_data->insert(pkt_data->cend(), pkt_type1.cbegin(), pkt_type1.cbegin() + nb_bytes);
                cout << "Received last packet frame with " << nb_bytes << " bytes inside. Total payload size is " << pkt_data->size() << "." << endl;
                status = packet_status::PKT_COMPLETE;
            }
            else
            {
                int frame_nb = (pkt_type1[25] >> 2) & 0x1F;
                if(frame_nb == received_pkt_frames+1)
                {
		            //cout << "Received frame number " << frame_nb << endl;
                    pkt_data->insert(pkt_data->cend(), pkt_type1.cbegin(), pkt_type1.cend() - 1);
                }
                else
                {
                    status = packet_status::ERROR;
                    cerr << "m17rx: The number of the frame added (" << frame_nb << ") does not follow the number of the previous frame (" << received_pkt_frames << ")." << endl;
                    return -1;
                }
            }

            received_pkt_frames++;
        }
        break;

        case SYNC_BER:
        {
            status = packet_status::BERT;
            corrected_errors += viterbi_decode_punctured(buffer.data(), deinterleaved.data(),
                                    puncture_pattern_2, deinterleaved.size(), sizeof(puncture_pattern_2));

            // Iterate over bits
            for(size_t i = 7; i < 197+7; i++)
            {
                size_t byte_idx = i/8;
                size_t bit_idx = 7-(i%8);
                bool bit = (buffer[byte_idx] >> bit_idx) & 0x1;

                if(bert_synccnt == 0)
                {
                    uint16_t lfsr_out = ((bert_lfsr >> 8) ^ (bert_lfsr >> 4)) & 0x1;
                    bert_lfsr = (bert_lfsr << 1) | lfsr_out;

                    bool err = (bit != lfsr_out);
                    bert_hist = (bert_hist << 1) | err;
                    bert_totcnt++;
                    bert_errcnt += err;
                    uint64_t hist_H = (bert_hist >> 64) & 0xFFFFFFFFFFFFFFFF;
                    uint64_t hist_L = bert_hist & 0xFFFFFFFFFFFFFFFF;

                    unsigned int popcnt = __builtin_popcountll(hist_H) + __builtin_popcountll(hist_L);
                    if(popcnt > bert_lockcnt)
                    {
                        bert_synccnt = bert_lockcnt;
                        bert_hist = 0;
                    }
                }
                else{
                    bool err = (bit ^ (bert_lfsr >> 8) ^ (bert_lfsr >> 4)) & 0x1;
                    bert_lfsr = (bert_lfsr << 1) | bit;
                    if(err)
                    {
                        bert_synccnt = bert_lockcnt;
                    }
                    else
                    {
                        bert_synccnt--;
                    }
                }
            }
        }
        break;

        default:
        {
            cerr << "m17rx: Unknown M17 sync word (0x" << hex << sync_word << dec << ")." << endl;
        }
        break;
    }

    return 0;
}

bool m17rx::is_valid() const
{
    if(status != packet_status::PKT_COMPLETE)
    {
        // An incomplete frame can not be valid
        return false;
    }

    // Check if the LSF is valid
    if(CRC_M17(lsf.data(), lsf.size()) != 0)
    {
        // LSF frame is corrupted
        return false;
    }

    return true;
}

bool m17rx::is_complete() const
{
    return (status == packet_status::PKT_COMPLETE);
}

uint32_t m17rx::corrected_bits() const
{
    return corrected_errors;
}

bool m17rx::is_error() const
{
    return (status == packet_status::ERROR);
}

array<uint8_t, 30> m17rx::get_lsf() const
{
    return lsf;
}

vector<uint8_t> m17rx::get_payload() const
{
    if(is_valid())
        return *pkt_data;

    return vector<uint8_t>();

}

bool m17rx::is_bert() const
{
    return status == packet_status::BERT;
}

size_t m17rx::get_bert_totcnt() const
{
    return bert_totcnt;
}

size_t m17rx::get_bert_errcnt() const
{
    return bert_errcnt;
}

bool m17rx::is_bert_synced() const
{
    return bert_synccnt == 0;
}
