#pragma once

#include <string>
#include <cstdint>
#include <cstdlib>

#include "spi.h"

/**
 * This class handles the configuration and settings of an SX1255 IC.
 * It does not handle the baseband data transfer.
 *
 * Use this class to set all the TX/RX/Other parameters that are transfered using the SPI interface.
 *
 * The gains can be set using ad-hoc functions. Other parameters must be set at compile time.
 */
class sx1255_drv
{
    public:
    sx1255_drv(const string dev_name);

    /**
     * Init sends the configuration registers as per the configuration
     * in the rest of this class
     */
    int init();

    /** Transmitter DAC gain */
    enum dac_gain {
        DAC_GAIN_MAX_min9 = 0,  // Max gain - 9dB
        DAC_GAIN_MAX_min6,      // Max gain - 6dB
        DAC_GAIN_MAX_min3,      // Max gain - 3dB (default)
        DAC_GAIN_MAX            // Max gain (0dBFS)
    };

    /** Receiver LNA gain */
    enum lna_gain {
        LNA_GAIN_MAX = 1,   // Max gain (default)
        LNA_GAIN_MAX_min6,  // Max gain - 6dB
        LNA_GAIN_MAX_min12, // Max gain - 12dB
        LNA_GAIN_MAX_min24, // Max gain - 24dB
        LNA_GAIN_MAX_min36, // Max gain - 36dB
        LNA_GAIN_MAX_min48, // Max gain - 48dB
    };

    /**
     * Sets the TX frequency of the SX1255 to freq
     *
     * @param freq the frequency in Hz
     *
     * @return 0 on success, -1 on error
     */
    int set_tx_freq(unsigned long freq);

    /**
     * Sets the gain of the TX DAC to gain
     *
     * @param gain the gain of the dac
     *
     * @return 0 on success, -1 on error
     */
    int set_dac_gain(dac_gain gain = DAC_GAIN_MAX_min3);

    /**
     * Sets the gain of the TX mixer
     *
     * @param gain, the gain to set. The actual gain will be -37.5 + 2*gain (in dB)
     *          gain must be between 0 and 15.
     *
     * @return 0 on success, -1 on error
     */
    int set_tx_mix_gain(unsigned char gain = 14);

    /**
     * Sets the RX frequency of the device
     *
     * @param freq the frequency in Hz
     *
     * @return 0 on success, -1 on error
     */
    int set_rx_freq(unsigned long freq);

    /**
     * Sets the RX LNA gain
     *
     * @param gain the gain of the LNA
     *
     * @return 0 on success, -1 on error
     */
    int set_lna_gain(lna_gain gain = LNA_GAIN_MAX);

    /**
     * Sets the RX Programmable Gain Array of the device
     *
     * @param gain the gain of the PGA. The actual gain will be 2*gain dB above the lowest gain (unspecified)
     *
     * @return 0 on success, -1 on error
     */
    int set_rx_pga_gain(unsigned char gain = 15);

    /**
     * Switch the device to RX mode.
     *
     * @return 0 on success, -1 on error
     */
    int switch_rx();

    /**
     * Switch the device to TX mode.
     *
     * @return 0 on success, -1 on error
     */
    int switch_tx();

    /**
     * Querries the version number of the device.
     *
     * @return the version number of the device
     */
    uint8_t read_version();

    private:
    static constexpr unsigned long xtal_frequency = 36864000UL;
    static constexpr unsigned char tx_mixer_tank_cap = 0x03; // 384 fF
    static constexpr unsigned char tx_mixer_tank_res = 0x04; // 2.18 kR
    static constexpr unsigned char tx_pll_bw = 0x03; // 300 Hz
    static constexpr unsigned char tx_filter_bw = 0x00; // 0.418 MHz
    static constexpr unsigned char tx_dac_bw = 0x02; // 40 taps

    static constexpr unsigned char rx_zin = 0x1; // 200 Ohm
    static constexpr unsigned char rx_adc_bw = 0x2; // 100kHz < BW < 400 kHz
    static constexpr unsigned char rx_adc_trim = 0x05; // Default trim value
    static constexpr unsigned char rx_pga_bw = 0x03; // 500 kHz
    static constexpr unsigned char rx_pll_bw = 0x03; // 300 Hz
    static constexpr unsigned char rx_adc_temp = 0x00;

    static constexpr unsigned char io_map = 0x00;

