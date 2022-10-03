import multiprocessing
import datetime
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

def thr_nw_get(bind_ip: str, bind_port: int, to_dsp: multiprocessing.Queue):
    SOCK_BUF_SIZE = 16384
    e_rcv_sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

    e_rcv_sock.bind((bind_ip, bind_port))
    print(f"Binded address: {bind_ip}\nBinded port: {bind_port}")
    e_rcv_sock.setblocking(False)
    e_rcv_selector = selectors.DefaultSelector()
    e_rcv_selector.register(e_rcv_sock, selectors.EVENT_READ)
    while True:
        e_rcv_selector.select()
        r_data = np.frombuffer(e_rcv_sock.recv(SOCK_BUF_SIZE), dtype=np.uint16)
        if to_dsp.full():
            to_dsp.get()
        to_dsp.put(r_data)

fliter_toggle = False
conv_window = []
def thr_dsp(from_nw: multiprocessing.Queue, to_plotter: multiprocessing.Queue, plot_length: int, filter_enabled: multiprocessing.Queue, filter_length: multiprocessing.Queue, resample_length: multiprocessing.Queue):
    global filter_toggle, conv_window
    CONV_WINDOW_SIZE = 7
    RESAMPLE_LENGTH_DEFAULT = 2048
    filter_toggle = False
    conv_window = HanningWindow(CONV_WINDOW_SIZE)
    o_array = np.zeros(plot_length)

    sample_interval = 1
    if (plot_length >= RESAMPLE_LENGTH_DEFAULT):
        sample_interval  = int(plot_length / RESAMPLE_LENGTH_DEFAULT)
        print(f"data length: {plot_length}, interval: {sample_interval}, to plotter: {len(o_array[0::sample_interval])}samples")

    print(f"FIFO data length: {plot_length}")
    while True:
        r_data = from_nw.get()
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
        if not resample_length.empty():
            rs_glength = resample_length.get()
            if (plot_length >= rs_glength):
                sample_interval  = int(plot_length / rs_glength)
                print(f"data length: {plot_length}, interval: {sample_interval}, to plotter: {len(o_array[0::sample_interval])}samples")
            else:
                sample_interval = 1
                print(f"data length: {plot_length}, interval: {sample_interval}, to plotter: {len(o_array[0::sample_interval])}samples")
        else :
            to_plotter.put(o_array[0::sample_interval])

def thr_waveform_plot(print_queue: multiprocessing.Queue, f_rate: int, y_range:multiprocessing.Queue):
    f_interval  = 1000/f_rate
    g_app = pyqtgraph.mkQApp()
    wf_L_graph = pyqtgraph.GraphicsLayoutWidget(title="ch.1")
    L_plot = wf_L_graph.addPlot()
    L_plot.setYRange(0, 1024)
    L_curve = L_plot.plot(symbol=None, symbolBrush=None, symbolPen=None, pen=(0, 255, 0))

    print(f"plot: set frame interval: {f_interval}")
    def update_data():
        try:
            if not y_range.empty():
                got_range = y_range.get()
                L_plot.setYRange(got_range[0], got_range[1])
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
    ca_parser.add_argument("--bind-address",  help="IPv4 Address to bind")
    ca_parser.add_argument("--bind-port", type=int, help="Port to bind")
    ca_parser.add_argument("--framerate", type=int, help="Plot framerate")
    ca_parser.add_argument("--plot-length", type=int, help="Plot data length")

    parsed_args = ca_parser.parse_args()
    parsed_args_dict = vars(parsed_args)
    if parsed_args_dict['bind_address'] is not None:
        rcv_bind_ip = parsed_args_dict['bind_address']
    if parsed_args_dict['bind_port'] is not None:
        rcv_bind_port = parsed_args_dict['bind_port']
    if parsed_args_dict['framerate'] is not None:
        plot_framerate = parsed_args_dict['framerate']
    if parsed_args_dict['plot_length'] is not None:
        plot_data_length = parsed_args_dict['plot_length']

    nw2dsp_queue = multiprocessing.Queue(maxsize=256)
    plot_data_queue = multiprocessing.Queue(maxsize=8)
    filter_toggle_queue = multiprocessing.Queue(maxsize=8)
    filter_length_queue = multiprocessing.Queue(maxsize=8)
    y_range_queue = multiprocessing.Queue(maxsize=8)
    resample_queue = multiprocessing.Queue(maxsize=8)

    y_range = [0, 1024]

    e_recv_thr = multiprocessing.Process(target=thr_nw_get, args=(rcv_bind_ip, rcv_bind_port, nw2dsp_queue), daemon=True)
    dsp_thr = multiprocessing.Process(target=thr_dsp, args=(nw2dsp_queue, plot_data_queue, plot_data_length, filter_toggle_queue, filter_length_queue, resample_queue), daemon=True)
    plot_thr = multiprocessing.Process(target=thr_waveform_plot, args=(plot_data_queue, plot_framerate, y_range_queue), daemon=True)

    e_recv_thr.start()
    dsp_thr.start()
    plot_thr.start()
    print(f"debug: PID\n NW receive{e_recv_thr.pid}\n DSP:{dsp_thr.pid}\n PLOT:{plot_thr.pid}")

    try:
        comchar = ''
        pr_flen = 0
        pr_ftoggle = False
        print("Waiting for termination...")
        while comchar != 'q':
            comchar = input("command > ")
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
            if comchar == 'rs':
                try:
                    g_num = int(input("resample length -> "))
                    if g_num < 1:
                        g_num = 1
                    resample_queue.put(g_num)
                except Exception:
                    print("illegal input")
            if comchar == 'yr':
                try:
                    y_range[0] = int(input(f"y range min (current: {y_range[0]}) -> "))
                except Exception:
                    print("illegal input")

                try:
                    y_range[1] = int(input(f"y range max (current: {y_range[1]}) -> "))
                except Exception:
                    print("illegal input")
                y_range_queue.put(y_range)

    except KeyboardInterrupt:
        print(f" main: at {datetime.datetime.now().isoformat()}")
        print(" main: KeyboardInterrupt received, exiting.")
    except Exception:
        raise