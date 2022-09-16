#ifndef GPIO_RPI_H_INCLUDED
#define GPIO_RPI_H_INCLUDED
#include "stdint.h"
#include <string>

extern bool is_GPIO_initialized;

class gpio_rw {
  private:
    unsigned int mode;
    unsigned int number;
  public:
    int read();
    int write(unsigned int level);
    gpio_rw(int number, std::string mode);
    ~gpio_rw();
};

class SPI_rw {
  private:
    unsigned int spi_mode;
    unsigned int spi_channel;
    unsigned int spi_flag;
    unsigned int spi_handle;

    void set_spi_flag(unsigned int spi_mode=0,
                      bool CE_active_high=false, bool not_CE_reserved=false,
                      bool is_aux=false, bool is_3wire=true, unsigned int mosi_switch_bytes=0,
                      bool tx_lsb_first=false, bool rx_lsb_first=false, unsigned int word_size=8);

  public:
    SPI_rw(unsigned int channel, unsigned int speed, unsigned int spi_mode=0,
           bool CE_active_high=false, bool not_CE_reserved=false,
           bool is_aux=false, bool is_3wire=true, unsigned int mosi_switch_bytes=0,
           bool tx_lsb_first=false, bool rx_lsb_first=false, unsigned int word_size=8);

    ~SPI_rw();

    int read(void* buf, unsigned int buf_len_bytes);
    int write(void* buf, unsigned int buf_len_bytes);
    int xfer(void* s_buf, void* r_buf, unsigned int buf_len_bytes);
};

#endif
