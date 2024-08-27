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

    vector<float> *data;
    static const array<float, nb_taps> taps;
    size_t bb_samples = 0;
    size_t bb_idx = 0;
    array<float, (nb_taps+N)> filt_buff{};
    size_t filt_offset = 0;

public:
    m17tx(const string_view &src, const string_view &dst, const shared_ptr<vector<uint8_t>> ip_pkt);
    ~m17tx();

    vector<float> getBasebandSamples(size_t n);
    vector<float> getSymbols() const;
};