    static constexpr unsigned char dig_loopback_en = 0;
    static constexpr unsigned char rf_loopback_en = 0;
    static constexpr unsigned char ckout_enable = 1;
    static constexpr unsigned char ck_select_tx_dac = 0;
    static constexpr unsigned char iism_rx_disable = 0;
    static constexpr unsigned char iism_tx_disable = 0;
    static constexpr unsigned char iism_mode = 0x02; // Mode B2
    static constexpr unsigned char iism_clk_div = 0x3; // XTAL/CLK_OUT div (0x04 = 12, 0x03 = 8)

    // interpolation/decimation factor = 8*3^1*2^4 = 384
    static constexpr unsigned char int_dec_mantisse = 0;
    static constexpr unsigned char int_dec_m_parameter = 0x01;
    static constexpr unsigned char int_dec_n_parameter = 0x04;
    static constexpr unsigned char iism_truncation = 0; // Alignment on LSB

    static constexpr unsigned char REG_WRITE        = 0x80;
    static constexpr unsigned char MODE_ADDR        = 0x00;
    static constexpr unsigned char FRFH_RX_ADDR     = 0x01;
    static constexpr unsigned char FRFM_RX_ADDR     = 0x02;
    static constexpr unsigned char FRFL_RX_ADDR     = 0x03;
    static constexpr unsigned char FRFH_TX_ADDR     = 0x04;
    static constexpr unsigned char FRFM_TX_ADDR     = 0x05;
    static constexpr unsigned char FRFL_TX_ADDR     = 0x06;
    static constexpr unsigned char VERSION_ADDR     = 0X07;
    static constexpr unsigned char TXFE1_ADDR       = 0x08;
    static constexpr unsigned char TXFE2_ADDR       = 0x09;
    static constexpr unsigned char TXFE3_ADDR       = 0x0A;
    static constexpr unsigned char TXFE4_ADDR       = 0x0B;
    static constexpr unsigned char RXFE1_ADDR       = 0x0C;
    static constexpr unsigned char RXFE2_ADDR       = 0x0D;
    static constexpr unsigned char RXFE3_ADDR       = 0x0E;
    static constexpr unsigned char CK_SEL_ADDR      = 0x10;
    static constexpr unsigned char STAT_ADDR        = 0x11;
    static constexpr unsigned char IISM_ADDR        = 0x12;
    static constexpr unsigned char DIG_BRIDGE_ADDR  = 0x13;

    spi_dev spi;

    static constexpr uint32_t sx1255_calc_freq(const unsigned long freq)
    {
        uint64_t tmp = ((uint64_t)freq * (2 << 19));
        return tmp/xtal_frequency;
    }

    static inline uint8_t MODE(uint8_t pa_enable, uint8_t tx_enable, uint8_t rx_enable, uint8_t ref_enable)
    {
        return ((pa_enable << 3) | (tx_enable << 2) | (rx_enable << 1) | ref_enable) & 0x0F;
    }

    static inline uint8_t TXFE1(uint8_t dac_gain, uint8_t mix_gain)
    {
        return ((dac_gain << 4) | mix_gain) & 0x7F;
    }

    static inline uint8_t TXFE2()
    {
        return ((tx_mixer_tank_cap << 3) | tx_mixer_tank_res) & 0x3F;
    }

    static inline uint8_t TXFE3()
    {
        return ((tx_pll_bw << 5) | tx_filter_bw) & 0x7F;
    }

    static inline uint8_t TXFE4()
    {
        return tx_dac_bw & 0x07;
    }

    static inline uint8_t RXFE1(uint8_t lna_gain, uint8_t pga_gain)
    {
        return (lna_gain << 5) | pga_gain << 1 | rx_zin;
    }

    static inline uint8_t RXFE2()
    {
        return (rx_adc_bw << 5) | (rx_adc_trim << 2) | rx_pga_bw;
    }

    static inline uint8_t RXFE3()
    {
        return ((rx_pll_bw << 1) | rx_adc_temp) & 0x07;
    }

    static inline uint8_t CK_SEL()
    {
        return ((dig_loopback_en << 3) | (rf_loopback_en << 2) | (ckout_enable << 1) | ck_select_tx_dac) & 0x0F;
    }

    static inline uint8_t IISM()
    {
        return (iism_rx_disable << 7) | (iism_tx_disable << 6) | (iism_mode << 4) | iism_clk_div;
    }

    static inline uint8_t DIG_BRIDGE()
    {
        return (int_dec_mantisse << 7) | (int_dec_m_parameter << 6) | (int_dec_n_parameter << 3) | (iism_truncation << 2);
    }
};