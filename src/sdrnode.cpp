#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <alsa/asoundlib.h>

#ifdef __aarch64__
#include <arm_neon.h>
#endif

#include "spi.h"
#include "sdrnode.h"

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
 * Convert an array of n float to an array of n 24 bits signed integers (stored in 32 bits signed integers)
 *
 * @tparam n number of bits to use for the scale factor (2**n)
 * @param input pointer to input float array
 * @param output pointer to output 32 bits integers array (24 bits numbers stored as 32 bits)
 * @param len number of elements in the input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
template <size_t n>
void float_to_int24(const float *input, int32_t *output, const size_t len)
{
    constexpr size_t coeff = (1 << n);
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
        out = vcvtq_n_s32_f32(in, n); // Convert float to 24 bits fixed point (stored in 32 bits int).
        vst1q_s32(output, out); // Store the converted number in output array.
        __builtin_prefetch(output+4, 1, 0); // Pre-fetch the next 4 int16, we write, lowest temporal locality.
        input += 4; // Increment input pointer
        output += 4; // Increment output pointer
    }

    for(size_t i = 0; i < remainder_iters; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int16_t>((*input) * coeff);
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
 * Convert an array of n 16 bits fixed point signed integers to an array of floats
 *
 * @tparam n number of bits to use for the scale factor (2**n)
 * @param input pointer to input 32 bits integer array (24 bits numbers stored as 32 bits)
 * @param output pointer to output float array
 * @param len number of elements in input and output arrays
 *
 * @remark Uses ARM Neon extension if available
 */
template <size_t n>
void int24_to_float(const int32_t *input, float *output, const size_t len)
{
    constexpr size_t coeff = (1 << n);
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
        out = vcvtq_n_f32_s32(in, n);// Convert 24 bits fixed point signed integer to float
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

    for(size_t i = 0; i < n; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / coeff;
        input++;
        output++;
    }

#endif
}

sdrnode::sdrnode(const unsigned long rx_freq, const unsigned long tx_freq, const int ppm) : sx1255(spi_devname)
{
    long corr = (static_cast<long>(tx_freq)*ppm)/1000000;
    tx_frequency = tx_freq + corr;

    corr = (static_cast<long>(rx_freq)*ppm)/1000000;
    rx_frequency = rx_freq + corr;


    if(tx_frequency < 400e6 || tx_frequency > 510e6)
        throw invalid_argument("TX frequency is outside the [400,510] MHz range.");

    if(rx_frequency < 400e6 || rx_frequency > 510e6)
        throw invalid_argument("RX frequency is outside the [400,510] MHz range.");


    // Reset SX1255
    gpio_set_level(gpio_SX1255_reset, true);
    usleep(100u);
    gpio_set_level(gpio_SX1255_reset, false);
    usleep(5000u);

    sx1255.init();

    sx1255.set_rx_freq(rx_frequency);
    sx1255.set_tx_freq(tx_frequency);
    sx1255.set_lna_gain(sx1255_drv::LNA_GAIN_MAX_min36);

    // Open PCM device for RX
    open_pcm_rx();

    // Set gpios for RX
    prepare_rx();

    // Switch SX1255 to RX mode
    sx1255.switch_rx();

    tx_nRx = false;
}

sdrnode::~sdrnode()
{
    close_pcm();
    snd_config_update_free_global();
}

int sdrnode::gpio_set_level(unsigned gpio, bool value)
{
    int fd;
    char gpio_filename[sysfs_gpio_max_len];

    snprintf(gpio_filename, sysfs_gpio_max_len, sysfs_gpio_val, gpio);

    fd = open(gpio_filename, O_WRONLY);
    if (fd < 0) {
        cerr << "gpioSetValue unable to open gpio "
             << to_string(gpio) << ": " << strerror(errno)
             << "." << endl;
        return -1;
    }

    if (value) {
        if (write(fd, "1", 2) != 2) {
            cerr << "gpioSetValue ON error : gpio "
                 << to_string(gpio) << ": " << strerror(errno)
                 << "." << endl;
            close(fd);
            return -1;
        }
    }
    else {
        if (write(fd, "0", 2) != 2) {
            cerr << "gpioSetValue OFF error :gpio "
                 <<  to_string(gpio) << ": " << strerror(errno)
                 << "." << endl;
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

int sdrnode::open_pcm_rx()
{
    int err;
    snd_pcm_hw_params_t *pcm_hw_params;

    err = snd_pcm_open(&pcm_hdl, audio_rx_dev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        cerr << "Cannot open audio device " << audio_rx_dev
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return -1;
    }

    snd_pcm_hw_params_malloc(&pcm_hw_params);
    err = snd_pcm_hw_params_any(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "Cannot initialize hardware parameter structure: "
        << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_access(pcm_hdl, pcm_hw_params, pcm_access);
    if (err < 0) {
        cerr << "Cannot set access type: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S24_LE);
    if (err < 0) {
        cerr << "Cannot set sample format: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    pcm_rate = ideal_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_hdl, pcm_hw_params, &pcm_rate, 0);
    if (err < 0) {
        cerr << "cannot set sample rate: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_channels(pcm_hdl, pcm_hw_params, 2);
    if (err < 0) {
        cerr << "cannot set channel count: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "cannot set parameters: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_free(pcm_hw_params);

    cout << "pcm_hw_params set successfuly" << endl;

    err = snd_pcm_prepare(pcm_hdl);
    if (err < 0) {
        cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return -1;
    }

    return 0;
}

int sdrnode::open_pcm_tx()
{
    int err;
    snd_pcm_hw_params_t *pcm_hw_params;

    err = snd_pcm_open(&pcm_hdl, audio_rx_dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        cerr << "Cannot open audio device " << audio_rx_dev
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_malloc(&pcm_hw_params);
    err = snd_pcm_hw_params_any(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "Cannot initialize hardware parameter structure: "
             << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_access(pcm_hdl, pcm_hw_params, pcm_access);
    if (err < 0) {
        cerr << "Cannot set access type: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S24_LE);
    if (err < 0) {
        cerr << "Cannot set sample format: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    pcm_rate = ideal_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_hdl, pcm_hw_params, &pcm_rate, 0);
    if (err < 0) {
        cerr << "cannot set sample rate: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_channels(pcm_hdl, pcm_hw_params, 2);
    if (err < 0) {
        cerr << "cannot set channel count: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params(pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "cannot set parameters: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_free(pcm_hw_params);

    cout << "pcm_hw_params set successfuly" << endl;

    err = snd_pcm_prepare(pcm_hdl);
    if (err < 0) {
        cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << endl;
        pcm_hdl = nullptr;
        return err;
    }

    return 0;
}

void sdrnode::close_pcm()
{
    if(pcm_hdl == nullptr)
        return;

    snd_pcm_drain(pcm_hdl);
    snd_pcm_close(pcm_hdl);
}

int sdrnode::switch_rx()
{
    // Close pcm device and open it in rx
    close_pcm();
    int ret = open_pcm_rx();
    if(ret < 0)
        return -1;

    prepare_rx(); // Configure GPIOs

    ret = sx1255.switch_rx(); // Switch radio to RX mode
    if(ret < 0)
        return -1;

    tx_nRx = false;

    return 0;
}

int sdrnode::switch_tx()
{
    // Close pcm device and open it in tx
    close_pcm();

    prepare_tx(); // Configure GPIOs

    int ret = open_pcm_tx();
    if(ret < 0)
    {
        prepare_rx();
        return -1;
    }


    ret = sx1255.switch_tx(); // Switch radio to RX mode
    if(ret < 0)
        return -1;

    tx_nRx = true;

    return 0;
}

size_t sdrnode::receive(complex<float> *rx, size_t n)
{
    // TODO use snd_pcm_mmap_readi to avoid an unnecessary malloc and copy
    if(!tx_nRx)
    {
        int32_t *buff = new int32_t[n*2];
        if(buff == nullptr)
            return 0;

        snd_pcm_sframes_t read = snd_pcm_readi(pcm_hdl, buff, n);

        if(read < 0)
            read = snd_pcm_recover(pcm_hdl, read, 0);
        if(read < 0)
        {
            cout << "pcm read returned " << read << endl;
            delete[](buff);
            return 0;
        }
        else if(read > 0)
        {
            int24_to_float<24>(buff, reinterpret_cast<float*>(rx), (size_t)read*2);
            delete[](buff);
            return read;
        }
    }

    return 0;
}

int sdrnode::transmit(const complex<float> *tx, size_t n)
{
    if(tx_nRx)
    {
        int32_t *buff = new int32_t[n*2];

        if(buff == nullptr)
            return 0;

        float_to_int24<23>(reinterpret_cast<const float*>(tx), buff, n*2);

        snd_pcm_sframes_t written = 0;
        while(n > 0)
        {
            written = snd_pcm_writei(pcm_hdl, buff, n);
            if(written > 0)
                n -= written;
            else if(written < 0)
            {
                written = snd_pcm_recover(pcm_hdl, written, 0);
                if(written < 0)
                    break;
            }

        }

        delete[](buff);

        return (written < 0)?-1:0;
    }

    return 0;
}
