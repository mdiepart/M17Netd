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
     * @param frame an array of 2*192 (384) soft bits (16 bits unsigned integers, one per bit)
     *
     * @return 0 on success, -1 on error
     */
    int add_frame(array<uint16_t, 2*SYM_PER_FRA> frame);

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

    private:
    static constexpr uint16_t SYNC_LSF = 0x55F7; /** LSF frame sync word */
    static constexpr uint16_t SYNC_PKT = 0x75FF; /** PKT frame sync word */

    enum class packet_status
    {
        EMPTY = 0,      /** Packet is empty */
        LSF_RECEIVED,   /** The LSF frame have been received and PKT frames may be added */
        COMPLETE,       /** The last PKT frame have been received */
        ERROR           /** An error occured (such as a skipped frame number) */
    };

    packet_status       status;                 /** Current status of the packet */
    array<uint8_t, 30>  lsf;                    /** LSF frame content */
    vector<uint8_t>     *pkt_data;              /** raw type-1 bits from the successive packet frames */
    uint32_t            corrected_errors;       /** Number of corrected bits along the full frame */
    int                 received_pkt_frames;    /** Number of packet frames received */
};
