#ifndef __TYPE_CONVERSION_HPP_
#define __TYPE_CONVERSION_HPP_

#ifdef __aarch64__
#include <arm_neon.h>
#endif
#include <cstddef>
#include <cstdint>

/**
 * Convert an array of n float to an array of n 16 bits signed integers
 *
 * @param input pointer to input float array
 * @param output pointer to output 16 bits integer array
 * @param len number of elements in the input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
void float_to_int16(const float *input, int16_t *output, const size_t len)
{
#ifdef __aarch64__
    int16x4_t out;
    int32x4_t tmp;
    float32x4_t in;

    const size_t simd_iters = len/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = len%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1q_f32(input); // Load next 4 floats
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 floats, we read, lowest temporal locality.
        tmp = vcvtq_n_s32_f32(in, 15); // Convert float to 16 bits fixed point (stored in 32 bits int).
        out = vmovn_s32(tmp); // narrows down the 32 bits to 16 bits integers.
        vst1_s16(output, out); // Store the converted number in output array.
        __builtin_prefetch(output+4, 1, 0); // Pre-fetch the next 4 int16, we write, lowest temporal locality.
        input += 4; // Increment input pointer
        output += 4; // Increment output pointer
    }

    for(size_t i = 0; i < remainder_iters; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int16_t>((*input) * __INT16_MAX__);
        input++;
        output++;

    }
#else
    // Naïve implementation, manually convert all floats to 16 bits integers

    for(size_t i = 0; i < len; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int16_t>((*input) * __INT16_MAX__);
        input++;
        output++;
    }

#endif
}

/**
 * Convert an array of n float to an array of n 32 bits signed integers
 *
 * @tparam n number of bits to use for the output (i.e. use 24 to return int24_t stored as int32_t)
 * @param input pointer to input float array
 * @param output pointer to output 32 bits integers array
 * @param len number of elements in the input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
template <size_t n>
void float_to_int32(const float *input, int32_t *output, const size_t len)
{
    const size_t bitlen = n-2; // Remove 1 for signedness, 1 because we want the length of fractional part only
    constexpr int32_t coeff = (1 << (n-1));
#ifdef __aarch64__
    int32x4_t out;
    float32x4_t in;

    const size_t simd_iters = len/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = len%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1q_f32(input); // Load next 4 floats
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 floats, we read, lowest temporal locality.
        out = vcvtq_n_s32_f32(in, bitlen); // Convert float to 24 bits fixed point (stored in 32 bits int).
        vst1q_s32(output, out); // Store the converted number in output array.
        __builtin_prefetch(output+4, 1, 0); // Pre-fetch the next 4 int16, we write, lowest temporal locality.
        input += 4; // Increment input pointer
        output += 4; // Increment output pointer
    }

    for(size_t i = 0; i < remainder_iters; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int32_t>((*input) * coeff);
        input++;
        output++;

    }
#else
    // Naïve implementation, manually convert all floats to 24 bits integers

    for(size_t i = 0; i < len; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int32_t>((*input) * coeff);
        input++;
        output++;
    }

#endif
}

/**
 * Convert an array of n 16 bits fixed point signed integers to an array of floats
 *
 * @param input pointer to input 16 bits integers array
 * @param output pointer to output float array
 * @param len number of elements in input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
void int16_to_float(const int16_t *input, float *output, const size_t len)
{
#ifdef __aarch64__
    float32x4_t out;
    int32x4_t tmp;
    int16x4_t in;

    const size_t simd_iters = len/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = len%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1_s16(input); // Load next 4 16-bits fixed points integers
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 ints, we read, lowest temporal locality.
        tmp = vmovl_s16(in); // Widen 16 bits to 32 bits integers
        out = vcvtq_n_f32_s32(tmp, 15);// Convert 16 bits fixed point signed integer to float (scaled down by 1/__INT16_MAX__)
        vst1q_f32(output, out); // Store the converted number in output array.
        __builtin_prefetch(output+4, 1, 0); // Pre-fetch the next 4 int16, we write, lowest temporal locality.
        input += 4; // Increment input pointer
        output += 4; // Increment output pointer
    }

    for(size_t i = 0; i < remainder_iters; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / __INT16_MAX__;
        input++;
        output++;
    }
#else
    // Naïve implementation, manually convert all floats to 16 bits integers

    for(size_t i = 0; i < len; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / __INT16_MAX__;
        input++;
        output++;
    }

#endif
}

/**
 * Convert an array of n 32 bits fixed point signed integers to an array of floats
 *
 * @tparam n number of meaningful bits in the input (including sign)
 * @param input pointer to input 32 bits integer array
 * @param output pointer to output float array
 * @param len number of elements in input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
template <size_t n>
void int32_to_float(const int32_t *input, float *output, const size_t len)
{
    const size_t bitlen = n-2; // Remove 1 for signedness, 1 because we want the length of fractional part only
    constexpr int32_t coeff = (1 << (n-1));
#ifdef __aarch64__
    float32x4_t out;
    int32x4_t in;

    const size_t simd_iters = len/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = len%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1q_s32(input); // Load next 4 32-bits fixed points integers
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 ints, we read, lowest temporal locality.
        in = reinterpret_cast<int32x4_t>(vshlq_n_u32(reinterpret_cast<uint32x4_t>(in), 8));
        out = vcvtq_n_f32_s32(in, bitlen);// Convert 24 bits fixed point signed integer to float
        vst1q_f32(output, out); // Store the converted number in output array.
        __builtin_prefetch(output+4, 1, 0); // Pre-fetch the next 4 ints, we write, lowest temporal locality.
        input += 4; // Increment input pointer
        output += 4; // Increment output pointer
    }

    for(size_t i = 0; i < remainder_iters; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / coeff;
        input++;
        output++;
    }
#else
    // Naïve implementation, manually convert all 24 bits integers to floats

    for(size_t i = 0; i < len; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / coeff;
        input++;
        output++;
    }

#endif
}

#endif