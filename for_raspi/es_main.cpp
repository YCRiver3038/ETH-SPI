#include "network.hpp"
#include "GPIO.hpp"
#include "buffers.hpp"

#include "signal.h"
#include "getopt.h"
#include "stdlib.h"
#include "limits.h"
#include <atomic>
#include <functional>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>

std::mutex mx_es_req;
std::atomic_bool eth_send_requested(false);

void eth_send_request(bool req){
  std::lock_guard lg_es_req(mx_es_req);
  eth_send_requested.store(req);
}

std::atomic_bool ie_exit_requested(false);
// signal(SIGINT) handler
void ie_signal_handler(int sig_num){
  network_force_return_req = true;
  ie_exit_requested.store(true);
}

void show_help(){
  printf(
    "--help : show help\n"
    "--buflen BUFLEN : set buffer length\n"
    "--sendto-ip IP_ADDRESS : set destination IP address\n"
    "--sendto-port PORT : set destination port\n"
    "--spi-ch CHANNEL : set SPI channel\n"
    "--spi-freq FREQ : set SPI clock frequency in Hz\n");
}

void thr_read_spi_data(ring_buffer<uint16_t>& buf, SPI_rw& spi_dev, uint32_t& send_req_timing) {
  union u_rw_ds {
    uint8_t data_u8[2];
    uint16_t data_u16;
  };
  u_rw_ds r_data;
  u_rw_ds w_data;
  u_rw_ds r_data_be;

  w_data.data_u8[0] = 0b01100000;
  w_data.data_u8[1] = 0x00;

  uint32_t ndata_count = 0;
  uint32_t ndata_count_reset_timing = 0;

  ndata_count_reset_timing = send_req_timing;
  if (send_req_timing > buf.get_buf_length()) {
    ndata_count_reset_timing = buf.get_buf_length();
    send_req_timing = buf.get_buf_length();
  }

  while (!ie_exit_requested.load()) {
    spi_dev.xfer(w_data.data_u8, r_data_be.data_u8, sizeof(uint16_t)); // read ADC data
    // endian transform
    //r_data.data_u8[0] = r_data_be.data_u8[1];
    //r_data.data_u8[1] = r_data_be.data_u8[0];
    // put data into FIFO
    buf.put_data(r_data_be.data_u16);
    ndata_count++;
    if (ndata_count >= ndata_count_reset_timing) {
      eth_send_request(true);
      ndata_count  = 0;
    }
  }
}

void thr_send(network& s_dest, ring_buffer<uint16_t>& data_buffer, uint32_t send_elements){
  while (!ie_exit_requested.load()) {
    if (eth_send_requested.load()) {
      s_dest.send_data(data_buffer.get_data_nelm(send_elements), send_elements);
      eth_send_request(false);
    }
  }
}

