import multiprocessing
import datetime
import os
from random import sample
import socket
import selectors
import argparse
import math

import numpy as np
import pyqtgraph

def HanningWindow(wsize):
	"""
	ref: http://www.fbs.osaka-u.ac.jp/labs/ishijima/FFT-06.html
	"""
	warray = np.zeros(wsize)
	iterator1 = np.arange(0,wsize,1)
	for i in iterator1:
		warray[i] = 0.5-(0.5*math.cos(2*math.pi*(i/(wsize-1))))
	return warray

def thr_nw_get(bind_ip: str, bind_port: int, to_plotter: multiprocessing.Queue, plot_length: int, filter_enabled: multiprocessing.Queue, filter_length: multiprocessing.Queue):
    CONV_WINDOW_SIZE = 7
    filter_toggle = False
    conv_window = HanningWindow(CONV_WINDOW_SIZE)
    PLOT_RESAMPLE_INTERVAL_NEAR = 2048
    SOCK_BUF_SIZE = 16384
    o_array = np.zeros(plot_length)
    e_rcv_sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    e_rcv_sock.bind((bind_ip, bind_port))
    print(f"Binded address: {bind_ip}\nBinded port: {bind_port}\nFIFO data length: {plot_length}")

    e_rcv_sock.setblocking(False)
    e_rcv_selector = selectors.DefaultSelector()
    e_rcv_selector.register(e_rcv_sock, selectors.EVENT_READ)
    sample_interval = 1
    if (plot_length >  PLOT_RESAMPLE_INTERVAL_NEAR):
        sample_interval  = int(plot_length /  PLOT_RESAMPLE_INTERVAL_NEAR)
        print(f"dala length: {plot_length}, interval: {sample_interval}, to plotter: {len(o_array[0::sample_interval])}samples")

    while True:
        e_rcv_selector.select()
        r_data = np.frombuffer(e_rcv_sock.recv(SOCK_BUF_SIZE), dtype=np.uint16)
        #FIFO buffer
        o_array[0:(plot_length-len(r_data))] = o_array[len(r_data):]
        o_array[(plot_length-len(r_data)):] = r_data
        if to_plotter.full():
            to_plotter.get()

        if not filter_enabled.empty():
            filter_toggle = filter_enabled.get()
        if not filter_length.empty():
            CONV_WINDOW_SIZE = filter_length.get()
            conv_window = HanningWindow(CONV_WINDOW_SIZE)

        if filter_toggle:
            temparr = np.convolve(o_array[0::sample_interval], conv_window, mode='valid') / (CONV_WINDOW_SIZE/2)
            to_plotter.put(temparr)
        else :
            to_plotter.put(o_array[0::sample_interval])

def thr_waveform_plot(print_queue: multiprocessing.Queue, f_rate: int):
    f_interval  = 1000/f_rate
    g_app = pyqtgraph.mkQApp()
    wf_L_graph = pyqtgraph.GraphicsLayoutWidget(title="ch.1")
    L_plot = wf_L_graph.addPlot()
    L_plot.setYRange(0, 1024)
    L_curve = L_plot.plot(symbol="o", symbolBrush=None, symbolPen=None, pen=(0, 255, 0))

    print(f"plot: set frame interval: {f_interval}")
    def update_data():
        try:
            data_to_plot = print_queue.get()
            L_curve.setData(data_to_plot)
        except KeyboardInterrupt:
            print("plot: KeyboardInterrupt received, exiting.")
            return
        except Exception:
            raise
    plot_timer = pyqtgraph.QtCore.QTimer()
    plot_timer.timeout.connect(update_data)
    plot_timer.start(int(f_interval))
    wf_L_graph.show()
    g_app.exec()

if __name__ == '__main__':
    plot_data_length = 8192
    plot_framerate = 60
    rcv_bind_ip = '127.0.0.1'
    rcv_bind_port = 60288

    ca_parser = argparse.ArgumentParser(description='')
    ca_parser.add_argument("--src-address",  help="IPv4 Address to bind")
    ca_parser.add_argument("--src-port", type=int, help="Port to bind")
    ca_parser.add_argument("--framerate", type=int, help="Plot framerate")
    ca_parser.add_argument("--plot-length", type=int, help="Plot data length")

    parsed_args = ca_parser.parse_args()
    parsed_args_dict = vars(parsed_args)
    if parsed_args_dict['src_address'] is not None:
        rcv_bind_ip = parsed_args_dict['src_address']
    if parsed_args_dict['src_port'] is not None:
        rcv_bind_port = parsed_args_dict['src_port']
    if parsed_args_dict['framerate'] is not None:
        plot_framerate = parsed_args_dict['framerate']
    if parsed_args_dict['plot_length'] is not None:
        plot_data_length = parsed_args_dict['plot_length']

    plot_data_queue = multiprocessing.Queue(maxsize=256)
    filter_toggle_queue = multiprocessing.Queue(maxsize=8)
    filter_length_queue = multiprocessing.Queue(maxsize=8)

    e_recv_thr = multiprocessing.Process(target=thr_nw_get, args=(rcv_bind_ip, rcv_bind_port, plot_data_queue, plot_data_length, filter_toggle_queue, filter_length_queue), daemon=True)
    plot_thr = multiprocessing.Process(target=thr_waveform_plot, args=(plot_data_queue, plot_framerate), daemon=True)
    e_recv_thr.start()
    plot_thr.start()
    try:
        comchar = ''
        pr_flen = 0
        pr_ftoggle = False
        print("Waiting for termination...")
        while comchar != 'q':
            comchar = input()
            if comchar == 'ft':
                pr_ftoggle = not pr_ftoggle
                filter_toggle_queue.put(pr_ftoggle)
                if pr_ftoggle:
                    print("Convolution filter enabled")
                filter_toggle_queue.put(pr_ftoggle)
            if comchar == 'fl':
                try:
                    pr_flen = int(input("filter length -> "))
                    filter_length_queue.put(pr_flen)
                except Exception:
                    print("illegal input")

    except KeyboardInterrupt:
        print(f" main: at {datetime.datetime.now().isoformat()}")
        print(" main: KeyboardInterrupt received, exiting.")
    except Exception:
        raise