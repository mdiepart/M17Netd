#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>

#include <linux/spi/spidev.h>

using namespace std;

class spi_dev
{
    public:
    spi_dev();
    spi_dev(string dev_name, int mode, int word_len, int speed);
    ~spi_dev();

    /** Reads n bytes from the spi bus
     * @param rx buffer in which the n bytes read will be written
     * @param n number of bytes to read
     * 
     * @returns the number of bytes read, 0 for EOF or -1 for errors. 
     */
    int recv(uint8_t *rx, const size_t n) const;

     /** Reads n bytes from the spi bus
     * @param tx buffer containing the n bytes to send
     * @param n number of bytes to send
     * 
     * @returns the number of bytes sent or -1 for errors. 
     */   
    int send(const uint8_t *tx, const size_t n) const;

    /** Performs a full-duplex exchange on the SPI bus, simultaneously sending and receiving n bytes.
     * @param rx buffer in which the n bytes read will be written
     * @param tx buffer containing the n bytes to send
     * @param n number of bytes to send/receive
     * 
     * @remark It is allowed for rx and tx to point to the same buffer although a specific function
     *          is provided for that usecase.
     * 
     * @return 0 on success, -1 on error
     */
    int send_recv(uint8_t *rx, const uint8_t *tx, const size_t n) const;

    /** Performs a full-duplex exchange on the SPI bus, simultaneously sending and receiving n bytes.
     * @param buff buffer containing the n bytes to send. When returning, this buffer will contain the n bytes read.
     * @param n number of bytes to send/receive
     * 
     * @return 0 on success, -1 on error
     */
    int send_recv(uint8_t *buff, const size_t n) const;
    
    private:
    int fd;
    unsigned long speed;
    unsigned char mode;
    unsigned char endianness;
    unsigned char word_len;
};