int main(int argc, char* argv[]) {
  /* Keyboard interrupt handler*/
  struct sigaction ie_sigint_action{};
  int sigaction_errcode = 0;
  ie_sigint_action.sa_handler = ie_signal_handler;
  sigaction_errcode = sigaction(SIGINT, &ie_sigint_action, NULL);
  if (sigaction_errcode != 0) {
    fprintf(stderr, "Interrupt handler registoration error: %s\n", strerror(sigaction_errcode));
    return -1;
  }

  /* parameter setup */
  constexpr uint32_t ca_buf_len_default = 4096;
  constexpr int ca_spi_freq_default = 1000000;
  constexpr int ca_spi_ch_default = 0;
  constexpr uint32_t ca_send_samples_default = 128;

  uint32_t ca_buf_len = ca_buf_len_default;
  int ca_spi_freq = ca_spi_freq_default;
  int ca_spi_ch = ca_spi_ch_default;
  std::string ca_ip_sendto("127.0.0.1");
  std::string ca_port_sendto("62988");
  uint32_t ca_send_samples = ca_send_samples_default;

  enum es_opt_ret {
    BUFLEN = 300,
    SEND_IP,
    SEND_PORT,
    SPI_CH,
    SPI_FREQ,
    ETH_SEND_TIMING,
    HELP = 32767
  };
  struct option es_opts[] = {
    {"help", no_argument, 0, (int)es_opt_ret::HELP},
    {"buflen", required_argument, 0, (int)es_opt_ret::BUFLEN},    
    {"spi-ch", required_argument, 0, (int)es_opt_ret::SPI_CH},
    {"spi-freq", required_argument, 0, (int)es_opt_ret::SPI_FREQ},
    {"sendto-ip", required_argument, 0, (int)es_opt_ret::SEND_IP},
    {"sendto-port", required_argument, 0, (int)es_opt_ret::SEND_PORT},
    {"sendto-timing", required_argument, 0, (int)es_opt_ret::ETH_SEND_TIMING},
    {0, 0, 0, 0}
  };

  int opt_num = 0;
  while (opt_num != -1) {
    opt_num = getopt_long(argc, argv, "", es_opts, NULL);
    switch (opt_num) {
      case (int)es_opt_ret::HELP:
        show_help();
        return 0;
      case (int)es_opt_ret::BUFLEN:
        ca_buf_len = (uint32_t)strtoul(std::string(optarg).c_str(), NULL, 0);
        if (ca_buf_len == ULONG_MAX) {
          ca_buf_len = ca_buf_len_default;
        }
        break;
      case (int)es_opt_ret::SEND_IP:
        ca_ip_sendto = std::string(optarg);
        break;
      case (int)es_opt_ret::SEND_PORT:
        ca_port_sendto = std::string(optarg);
        break;
      case (int)es_opt_ret::SPI_CH:
        ca_spi_ch = (int)strtol(std::string(optarg).c_str(), NULL, 0);
        if ((ca_spi_ch == LONG_MAX || (ca_spi_ch == LONG_MIN))) {
          ca_spi_ch = ca_spi_ch_default;
        }
        break;
      case (int)es_opt_ret::SPI_FREQ:
        ca_spi_freq = (int)strtol(std::string(optarg).c_str(), NULL, 0);
        if ((ca_spi_freq == LONG_MAX || (ca_spi_freq == LONG_MIN))) {
          ca_spi_freq = ca_spi_freq_default;
        }            
        break;
      case (int)es_opt_ret::ETH_SEND_TIMING:
        ca_send_samples = (uint32_t)strtoul(std::string(optarg).c_str(), NULL, 0);
        if (ca_send_samples == ULONG_MAX) {
          ca_send_samples = ca_send_samples_default;
        }
      default: 
        break;
    } 
  }
  
  /* I/F setup */
  SPI_rw spi_adc(ca_spi_ch, ca_spi_freq);
  network remote_host(ca_ip_sendto, ca_port_sendto, AF_INET, SOCK_DGRAM);
  ring_buffer<uint16_t> spi_data_buf(ca_buf_len);
  printf( 
    "Parameters:\n"
    " SPI buffer length: %d\n"
    " SPI channel: %d\n"
    " SPI frequency: %d\n"
    " destination host: %s:%s\n",
    ca_buf_len, ca_spi_ch, ca_spi_freq, ca_ip_sendto.c_str(), ca_port_sendto.c_str());

  /* main routine */
  int nw_con_stat = 0;
  nw_con_stat = remote_host.nw_connect();
  if (nw_con_stat > 0){
    printf("Network connection error: %s", strerror(nw_con_stat));
    return -1;
  }
  std::thread eth_send_thr(thr_send, std::ref(remote_host), std::ref(spi_data_buf), ca_send_samples);
  std::thread adc_read_thr(thr_read_spi_data, std::ref(spi_data_buf), std::ref(spi_adc), std::ref(ca_send_samples));

  printf("Press 'q' to terminate\n");
  while(!ie_exit_requested.load() && (getchar() != 'q')){}
  ie_exit_requested.store(true);
  network_force_return_req = true;

  eth_send_thr.join();
  adc_read_thr.join();

  return 0;
}
