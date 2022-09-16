#ifndef BUFFERS_H_INCLUDED
#define BUFFERS_H_INCLUDED
#include "stdint.h"
#include <cstring>
#include <mutex>
#include <thread>

template <typename DTYPE> class fifo_buffer {
  private:
    DTYPE* buffer_arr = nullptr;
    uint32_t blength = 0;

  public:
    fifo_buffer(uint32_t c_blength){
      blength = c_blength;
      buffer_arr = new DTYPE[blength];
      std::memset(buffer_arr, 0, blength);
    }
    ~fifo_buffer(){
      delete[] buffer_arr;
    }
    void put_data(DTYPE data){
      for (uint32_t tempctr = 0; tempctr < blength-1; tempctr++) {
        buffer_arr[tempctr] = buffer_arr[tempctr+1];
      }
      buffer_arr[blength-1] = data;
      return;
    }
    void put_data_arr(DTYPE* data_arr, uint32_t length) { 
      for (uint32_t tempctr=0; tempctr<length; tempctr++) {
        put_data(data_arr[tempctr]);
      }
    }
    DTYPE get_data_single(){
      return buffer_arr[0];
    }
    const DTYPE* get_data_array(){
      return buffer_arr;
    }
};

template <typename DTYPE> class ring_buffer {
  private:
    DTYPE* buffer_arr = nullptr;
    DTYPE* buf_ret_dest = nullptr;
    DTYPE* ret_nl_dest = nullptr;
    uint32_t blength = 0;
    uint32_t h_idx = 0;
    std::mutex mx_buf_guard;
    bool nl_allocated = false;
    void inline put_data_nolock(DTYPE data){
      buffer_arr[h_idx] = data;
      h_idx = (h_idx+1) % blength;
    }

  public:
    ring_buffer(uint32_t c_blength){
      blength = c_blength;
      buffer_arr = new DTYPE[blength];
      buf_ret_dest = new DTYPE[blength];
      std::memset(buffer_arr, 0, blength);
    }
    ~ring_buffer(){
      delete[] buffer_arr;
      delete[] buf_ret_dest;
      if (nl_allocated) {
        delete[] ret_nl_dest;
        ret_nl_dest = nullptr;
      }
      buffer_arr = nullptr;
      buf_ret_dest = nullptr;
    }
    uint32_t inline get_buf_length(){
      return blength;
    }
    void put_data(DTYPE data){
      std::lock_guard buf_sput_lock(mx_buf_guard);
      put_data_nolock(data);
    }
    void put_data_arr(DTYPE* data_arr, uint32_t length) {
      std::lock_guard buf_aput_lock(mx_buf_guard);
      for (uint32_t tempctr=0; tempctr<length; tempctr++) {
        put_data_nolock(data_arr[tempctr]);
      }
    }
    DTYPE get_data_single(){
      std::lock_guard buf_sget_lock(mx_buf_guard);
      return buffer_arr[h_idx];
    }
    DTYPE* get_data_nelm(uint32_t length){
      std::lock_guard buf_nget_lock(mx_buf_guard);
      ret_nl_dest = new DTYPE[length];
      nl_allocated = true;

      int temp_cpcount = 0;
      int temp_idx = h_idx;
      while (temp_cpcount < length) {
        ret_nl_dest[temp_cpcount] = buffer_arr[temp_idx];
        temp_idx = (temp_idx+1) % blength;
        temp_cpcount++;
      }
      return ret_nl_dest;
    }
    DTYPE* get_data_array(uint32_t& hidx_dest){
      std::lock_guard buf_aget_lock(mx_buf_guard);
      int tempctr = h_idx;
      int cp_count = 0;
      while (cp_count < blength) {
        buf_ret_dest[cp_count] = buffer_arr[tempctr];
        tempctr = (tempctr+1) % blength;
        cp_count++;
      }
      hidx_dest = h_idx;
      return buf_ret_dest;
    }
};
#endif