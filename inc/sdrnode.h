#pragma once

#include <string>
#include <cstdlib>
#include <vector>

#include <alsa/asoundlib.h>

#include "sx1255.h"

using namespace std;

class sdrnode
{
    private:
    // GPIOs
    static constexpr unsigned gpio_ADC_temp_enable = 11;
    static constexpr unsigned gpio_ADC_batt_enable = 12;
    static constexpr unsigned gpio_PA_enable       = 15;
    static constexpr unsigned gpio_TX_lowpower     = 16;
    static constexpr unsigned gpio_bias_enable     = 17;
    static constexpr unsigned gpio_SX1255_reset    = 54;
    static constexpr unsigned gpio_relay_TX        = 55;
    static constexpr size_t   sysfs_gpio_max_len   = 30;
    static constexpr const char *sysfs_gpio_val    = "/sys/class/gpio/gpio%d/value";

    /**
     * Sets the level of a gpio pin
     * 
     * @param gpio pin number whoe level is to be set
     * @param value value to set: true for high, false for low
     * 
     * @return 0 on success, -1 on error
     */
    int gpio_set_level(unsigned gpio, bool value);

    // PCM/I2S
    static constexpr unsigned ideal_rate   = 96000; /** Ideal baseband sampling rate */
    unsigned int pcm_rate = ideal_rate;
    static constexpr const char *audio_rx_dev     = "default:GDisDACout";
    static constexpr const char *audio_tx_dev     = "default:GDisDACout";
    
    static constexpr snd_pcm_access_t pcm_access = SND_PCM_ACCESS_RW_INTERLEAVED;
    snd_pcm_t *pcm_hdl;

    /**
     * Open the PCM device in TX mode
     * 
     * @return 0 on success, -1 on error
     */
    int open_pcm_tx();

    /**
     * Open the PCB device in RX mode
     * 
     * @return 0 on success, -1 on error
     */
    int open_pcm_rx();

    /**
     * Closes the PCM device. 
     */
    void close_pcm();
    

    // SX1255
    static constexpr const char *spi_devname = "/dev/spidev1.0";
    sx1255_drv sx1255;

    /**
     * Sets the GPIOs ready for TX
     */
    inline void prepare_tx()
    {
        gpio_set_level(gpio_relay_TX, 1);

        if( usleep(10000) < 0)
            cerr << "usleep was interrupted while preparing to TX." << endl;
        
        //gpio_set_level( gpio_PMAX_BUFA_enable, 1 ); // ??
        gpio_set_level( gpio_PA_enable, 1 );
        gpio_set_level( gpio_bias_enable, 1 );
    }

    /**
     * Sets the GPIOs ready for RX
     */
    inline void prepare_rx()
    {
        gpio_set_level( gpio_bias_enable, 0 ) ;
        gpio_set_level( gpio_PA_enable, 0 ) ;
        gpio_set_level( gpio_relay_TX, 0 ) ;
    }

    // Internal state
    bool tx_nRx = false;
    unsigned long rx_frequency = 0;
    unsigned long tx_frequency = 0;


    public:
    sdrnode(const unsigned long rx_freq, const unsigned long tx_freq, int ppm);
    ~sdrnode();
    /**
     * Switches the sdrnode unit in RX mode. Once in RX mode, the samples can be querried with receive()
     * 
     * @return 0 on success, -1 on error
     */
    int switch_rx();

    /**
     * Switches the sdrnode unit in TX mode. Once in TX mode, samples can be provided with transmit()
     * 
     * @return 0 on success, -1 on error
     */
    int switch_tx();

    /**
     * Reads at most n samples from the radio
     * This operation will read a number n of samples. Each 
     * sample containing two floats, it will read at most 2n floats
     * 
     * @param rx buffer containing 2n floats
     * @param n number of I/Q samples to send. One sample is composed of 2 floats
     * 
     * @return the number of samples read
     */
    size_t receive(float *rx, const size_t n);

    /**
     * Writes at most n samples to the radio
     * This operation will write a number n of samples. Each sample containing
     * two floats, it will write at most 2n floats.
     * 
     * @param tx buffer containing 2n floats
     * @param n number of I/Q samples to send. One sample is composed of 2 floats
     * 
     * @return the number of samples written
     */
    size_t transmit(const float *tx, const size_t n);

};