#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <numeric>

#include <m17.h>

#include "m17tx.h"

constexpr array<float, 161> m17tx::taps =
{
-0.002223795436133338f, -0.002258562030850857f, -0.002165191659582787f, -0.001940925054641284f,
-0.001590061429387669f, -0.001124098247971356f, -0.000561489178725134f,  0.000072986395428971f,
 0.000749228836822979f,  0.001433164849238003f,  0.002088381783765276f,  0.002677978438456125f,
 0.003166546138899426f,  0.003522182572779720f,  0.003718433964702327f,  0.003736059273378167f,
 0.003564513502570597f,  0.003203056021223239f,  0.002661403804300452f,  0.001959868285974094f,
 0.001128937358501837f,  0.000208290020837195f, -0.000754740846091241f, -0.001707213399497108f,
-0.002593057208989781f, -0.003355890813367709f, -0.003942055550078388f, -0.004303726757621754f,
-0.004401951199437721f, -0.004209453908864684f, -0.003713059198443574f, -0.002915579589649612f,
-0.001837042838958750f, -0.000515150652117307f,  0.000995107663336867f,  0.002622728531882198f,
 0.004282857720736837f,  0.005879806665602678f,  0.007310842635900604f,  0.008470633939519370f,
 0.009256194694322978f,  0.009572142063156937f,  0.009336053896165064f,  0.008483697849890306f,
 0.006973895352777105f,  0.004792785992855719f,  0.001957270338105298f, -0.001482568250839778f,
-0.005442230152061396f, -0.009802881030254906f, -0.014412827249051886f, -0.019090410765932563f,
-0.023628292538358159f, -0.027799035775521820f, -0.031361843944420778f, -0.034070255674238777f,
-0.035680551908776614f, -0.035960591958462185f, -0.034698766347739357f, -0.031712737022367654f,
-0.026857630620506945f, -0.020033358677597850f, -0.011190759859969880f, -0.000336293101214946f,
 0.012464944182369120f,  0.027088043320100185f,  0.043349064246229967f,  0.061007806136395594f,
 0.079772534825044220f,  0.099306560540135216f,  0.119236489534289961f,  0.139161908524203820f,
 0.158666204080632339f,  0.177328172567169262f,  0.194734041856030576f,  0.210489505338799121f,
 0.224231362657611233f,  0.235638370493727795f,  0.244440930460285671f,  0.250429278860985316f,
 0.253459893444308748f,  0.253459893444308748f,  0.250429278860985316f,  0.244440930460285671f,
 0.235638370493727795f,  0.224231362657611233f,  0.210489505338799121f,  0.194734041856030576f,
 0.177328172567169262f,  0.158666204080632339f,  0.139161908524203820f,  0.119236489534289961f,
 0.099306560540135216f,  0.079772534825044220f,  0.061007806136395594f,  0.043349064246229967f,
 0.027088043320100185f,  0.012464944182369120f, -0.000336293101214946f, -0.011190759859969880f,
-0.020033358677597850f, -0.026857630620506945f, -0.031712737022367654f, -0.034698766347739357f,
-0.035960591958462185f, -0.035680551908776614f, -0.034070255674238777f, -0.031361843944420778f,
-0.027799035775521820f, -0.023628292538358159f, -0.019090410765932563f, -0.014412827249051886f,
-0.009802881030254906f, -0.005442230152061396f, -0.001482568250839778f,  0.001957270338105298f,
 0.004792785992855719f,  0.006973895352777105f,  0.008483697849890306f,  0.009336053896165064f,
 0.009572142063156937f,  0.009256194694322978f,  0.008470633939519370f,  0.007310842635900604f,
 0.005879806665602678f,  0.004282857720736837f,  0.002622728531882198f,  0.000995107663336867f,
-0.000515150652117307f, -0.001837042838958750f, -0.002915579589649612f, -0.003713059198443574f,
-0.004209453908864684f, -0.004401951199437721f, -0.004303726757621754f, -0.003942055550078388f,
-0.003355890813367709f, -0.002593057208989781f, -0.001707213399497108f, -0.000754740846091241f,
 0.000208290020837195f,  0.001128937358501837f,  0.001959868285974094f,  0.002661403804300452f,
 0.003203056021223239f,  0.003564513502570597f,  0.003736059273378167f,  0.003718433964702327f,
 0.003522182572779720f,  0.003166546138899426f,  0.002677978438456125f,  0.002088381783765276f,
 0.001433164849238003f,  0.000749228836822979f,  0.000072986395428971f, -0.000561489178725134f,
-0.001124098247971356f, -0.001590061429387669f, -0.001940925054641284f, -0.002165191659582787f,
-0.002258562030850857f
};

