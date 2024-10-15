#include "sx1255.h"
#include <alsa/asoundlib.h>

#include <cmath>
#include <vector>
#include <complex>
#include <iostream>
#include <array>
#include <csignal>

using namespace std;

int open_pcm_tx(snd_pcm_t **pcm_hdl, char *audio_dev);
int gpio_set_level(unsigned gpio, bool value);

volatile bool running = true;

void sigint_catcher(int signum)
{
    if(signum == SIGINT)
    {
        std::cout << "Ctrl-C caught, stopping all threads." << std::endl;
        running = false;
    }
}

int main(int argc, char *argv[])
{

    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, 0);

    constexpr unsigned int freq = 433475000;
    constexpr int ppm = -32;
    constexpr size_t n_frames = 960*2;
    constexpr unsigned int corrected_freq = freq + (static_cast<long>(freq)*ppm)/1000000;
    char audio_dev[] = "default:GDisDACout";
    char spi_dev[] = "/dev/spidev1.0";

    static constexpr unsigned gpio_PA_enable       = 15;
    static constexpr unsigned gpio_TX_lowpower     = 16;
    static constexpr unsigned gpio_bias_enable     = 17;
    static constexpr unsigned gpio_SX1255_reset    = 54;
    static constexpr unsigned gpio_relay_TX        = 55;

    sx1255_drv sx1255 = sx1255_drv(spi_dev);
    snd_pcm_t *pcm_handle;

    // Init complex samples array
    /* Full sample rate of 96000
     * Frequency of 9600 Hz -> 9600 cycles in 96000 samples = 96 cycles in 960 samples
     *
    */

    int16_t complex_baseband[2*n_frames];
    constexpr float tone = 2400;
    constexpr int16_t scale = INT16_MAX;

    for(size_t i = 0; i < n_frames; i++)
    {
        double arg = 2.0*M_PI*tone*i/96000.0;
        int16_t I = scale * cos(arg);
        int16_t Q = scale * sin(arg);
        complex_baseband[2*i] = I;
        complex_baseband[2*i+1] = Q;
    }

    gpio_set_level(gpio_SX1255_reset, 1);
    usleep(100);
    gpio_set_level(gpio_SX1255_reset, 0);
    usleep(5000);

    sx1255.init();
    sx1255.set_tx_freq(corrected_freq);

    open_pcm_tx(&pcm_handle, audio_dev);

    gpio_set_level(gpio_relay_TX, 1);
    usleep(10000);
    gpio_set_level(gpio_PA_enable, 1);
    gpio_set_level(gpio_bias_enable, 1);

    sx1255.switch_tx();

    while(running)
    {
        long size = n_frames;
        while(size > 0)
        {
            snd_pcm_sframes_t ret = snd_pcm_writei(pcm_handle, complex_baseband + 2*(n_frames-size), size);

            if(ret < 0)
                snd_pcm_recover(pcm_handle, ret, 0);
            else
                size -= ret;
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    sx1255.switch_rx();

    gpio_set_level(gpio_bias_enable, 0);
    gpio_set_level(gpio_PA_enable, 0);
    gpio_set_level(gpio_relay_TX, 0);

    return 0;
}

int open_pcm_tx(snd_pcm_t **pcm_hdl, char *audio_dev)
{
    int err;
    snd_pcm_hw_params_t *pcm_hw_params;

    err = snd_pcm_open(pcm_hdl, audio_dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        cerr << "Cannot open audio device " << audio_dev
             << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_malloc(&pcm_hw_params);
    err = snd_pcm_hw_params_any(*pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "Cannot initialize hardware parameter structure: "
             << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_access(*pcm_hdl, pcm_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        cerr << "Cannot set access type: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_format(*pcm_hdl, pcm_hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        cerr << "Cannot set sample format: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    unsigned int pcm_rate = 96000;
    err = snd_pcm_hw_params_set_rate_near(*pcm_hdl, pcm_hw_params, &pcm_rate, 0);
    if (err < 0) {
        cerr << "cannot set sample rate: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params_set_channels(*pcm_hdl, pcm_hw_params, 2);
    if (err < 0) {
        cerr << "cannot set channel count: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    err = snd_pcm_hw_params(*pcm_hdl, pcm_hw_params);
    if (err < 0) {
        cerr << "cannot set parameters: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    snd_pcm_hw_params_free(pcm_hw_params);

    cout << "pcm_hw_params set successfuly" << endl;

    err = snd_pcm_prepare(*pcm_hdl);
    if (err < 0) {
        cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << endl;
        *pcm_hdl = nullptr;
        return err;
    }

    return 0;
}

int gpio_set_level(unsigned gpio, bool value)
{
    int fd;
    static constexpr const char *sysfs_gpio_val    = "/sys/class/gpio/gpio%d/value";
    static constexpr size_t   sysfs_gpio_max_len   = 30;
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
