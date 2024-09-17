#include <array>
#include <iostream>
#include <m17.h>

#include "m17rx.h"

using namespace std;

m17rx::m17rx(): status(packet_status::EMPTY), lsf(), corrected_errors(0), received_pkt_frames(0)
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

int m17rx::add_frame(array<uint16_t, 2*SYM_PER_FRA> frame)
{
    // Check if the packet is ready to receive a frame
    if(status == packet_status::COMPLETE)
    {
        cerr << "m17rx: cannot add another frame to a completed packet." << endl;
        return -1;
    }
    else if(status == packet_status::ERROR)
    {
        cerr << "m17rx: cannot add another frame to a packet in error state" << endl;
        return -1;
    }

    array<uint16_t, 2*SYM_PER_PLD> deinterleaved;

    // De-randomize the last 368 bits
    randomize_soft_bits(frame.data() + 2*SYM_PER_SWD);

    // de-interleave bits
    reorder_soft_bits(deinterleaved.data(), frame.data() + 2*SYM_PER_SWD);

    // Check the first 16 bits for a sync word
    uint16_t sync_word = 0;
    for(size_t i = 0; i < 16; i++)
    {
        // If bit is closer to 1
        if(frame[i] >= 0x7FFF)
        {
            sync_word++;
        }
        // Shift left
        sync_word <<= 1;
    }

    // Decode the frame based on the syncword
    switch(sync_word)
    {
        case SYNC_LSF:
            if(status != packet_status::EMPTY)
            {
                cerr << "m17rx: trying to add an LSF frame to a non-empty packet." << endl;
                return -1;
            }
            status = packet_status::LSF_RECEIVED;
            // Decode 368 type-3 soft bits to type-1 bits, stored in the lsf array
            corrected_errors += viterbi_decode_punctured(lsf.data(), deinterleaved.data(),
                                    puncture_pattern_2, deinterleaved.size(), sizeof(puncture_pattern_2));
        break;

        case SYNC_PKT:
            if(status != packet_status::LSF_RECEIVED)
            {
                cerr << "m17rx: packet is not ready to receive a new packet frame." << endl;
                return -1;
            }
            array<uint8_t, 26> pkt_type1; // 200+6 type-1 bits for pkt

            // Decode 368 type-3 soft bits to type-1 bits
            corrected_errors += viterbi_decode_punctured(pkt_type1.data(), deinterleaved.data(),
                                    puncture_pattern_3, deinterleaved.size(), sizeof(puncture_pattern_3));

            // Check that the frame number is consistent with what have been received previously
            if(pkt_type1[25] & (1 << 5)) // Check if this is the last frame
            {
                size_t nb_bytes = pkt_type1[25] & 0x1F;
                pkt_data->insert(pkt_data->cend(), pkt_type1.cbegin(), pkt_type1.cbegin() + nb_bytes);
                status = packet_status::COMPLETE;
            }
            else
            {
                size_t frame_nb = pkt_type1[25] & 0x1F;
                if(frame_nb == received_pkt_frames+1)
                {
                    pkt_data->insert(pkt_data->cend(), pkt_type1.cbegin(), pkt_type1.cend() - 1);
                }
                else
                {
                    status = packet_status::ERROR;
                    cerr << "The number of the frame added does not follow the number of the previous frame" << endl;
                    return -1;
                }
            }

            received_pkt_frames++;

        break;

        default:
            cerr << "m17rx: Unknown M17 sync word (0x" << hex << sync_word << ")." << endl;
            break;
    }

    return 0;
}

bool m17rx::is_valid() const
{
    if(status != packet_status::COMPLETE)
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
    return (status == packet_status::COMPLETE);
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