m17tx::m17tx(const string_view &src, const string_view &dst, const shared_ptr<vector<uint8_t>> ip_pkt): bb_samples(0), bb_idx(0), filt_offset(0)
{
    if(ip_pkt->size() > 820)
    {
        throw(invalid_argument("ip_pkt is longer than the maximum payload a packet superframe can contain."));
    }

    // An M17 transmission (superframe)
    // A preamble (+3, -3)
    // An LSF frame ()
    // A packet sync burst
    // A "packet frames" (up to 25 bytes each)
    // (repeat last two step up to 32x for up to 33 frames total)
    // an EOT

    // How much data to reserve :
    // Preamble: 192 symbols
    // An LSF sync + frame: 192 symbols
    // PKT sync + frames: 192 symbols per frame
    // EOT: 192 symbols

    float frame[192];
    uint32_t cnt = 0;
    lsf_t lsf;
    uint8_t pkt_data[26]; // 25 bytes per frame + metadata (6 bits rounded up to 1 byte)

    // Content of the Link Setup Frame
    memset(&lsf, 0, sizeof(lsf_t));
    encode_callsign_bytes(lsf.src, src.data());
    encode_callsign_bytes(lsf.dst, dst.data());
    uint16_t lsf_type = M17_TYPE_PACKET | M17_TYPE_DATA | M17_TYPE_CAN(0);
    lsf.type[0] = lsf_type >> 8;
    lsf.type[1] = lsf_type & 0xFF;
    uint16_t lsf_crc = LSF_CRC(&lsf);
    lsf.crc[0] = lsf_crc >> 8;
    lsf.crc[1] = lsf_crc & 0xFF;

    // 25 bytes max per frame
    unsigned short nb_pkt_frames = (ip_pkt->size()+3)/25 + ((ip_pkt->size()+3)%25)?1:0;
    symbols = new vector<float>();
    symbols->reserve((nb_pkt_frames+3)*192); // packets + Preamble, LSF, EOT

    // Insert preample preceding the LSF frame
    send_preamble(frame, &cnt, PREAM_LSF);
    std::copy(frame, frame+cnt, back_inserter(*symbols));
    cnt = 0;

    // Insert LSF frame (includes the syncword)
    send_frame(frame, nullptr, FRAME_LSF, &lsf, 0, 0);
    std::copy(frame, frame+SYM_PER_FRA, back_inserter(*symbols));

    uint16_t pkt_crc = CRC_M17(ip_pkt->data(), ip_pkt->size()); // Packet CRC
    ip_pkt->push_back(static_cast<uint8_t>(pkt_crc >> 8));
    ip_pkt->push_back(static_cast<uint8_t>(pkt_crc));

    uint16_t index_o = 1; // The data-type specifier is already in there
    uint16_t index_i = 0;
    uint8_t frame_number = 0;
    pkt_data[0] = (uint8_t)0x4; // IPv4 data-type specifier

    while(index_i < ip_pkt->size())
    {
        index_o %= 25;
        // Copy whatever data is needed, whch is the lowest amount between
        // what is left to send and the space still available in the frame
        uint16_t len = min(static_cast<uint16_t>(ip_pkt->size())-index_i, 25-index_o );

        memcpy(pkt_data+index_o, ip_pkt->data()+index_i, len);

        index_i += len;
        index_o += len;

        // Check if this is the last frame
        if(index_i >= ip_pkt->size())
        {
            memset(pkt_data+index_o, 0, 26-index_o);
            pkt_data[25] = ((index_o << 2) | (1 << 7)) & 0xFC; // EOT + pkt len

        }else{
            pkt_data[25] = frame_number << 2; // frame number
        }

        send_frame(frame, pkt_data, FRAME_PKT, nullptr, 0, 0);
        std::copy(frame, frame+SYM_PER_FRA, back_inserter(*symbols));
        frame_number++;
    }

    send_eot(frame, &cnt);
    std::copy(frame, frame+cnt, back_inserter(*symbols));
    cnt = 0;
    filt_buff.fill(0.0f);
    filt_buff[0] = symbols->at(bb_idx++);
}

m17tx::~m17tx()
{
    delete(symbols);
}

vector<float> m17tx::get_baseband_samples(size_t n)
{
    // Output baseband filtered signal
    vector<float> baseband = vector<float>();
    baseband.reserve(n);

    for(size_t i = 0; i < n; i++)
    {
        auto filt_buff_it = filt_buff.begin() + filt_offset;
        float out = inner_product(taps.begin(), taps.end(), filt_buff_it, 0.0f);

        if(filt_offset == 0)
        {
            // Reload next sample
            for(size_t j = 180; j >= N; j -= N)
            {
                // Shift non-zero elements
                filt_buff[j] = filt_buff[j-N];
            }

            // If we used all symbols, use 0s instead
            if(bb_idx >= symbols->size() && bb_idx < symbols->size()+(nb_taps/N))
            {
                filt_buff[0] = 0;
                bb_idx++;
            }
            else if(bb_idx >= symbols->size()+(nb_taps/N))
            {
                cout << "We used all symbols" << endl;
                break;
            }
            else
            {
                filt_buff[0] = symbols->at(bb_idx++);
            }

            filt_offset = N-1;
        }
        else
        {
            filt_offset--;
        }
        baseband.push_back(out);

        bb_samples++;
        if(bb_samples >= (symbols->size() * N)+nb_taps/2)
        {
            break;
        }
    }

    return baseband;
}

vector<float> m17tx::get_symbols() const
{
    return *symbols;
}

size_t m17tx::baseband_samples_left() const
{
    return ((symbols->size() * N)+nb_taps/2)-bb_samples;
}