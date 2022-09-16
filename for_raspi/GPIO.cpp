#include "GPIO.hpp"
#include "pigpio.h"

bool is_GPIO_initialized = false;

gpio_rw::gpio_rw(int cs_number, std::string cs_mode){
  if (!is_GPIO_initialized) {
    int GPIO_init_state = 0;
    GPIO_init_state = gpioInitialise();
    if (GPIO_init_state != PI_INIT_FAILED) {
      is_GPIO_initialized = true;
    }
  }
  if (cs_mode=="r") {
    gpioSetMode(cs_number, PI_INPUT);
    number = cs_number;
    mode = PI_INPUT;
  } else if (cs_mode == "w") {
    gpioSetMode(cs_number, PI_OUTPUT);
    number = cs_number;
    mode = PI_OUTPUT;
  } else if (cs_mode == "ALT0") {
    gpioSetMode(cs_number, PI_ALT0);
    number = cs_number;
    mode = PI_ALT0;
  } else if (cs_mode == "ALT1") {
    gpioSetMode(cs_number, PI_ALT1);
    number = cs_number;
    mode = PI_ALT1;
  } else if (cs_mode == "ALT2") {
    gpioSetMode(cs_number, PI_ALT2);
    number = cs_number;
    mode = PI_ALT2;
  } else if (cs_mode == "ALT3") {
    gpioSetMode(cs_number, PI_ALT3);
    number = cs_number;
    mode = PI_ALT3;
  } else if (cs_mode == "ALT4") {
    gpioSetMode(cs_number, PI_ALT4);
    number = cs_number;
    mode = PI_ALT4;
  } else if (cs_mode == "ALT5") {
    gpioSetMode(cs_number, PI_ALT5);
    number = cs_number;
    mode = PI_ALT5;
  } else {
    gpioSetMode(cs_number, PI_OUTPUT);
    number = cs_number;
    mode = PI_OUTPUT;
  }
}

gpio_rw::~gpio_rw(){
  if (is_GPIO_initialized) {
    gpioTerminate();
    is_GPIO_initialized = false;
  }
}
int gpio_rw::read() {
  return gpioRead(number);
}
int gpio_rw::write(unsigned int level) {
  return gpioWrite(number, level);
}

/*
ref: https://abyz.me.uk/rpi/pigpio/cif.html#spiOpen

spiFlags consists of the least significant 22 bits.
21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 b  b  b  b  b  b  R  T  n  n  n  n  W  A u2 u1 u0 p2 p1 p0  m  m
mm defines the SPI mode.
Warning: modes 1 and 3 do not appear to work on the auxiliary SPI.
Mode POL PHA
 0    0   0
 1    0   1
 2    1   0
 3    1   1
px is 0 if CEx is active low (default) and 1 for active high.
ux is 0 if the CEx GPIO is reserved for SPI (default) and 1 otherwise.
A is 0 for the main SPI, 1 for the auxiliary SPI.
W is 0 if the device is not 3-wire, 1 if the device is 3-wire. Main SPI only.
nnnn defines the number of bytes (0-15) to write before switching the MOSI line to MISO to read data. This field is ignored if W is not set. Main SPI only.
T is 1 if the least significant bit is transmitted on MOSI first, the default (0) shifts the most significant bit out first. Auxiliary SPI only.
R is 1 if the least significant bit is received on MISO first, the default (0) receives the most significant bit first. Auxiliary SPI only.
bbbbbb defines the word size in bits (0-32). The default (0) sets 8 bits per word. Auxiliary SPI only. 
*/
void SPI_rw::set_spi_flag(unsigned int spi_mode,
                          bool CE_active_high, bool not_CE_reserved,
                          bool is_aux, bool is_3wire, unsigned int mosi_switch_bytes,
                          bool tx_lsb_first, bool rx_lsb_first, unsigned int word_size){
  spi_flag = spi_mode;
  if (CE_active_high) {
    spi_flag |= 1 << (2+spi_channel);
  }
  if (not_CE_reserved) {
    spi_flag |= 1 << (5+spi_channel);
  }
  if (is_aux) {
    spi_flag |= 1 << 8; 
    if (tx_lsb_first) {
      spi_flag |= 1 << 14;
    }
    if (rx_lsb_first) {
      spi_flag |= 1 << 15;
    }
    spi_flag |= word_size << 16;
  } else {
    if (!is_3wire) {
      spi_flag |= 1 << 9;
      spi_flag |= mosi_switch_bytes << 10;
    }
  }
}

SPI_rw::SPI_rw(unsigned int channel, unsigned int speed, unsigned int spi_mode,
               bool CE_active_high, bool not_CE_reserved,
               bool is_aux, bool is_3wire, unsigned int mosi_switch_bytes,
               bool tx_lsb_first, bool rx_lsb_first, unsigned int word_size) {
  if (!is_GPIO_initialized) {
    int GPIO_init_state = 0;
    GPIO_init_state = gpioInitialise();
    if (GPIO_init_state != PI_INIT_FAILED) {
      is_GPIO_initialized = true;
    }
  }
  set_spi_flag(spi_mode, CE_active_high, not_CE_reserved, is_aux, is_3wire, mosi_switch_bytes, tx_lsb_first, rx_lsb_first, word_size);
  spi_handle = spiOpen(channel, speed, spi_flag);
}

SPI_rw::~SPI_rw(){
  spiClose(spi_handle);
  if (is_GPIO_initialized) {
    gpioTerminate();
    is_GPIO_initialized = false;
  }
}

int SPI_rw::read(void* buf, unsigned int buf_len_bytes){
  return spiRead(spi_handle, (char*)buf, buf_len_bytes);
}

int SPI_rw::write(void* buf, unsigned int buf_len_bytes){
  return spiWrite(spi_handle, (char*)buf, buf_len_bytes);
}

int SPI_rw::xfer(void* s_buf, void* r_buf, unsigned int buf_len_bytes){
  return spiXfer(spi_handle, (char*)s_buf, (char*)r_buf, buf_len_bytes);
}
