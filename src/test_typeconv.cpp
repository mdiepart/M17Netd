#include <iostream>
#include <array>
#include "type_conversion.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    constexpr size_t len = 10000;
    array<float, len> truth_float;
    array<int16_t, len> truth_int16;
    array<int32_t, len> truth_int24;

    // Fill the input float array
    float step = 2.0f/(len-1);
    truth_float[0] = -1.0f;
    for(size_t i = 1; i < truth_float.size(); i++)
    {
        truth_float[i] = truth_float[i-1] + step;
    }
    truth_float[len-1] = 1.0f;

    // Fill the input for int16
    for(size_t i = 0; i < len; i++)
    {
        truth_int16[i] = truth_float[i] * __INT16_MAX__;
    }

    // Fill the input for int24
    for(size_t i = 0; i < len; i++)
    {
        int32_t c = (1<<23)-1;
        truth_int24[i] = truth_float[i] * c;
    }

    array<int16_t, len> check_int16;
    array<int32_t, len> check_int24;
    float_to_int16(truth_float.data(), check_int16.data(), len);
    float_to_int32<24>(truth_float.data(), check_int24.data(), len);

    cout << "Checking the conversions from floats to integers" << endl;
    for(size_t i = 0; i < len; i++)
    {
        if(check_int16[i] != truth_int16[i])
        {
            cout << "At index " << i << " the 16 bits computed value (" << check_int16[i] << ") differs from the expected value (" << truth_int16[i] << ")." << endl;
        }
        if(check_int24[i] != truth_int24[i])
        {
            cout << "At index " << i << " the 24 bits computed value (" << check_int24[i] << ") differs from the expected value (" << truth_int24[i] << ")." << endl;
        }
    }

    array<float, len> check_float_from16;
    array<float, len> check_float_from24;
    int16_to_float(truth_int16.data(), check_float_from16.data(), len);
    int32_to_float<24>(truth_int24.data(), check_float_from24.data(), len);

    cout << "Checking the conversions from integers to floats" << endl;
    for(size_t i = 0; i < len; i++)
    {
        if(check_float_from16[i] - truth_float[i] > 1e-5)
        {
            cout << "At index " << i << " the float value computed from 16 bits integer (" << check_float_from16[i] << ") differs from the expected value (" << truth_float[i] << ")." << endl;
        }
        if(check_float_from24[i] - truth_float[i] > 1e-6)
        {
            cout << "At index " << i << " the float value computed from 24 bits integer (" << check_float_from24[i] << ") differs from the expected value (" << truth_float[i] << ")." << endl;
        }
    }

    return EXIT_SUCCESS;
}
