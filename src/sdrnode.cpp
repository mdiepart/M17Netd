#include <cstdlib>
#include <iostream>
#include <cstring>

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
 * Uses ARM Neon extension if available
 */
void float_to_int16(float *input, int16_t *output, const size_t n);

/**
 * Convert an array of n 16 bits fixed point signed integers to an array of floats
 *
 * Uses ARM Neon extension if available
 */
void int16_to_float(int16_t *input, float *output, const size_t n);


void float_to_int16(float *input, int16_t *output, const size_t n)
{
#ifdef __aarch64__
    int16x4_t out;
    int32x4_t tmp;
    float32x4_t in;

    const size_t simd_iters = n/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = n%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1q_f32(input); // Load next 4 floats
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 floats, we read, lowest temporal locality.
        tmp = vcvtq_n_s32_f32(in, 16); // Convert float to 16 bits fixed point (stored in 32 bits int).
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

    for(size_t i = 0; i < n; i++)
    {
        // Scale to 16 bits, cast and store
        *output = static_cast<int16_t>((*input) * __INT16_MAX__);
        input++;
        output++;
    }

#endif
}

void int16_to_float(int16_t *input, float *output, const size_t n)
{
#ifdef __aarch64__
    float32x4_t out;
    int32x4_t tmp;
    int16x4_t in;

    const size_t simd_iters = n/4; // How many iters we can do with simd (4 floats per iter)
    const size_t remainder_iters = n%4; // How many iters are left if n is not a multiple of 4

    // Loop over the input buffer, convert floats 4 by 4 using SIMD
    for(size_t i = 0; i < simd_iters; i++)
    {
        in = vld1_s16(input); // Load next 4 16-bits fixed points integers
        __builtin_prefetch(input+4, 0, 0); // Pre-fetch the next 4 ints, we read, lowest temporal locality.
        tmp = vmovl_s16(in); // Widen 16 bits to 32 bits integers
        out = vcvtq_n_f32_s32(tmp, 16);// Convert 16 bits fixed point signed integer to float (scaled down by 1/__INT16_MAX__)
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

    for(size_t i = 0; i < n; i++)
    {
        // Cast to float then scale down and store
        *output = static_cast<float>(*input) / __INT16_MAX__;
        input++;
        output++;
    }

#endif
}

sdrnode::sdrnode(unsigned long rx_freq, unsigned long tx_freq, int ppm) : sx1255(spi_devname)
{
    unsigned long corr = (tx_freq*ppm)/1000000;
    tx_freq += corr;

    corr = (rx_freq*ppm)/1000000;
    rx_freq += corr;

    if(tx_freq < 400e6 || tx_freq > 510e6)
        throw invalid_argument("TX frequency is outside the [400,510] MHz range.");

    if(rx_freq < 400e6 || rx_freq > 510e6)
        throw invalid_argument("RX frequency is outside the [400,510] MHz range.");

    tx_frequency = tx_freq;
    rx_frequency = rx_freq;

    // Reset SX1255
    gpio_set_level(gpio_SX1255_reset, true);
    usleep(100u);
    gpio_set_level(gpio_SX1255_reset, false);
    usleep(5000u);

    sx1255.init();

    sx1255.set_rx_freq(rx_frequency);
    sx1255.set_tx_freq(tx_frequency);

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

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S16);
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

    err = snd_pcm_hw_params_set_format(pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S16_LE);
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
    int ret = open_pcm_tx();
    if(ret < 0)
        return -1;

    prepare_tx(); // Configure GPIOs

    ret = sx1255.switch_tx(); // Switch radio to RX mode
    if(ret < 0)
        return -1;

    tx_nRx = true;

    return 0;
}

size_t sdrnode::receive(float *rx, size_t n)
{
    // TODO use snd_pcm_mmap_readi to avoid an unnecessary malloc and copy
    if(!tx_nRx)
    {
        // Check that we are in a state where we can receive data
        int16_t *buff = new int16_t[n*2];

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
            int16_to_float(buff, rx, (size_t)read*2);
            delete[](buff);
            return read;
        }
    }

    return 0;
}

size_t sdrnode::transmit(const float *tx, size_t n)
{
    if(tx_nRx)
    {
        int16_t *buff = new int16_t[n*2];

        if(buff == nullptr)
            return 0;
        
        float_to_int16(const_cast<float*>(tx), buff, n*2);
        snd_pcm_sframes_t written = snd_pcm_writei(pcm_hdl, buff, n);

        if(written < 0)
            written = snd_pcm_recover(pcm_hdl, written, 0);

        delete[](buff);

        return (written<=0)?0:written;
    }

    return 0;
}
