
import os
import pickle

import numpy as np

from math import sqrt
from time import time
from matplotlib import pyplot as plt



cdef char* cache_path = './'

def to_lux(bites):

    # fix endian-ness endian
    bites = bites[::-1]

    cdef int m = int.from_bytes(bites, 'big') & 0x0FFF
    cdef int e = (int.from_bytes(bites, 'big') & 0xF000) >> 12

    if e == 0:
        e = 1
    else:
        e = 2 << (e-1)

    return m * (0.01 * e)


def decode_opt_stk(filename, data_rate):

    # check if cached data is found
    cdef double [:, ::1] output_view
    cached = bytearray(filename)
    cached.extend(b'.pkl')
    if os.path.exists(os.path.join(cache_path, bytes(cached))):
        # if cached data found return it from here
        f = open(os.path.join(cache_path, bytes(cached)), 'rb')
        data = pickle.load(f)
        output_view = data

        return output_view

    cdef bytes stream
    with open(filename, 'rb') as f:
        stream = f.read()

    cdef int data_len = 10
    cdef float period = 1000000/data_rate      # in micro-seconds
    cdef int samples_len = len(stream)//data_len
    output = np.zeros((2, samples_len), np.double)
    #cdef long [:, ::1] output_view = output
    output_view = output

    cdef int i
    cdef long time_hi
    cdef long time_lo
    cdef double data_decoded
    cdef int ind

    for i in range(0, len(stream), data_len):

        ind = i//data_len

        data_decoded = to_lux(stream[i:i+2])
        time_lo = int.from_bytes(stream[i+2:i+4][::-1]+stream[i+4:i+6][::-1], 'big')
        time_hi = int.from_bytes(stream[i+6:i+8][::-1]+stream[i+8:i+10][::-1], 'big')

        # replacing bad data points
        if data_decoded == 0 and i > 0:
            output_view[0, ind] = output[0, ind-1]
            output_view[1, ind] = output[1, ind-1]+period

        # do normal operation
        elif data_decoded != 0:
            output_view[0, ind] = data_decoded
            output_view[1, ind] = int((time_hi*(2**32))/48 + time_lo/48)
        
        else:
            print("Houston we have a problem ...")
    
    # cache data for later use
    f = open(os.path.join(cache_path, bytes(cached)), 'wb')
    pickle.dump(output, f)

    return output_view


def decode_imu_stk(filename, data_rate):

    # check if cached data is found
    cdef double [:, ::1] output_view
    cached = bytearray(filename)
    cached.extend(b'.pkl')
    if os.path.exists(os.path.join(cache_path, bytes(cached))):
        # if cached data found return it from here
        f = open(os.path.join(cache_path, bytes(cached)), 'rb')
        data = pickle.load(f)
        output_view = data

        return output_view

    cdef int data_len = 14
    cdef int period = 1000000/data_rate      # in micro-seconds

    cdef bytes stream
    with open(filename, 'rb') as f:
        stream = f.read()

    cdef int samples_len = len(stream)//data_len
    output = np.zeros((4, samples_len), np.double)
    output_view = output

    cdef int i
    cdef int ind
    cdef float data_x
    cdef float data_y
    cdef float data_z
    cdef long time_hi
    cdef long time_lo
    for i in range(0, len(stream), data_len):

        ind = i//data_len

        # axis data
        data_x = int.from_bytes(stream[i:i+2][::-1], 'big', signed=True)/(32768/8)
        data_y = int.from_bytes(stream[i+2:i+4][::-1], 'big', signed=True)/(32768/8)
        data_z = int.from_bytes(stream[i+4:i+6][::-1], 'big', signed=True)/(32768/8)

        # timestamps
        time_hi = int.from_bytes(stream[i+6:i+8][::-1]+stream[i+8:i+10][::-1], 'big')
        time_lo = int.from_bytes(stream[i+10:i+12][::-1]+stream[i+12:i+14][::-1], 'big')

        if time_hi == 0 and time_lo == 0 and i > 0:
            output[0, ind] = output[0, ind-1]
            output[1, ind] = output[1, ind-1]
            output[2, ind] = output[2, ind-1]
            output[-1, ind] = output[-1, ind-1]+period

        elif time_hi != 0 or time_lo != 0:
            output[0, ind] = data_x
            output[1, ind] = data_y
            output[2, ind] = data_z
            output[-1, ind] = int((time_hi*(2**32))/48 + time_lo/48)
        
        else:
            print("Houston we have a problem ...")

    # cache data for later use
    f = open(os.path.join(cache_path, bytes(cached)), 'wb')
    pickle.dump(output, f)

    return output_view


def decode_imu_esp(filename, data_rate):

    # check if cached data is found
    cdef double [:, ::1] output_view
    cached = bytearray(filename)
    cached.extend(b'.pkl')
    if os.path.exists(os.path.join(cache_path, bytes(cached))):
        # if cached data found return it from here
        f = open(os.path.join(cache_path, bytes(cached)), 'rb')
        data = pickle.load(f)
        output_view = data

        return output_view

    cdef int data_len = 20
    cdef int period = 1000000/data_rate      # in micro-seconds

    cdef bytes stream
    with open(filename, 'rb') as f:
        stream = f.read()

    cdef int samples_len = len(stream)//data_len
    output = np.zeros((4, samples_len), np.double)
    output_view = output

    cdef int i
    cdef double data_x
    cdef double data_y
    cdef double data_z
    cdef long time_hi
    cdef long time_lo
    cdef int ind
    for i in range(0, len(stream), data_len):

        ind = i//data_len

        # axis data
        data_x = int.from_bytes(stream[i+6:i+8][::-1], 'big', signed=True)/(32768/8)
        data_y = int.from_bytes(stream[i+8:i+10][::-1], 'big', signed=True)/(32768/8)
        data_z = int.from_bytes(stream[i+10:i+12][::-1], 'big', signed=True)/(32768/8)

        # timestamps
        time_lo = int.from_bytes(stream[i+14:i+16][::-1]+stream[i+12:i+14][::-1], 'big')
        time_hi = int.from_bytes(stream[i+18:i+20][::-1]+stream[i+16:i+18][::-1], 'big')

        if time_hi == 0 and time_lo == 0 and i > 0:
            output[0, ind] = output[0, ind-1]
            output[1, ind] = output[1, ind-1]
            output[2, ind] = output[2, ind-1]
            output[-1, ind] = output[-1, ind-1]+period

        elif not (time_hi != 0 and time_lo != 0):
            output[0, ind] = data_x
            output[1, ind] = data_y
            output[2, ind] = data_z
            output[-1, ind] = time_hi*(2**32) + time_lo
        
        else:
            print("Houston we have a problem ...")

    # cache data for later use
    f = open(os.path.join(cache_path, bytes(cached)), 'wb')
    pickle.dump(output, f)

    return output


def decode_aud_stk(filename, data_rate):

    f = open(filename, 'rb')
    data = pickle.load(f)
    cdef double [:, ::1] data_view = data

    if data_view.shape[0] != 2:
        
        print(len(data), len(data[0]), len(data[-1][:-192]))
        print("Huoston, we have a problem !!!")
        exit(0)

    cdef int i
    for i in range(data_view.shape[1]):
        data_view[1, i] = int(data_view[1, i]/10)

    return data_view


def decode_aud_esp(char* filename, 
                   int data_rate):

    # check if cached data is found
    cdef double [:, ::1] output_view
    cached = bytearray(filename)
    cached.extend(b'.pkl')
    if os.path.exists(os.path.join(cache_path, bytes(cached))):
        # if cached data found return it from here
        f = open(os.path.join(cache_path, bytes(cached)), 'rb')
        data = pickle.load(f)
        output_view = data

        return output_view

    cdef int data_len = 33280
    cdef int frame_len = 1030
    #period = 1000000/data_rate      # in micro-seconds

    cdef bytes stream
    with open(filename, 'rb') as f:
        stream = f.read()[22:]

    #output = ([], [])
    cdef int samples_len = (len(stream)//33280)*8000
    output = np.zeros((2, samples_len), np.double)
    #cdef long [:, ::1] output_view = output
    output_view = output
    cdef int missing_frames = 0

    cdef int prev_fr_no = 0
    cdef int count = 0
    cdef int processed_frames = 0
    cdef int duplicates = 0
    cdef int i = 0

    #print(output.shape, output_view.shape)
    #exit(0)

    cdef double base_data
    cdef double data_point
    cdef long base_hi
    cdef long base_lo
    cdef int off_time
    cdef int base_time

    for i in range(0, len(stream)-data_len, data_len):

        count = 0
        processed_frames += 1
        # tracking missing time
        if i > 0 and (stream[i+data_len-1] - prev_fr_no > 1 or stream[i+data_len-1] - prev_fr_no + 256 > 1):
            missing_frames += (stream[i+data_len-1] - prev_fr_no)
            
            if stream[i+data_len-1] - prev_fr_no == 0:
                duplicates += 1
                processed_frames -= 1
                continue
            
        elif i > 0 and (stream[i+data_len-1] - prev_fr_no < 1 and stream[i+data_len-1] - prev_fr_no > -254):
            print(stream[i+data_len-1] - prev_fr_no)
            continue

        for j in range(i, i+data_len-frame_len, frame_len):
            
            # getting base timestamp
            base_data = int.from_bytes(stream[j:j+2][::-1], 'big')
            base_hi = int.from_bytes(stream[j+2:j+4][::-1]+stream[j+4:j+6][::-1], 'big')
            base_lo = int.from_bytes(stream[j+6:j+8][::-1]+stream[j+8:j+10][::-1], 'big')
            base_time = base_hi*(2**32)+base_lo

            output_view[0, (processed_frames-1)*8000+count] = base_data
            output_view[1, (processed_frames-1)*8000+count] = base_time
            count += 1

            for k in range(j+10, j+frame_len, 4):

                # only 8000 valid samples in space of 8192
                if count >= 8000:
                    # print (i, j, k, count)
                    break

                data_point = int.from_bytes(stream[k:k+2][::-1], 'big')
                off_time = int.from_bytes(stream[k+2:k+4][::-1], 'big', signed=True)

                output_view[0, (processed_frames-1)*8000+count] = data_point
                output_view[1, (processed_frames-1)*8000+count] = base_time-off_time
                count += 1

        prev_fr_no = stream[i+data_len-1]
    print("{} duplicates and missed {} frames".format(duplicates, missing_frames))

    # cache data for later use
    f = open(os.path.join(cache_path, bytes(cached)), 'wb')
    pickle.dump(output, f)

    return output_view


def decode_mot_esp():
    return

def decode_rssi_esp(filename, data_rate):
    
    # check if cached data is found
    cdef double [:, ::1] output_view
    cached = bytearray(filename)
    cached.extend(b'.pkl')
    if os.path.exists(os.path.join(cache_path, bytes(cached))):
        # if cached data found return it from here
        f = open(os.path.join(cache_path, bytes(cached)), 'rb')
        data = pickle.load(f)
        output_view = data

        return output_view

    cdef bytes stream
    with open(filename, 'rb') as f:
        stream = f.read()[21:]
    
    cdef int frame_len = 16384
    cdef int pack_len = 9
    cdef int valid_frames = int(frame_len/pack_len)
    cdef int samples_len = int(frame_len//pack_len)*int(len(stream)//frame_len)
    output = np.zeros((2, samples_len), np.double)
    output_view = output
    cdef int missing_frames = 0
    
    cdef int ign = 0
    cdef int count = 0
    cdef int ind = 0
    cdef int i, j
    cdef int base_data
    for i in range(0, len(stream), frame_len):
        
        count = 0
        for j in range(i, i+frame_len, pack_len):
            count += 1
            if count > valid_frames:
                break
            
            if ind < output_view.shape[1]:
                # decoding time
                output_view[-1, ind] = int.from_bytes(stream[j+1:j+9], 'little')
                
                # debugging if condition to be removed later
                if ind > 0 and (output_view[-1, ind] - output_view[-1, ind-1] <= 0 or output_view[-1, ind] - output_view[-1, ind-1] > 5000):
                    #print(int.from_bytes(bytes([stream[j]]), 'big', signed = True), time[ind], time[ind-1], time[ind]-time[ind-1], ind, time[ind] - time[ind-3])
                    pass
                
                base_data = int.from_bytes(bytes([stream[j]]), 'big', signed = True)
                # removing outliers
                if ind > 0 and (base_data < -70 or base_data >= -10):
                    ign += 1
                    output_view[0, ind] = output_view[0, ind-1]
                else:
                    output_view[0, ind] = base_data
                ind += 1
    print("ignored {} samples".format(ign))
    
    # cache data for later use
    f = open(os.path.join(cache_path, bytes(cached)), 'wb')
    pickle.dump(output, f)

    return output_view

data_cbs = {
    b'opt': {b'stk': decode_opt_stk},
    b'imu': {b'stk': decode_imu_stk, b'esp': decode_imu_esp},
    b'aud': {b'stk': decode_aud_stk, b'esp': decode_aud_esp},
    b'mot': {b'esp': decode_mot_esp},
    b'rssi': {b'esp': decode_rssi_esp}
}


def down_sample(double [:, ::1] data, int order):

    key = data.shape[0]
    # intializing output data structure
    output = np.zeros((key, int(data.shape[1]/order)), dtype=np.double)
    cdef double [:, ::1] output_view = output

    cdef int i, dest_ind
    for i in range(0, data.shape[1], order):

        dest_ind = int(i/order)
        if dest_ind < output_view.shape[1]:
                output_view[0, dest_ind] = data[0, i]
                output_view[-1, dest_ind] = data[-1, i]
                if key == 4:
                    output_view[1, dest_ind] = data[1, i]
                    output_view[2, dest_ind] = data[2, i]

    return output_view


# filter esp32 audio [temp]
def fix_esp_aud(double [:, ::1] data, int data_rate):

    output = np.zeros((2, data.shape[1]), np.double)
    cdef double [:, ::1] output_view = output

    cdef float period = 1000000/data_rate

    cdef int i
    cdef double t
    for i in range(data.shape[1]):
        t = data[-1, i]
        if i == 0:
            output_view[0, i] = data[0, i]
            output_view[1, i] = data[1, i]
        elif i > 0 and t - output_view[-1, i-1] > period/2:
            output_view[0, i] = data[0, i]
            output_view[1, i] = data[1, i]
        #elif t - time[i-1] < -400000:
        #    print(i, t, time[i-1], t-time[i-1])

    return output_view


# get time between samples
def get_samp_per(long [:, ::1] time, int off=20):

    output = np.zeros(time.shape[1]-1, np.intc)
    cdef int [::1] output_view = output

    cdef int i
    cdef long t
    for i in range(time.shape[1]):
        t = time[-1, i]
        if i > 0:
            output_view[i-1] = t-time[-1, i-1]

    return output_view


def fill_in_time(char* data_type, int samp_freq, double [:, ::1] data):

    if data_type == b'imu':
        
        key = 4
    
    else:
        key = 2

    cdef double [:, ::1] data_view = data
    cdef double [:, ::1] output_view

    # sampling period
    cdef float period = 1000000/samp_freq

    cdef int i
    cdef double t
    # estimating the length of final array
    cdef int final_length = 0

    for i in range(data_view.shape[1]):

        t = data_view[-1, i]
        if i == 0:
            final_length += 1

        # count the element directly if no gap is detected
        elif i > 0 and (t - data_view[-1, i-1]) < (period+period/2):
            final_length += 1
        
        # interval between consecutive samples is larger than the expected add samples in between
        elif i > 0 and (t - data_view[-1, i-1]) >= (period+period/2):
            final_length  += int((t-data_view[-1, i-1])/period)

    # declaring output structre
    output = np.zeros((key, final_length), np.double)
    output_view = output

    cdef double base_time
    cdef int count
    cdef double current_time

    # fill the gaps
    cdef int off = 0
    for i in range(data_view.shape[1]):

        t = data_view[-1, i]
        # add the first elements readily
        if i == 0:
            output_view[0, off] = data_view[0, i]
            output_view[-1, off] = data_view[-1, i]
            if data_type == b'imu':
                output_view[1, off] = data_view[1, i]
                output_view[2, off] = data_view[2, i]
            off += 1

        # add the element directly if no gap is detected
        elif i > 0 and (t - data_view[-1, i-1]) < (period+period/2):

            output_view[0, off] = data_view[0, i]
            output_view[-1, off] = data_view[-1, i]
            if data_type == b'imu':
                output_view[1, off] = data_view[1, i]
                output_view[2, off] = data_view[2, i]
            off += 1
        
        # interval between consecutive samples is larger than the expected add samples in between
        elif i > 0 and (t - data_view[-1, i-1]) >= (period+period/2):

            base_time = data_view[-1, i-1]
            count = 0

            # should continue until the gap is brigded
            while (t - base_time) > period/2:

                count += 1
                current_time = base_time + int(count*period)
                #print(t - current_time)
                if t - current_time < period+period/2:
                    break

                output_view[0, off] = data_view[0, i]
                output_view[-1, off] = current_time
                if data_type == b'imu':
                    output_view[1, off] = data_view[1, i]
                    output_view[2, off] = data_view[2, i]
                off += 1

    return output_view[:, :off].copy()


# exponentially weighted mean and corresponding st. dev.
def ewms(double [:] data, float alpha=0.5):

    stats = np.zeros((2, len(data)), dtype=np.double)
    cdef double[:, ::1] stats_view = stats
    stats_view[0, 0] = data[0]
    
    cdef int i 
    cdef double d, curr_var, prev_var = 0
    for i in range(data.shape[0]):
        d = data[i]
        if i > 0:
            stats_view[0, i] = stats_view[0, i-1] + alpha*(d - stats_view[0, i-1])
            curr_var = (1 - alpha)*(prev_var+ alpha*(d - stats_view[0, i])**2)
            stats_view[1, i] = sqrt(curr_var)
            prev_var = curr_var

    return stats


# detect events from a signal
def detect_events(double [:, ::1] data, 
                  char* data_type, 
                  int samp_freq, 
                  int thresh=1, 
                  float alpha=0.5, 
                  int data_axis=0):

    # sampling period
    # period = 1000000/data_rate

    data_time = np.array(data[-1], dtype=np.double)
    data_sens = np.array(data[data_axis], dtype=np.double)
    cdef double [::1] data_time_view = data_time
    cdef double [::1] data_sens_view = data_sens

    p2 = time()
    # getting stats on data
    stats = ewms(data_sens_view, alpha)
    cdef double[:, ::1] stats_view = stats
    p3 = time()
    print("ewma took {}".format(p3-p2))

    cdef bint debug = False
    cdef int i
    ss = np.zeros(stats.shape[1], dtype=np.double)
    ss_dual = np.zeros(stats.shape[1], dtype=np.double)
    cdef double[::1] ss_view = ss
    cdef double[::1] ss_dual_view = ss_dual
    if debug:

        for i in range(stats.shape[1]):
            ss_view[i] = stats_view[0, i] + thresh*stats_view[1, i]
            ss_dual_view[i] = stats_view[0, i] - thresh*stats_view[1, i]

        plt.figure()
        plt.plot(range(data_sens_view.shape[0]), data_sens_view, 'g--')
        plt.plot(range(ss_view.shape[0]), stats_view[0, :], 'b--')
        plt.plot(range(ss_view.shape[0]), ss_view, 'r--')
        plt.plot(range(ss_view.shape[0]), ss_dual_view, '--')
        plt.show()
        #exit()


    # two-sided
    cdef bint two_sided = False
    if data_type in [b'aud', b'opt']:
        two_sided = True
    
    # event gaps (micro-seconds) [TUNE]
    cdef int event_gap
    if data_type == b'opt':
        event_gap = 1000000
    if data_type == b'imu':
        event_gap = 2000000
    if data_type == b'aud':
        event_gap = 1000000
    if data_type == b'rssi':
        event_gap = 10000000
    
    output = np.zeros(data_sens_view.shape[0], dtype=np.intc)
    cdef int [::1] output_view = output

    # intialize with first timestamp, ignoring potential false positives in initial data-points
    p4 = time()
    i = 0
    cdef double t
    cdef double event_start = data_time[0]

    for i in range(data_time_view.shape[0]):

        t = data_time_view[i]
        if data_sens_view[i] > stats_view[0, i] + thresh*stats_view[1, i] and t - event_start > event_gap:

            event_start = t
            output_view[i] = 1

        elif two_sided and data_sens_view[i] < stats_view[0, i] - thresh*stats_view[1, i] and t - event_start > event_gap:

            event_start = t
            output_view[i] = 1
        
    p5 = time()
    print("Post ewma took {}".format(p5-p4))

    return output_view


# scale the axis
def scale_to_sec(time):
    cdef long x
    return [x/1000000 for x in time]


def process_data(char* file_path_01,
                 char* file_path_02, 
                 char* data_type, 
                 int data_rate, 
                 int order, 
                 char* device_type, 
                 char* device_type2, 
                 bint debug=True, 
                 float alpha=0.5, 
                 int offset=0):

    # decoding data
    cdef double p1 = time()
    
    cdef double [:, ::1] data = data_cbs[data_type][device_type](file_path_01, data_rate)
    cdef double p2 = time()
    print("decoding first stream took: {}".format(p2-p1))
    cdef double [:, ::1] temp_data2 = data_cbs[data_type][device_type2](file_path_02, data_rate/order)
    cdef double p3 = time()
    print("decoding second stream took: {}".format(p3-p2))

    #plt.figure()
    #plt.plot(scale_to_sec(temp_data2[-1, :]), temp_data2[0, :], 'g--')
    #plt.show()
    #exit()

    # truncate second data stream to synchronize its starting point with first stream
    cdef double [:, ::1] data2 = temp_data2[:, offset:].copy()
    cdef double p4 = time()
    print("Truncating data took: {}".format(p4-p3))

    # fixing issues with esp32 audio [gaps in timestamps]
    if data_type == b'aud' and device_type == b'esp':
        data = fix_esp_aud(data, data_rate)
    if data_type == b'aud' and device_type2 == b'esp':
        data2 = fix_esp_aud(data2, data_rate/order)
    cdef double p5 = time()
    print("Fixing esp audio took: {}".format(p5-p4))

    # fill in time gaps
    # print(data.shape, data2.shape)
    data = fill_in_time(data_type, data_rate, data)
    data2 = fill_in_time(data_type, data_rate/order, data2)

    # print(data.shape, data2.shape)
    cdef double p6 = time()
    print("Filling in time gap took: {}".format(p6-p5))

    cdef double p7 = 0.0
    cdef double p8 = 0.0    
    # downsample audio for plotting it out
    if debug and data_type == b'aud':
        data = down_sample(data, 4)
        data2 = down_sample(data2, 4)
        p7 = time()
        print("Downsampling audio took: {}".format(p7-p6))

    # downsample to match lower data rate
    elif order > 1:
        p7 = time()
        data = down_sample(data, order)
        # update data_rate value
        data_rate /= order

        p8 = time()
        print("Downsampling high frequency data took: {}".format(p8-p7))

    p8 = time()
    cdef int data_axis = 0
    cdef int data_axis2 = 0
    if data_type in [b'opt', b'aud', b'rssi']:
        data_axis = 0
        data_axis2 = 0
    elif data_type == b'imu':
        data_axis = 2               # [TUNE]
        data_axis2 = 2

    cdef float thresh = 0.0
    if data_type == b'opt':
        thresh = 2
    elif data_type == b'imu':
        thresh = 2
    elif data_type == b'aud':
        thresh = 2.15
    elif data_type == b'rssi':
        thresh = 1.8

    cdef int [::1] events
    cdef int [::1] events2

    # detect events
    events = detect_events(data, data_type, data_rate, thresh=thresh, alpha=alpha, data_axis=data_axis)
    events2 = detect_events(data2, data_type, data_rate, thresh=thresh, alpha=alpha, data_axis=data_axis2)
    cdef double p9 = time()
    print("Event Detection: {}".format(p9-p8))

    if debug:
        # getting event indices
        print([i for i, x in enumerate(events) if x == 1])
        print([i for i, x in enumerate(events2) if x == 1])

        # getting number of events detected
        print(sum([x for x in events if x == 1]))
        print(sum([x for x in events2 if x == 1]))


    # matching the two detecting events for synchronization error
    
    # sampling period
    cdef float period = 1000000/data_rate
    cdef list offsets = []
    cdef list off_time = []
    
    cdef int event_gap = 0
    cdef int search_wind = 0
    if data_type == b'opt':
        event_gap = 1000000
        search_wind = int(event_gap/period)
    elif data_type == b'imu':
        event_gap = 1000000
        search_wind = int(event_gap/period)
    elif data_type == b'aud':
        event_gap = 500000
        search_wind = int(event_gap/period)
    elif data_type == b'rssi':
        event_gap = 1000000
        search_wind = int(event_gap/period)

    cdef int i, a
    cdef int ind = 0
    cdef int current_ind = 0
    cdef int current_offset = 0
    cdef int corr_counts = 0
    
    for i in range(events.shape[0]):

        a = events[i]
        # if we have an event at given index and check for the event in the parallel stream in the given window
        if a == 1 and sum([x for x in events2[i-search_wind:i+search_wind] if x == 1]) > 0:

            ind = (i-search_wind)+[j for j in range(2*search_wind) if j < events2[i-search_wind:i+search_wind].shape[0] and events2[i-search_wind:i+search_wind][j] == 1][0]
            if not offsets:
                offsets.append(data[-1, i]-data2[-1, ind])
                off_time.append(data[-1, i])

            elif offsets:

                current_ind = ind
                current_offset = data[-1, i]-data2[-1, ind]-offsets[0]

                # if the error is greater than one sampling period fix it 
                corr_count = 0
                if current_offset > period/2:

                    while( current_offset > period/2 and corr_count <= 2*search_wind):
                        
                        current_ind += 1
                        corr_count += 1
                        current_offset = data[-1, i]-data2[-1, current_ind]-offsets[0]

                if current_offset < -period/2:

                    while( current_offset < -period/2 and corr_count <= search_wind):
                        
                        current_ind -= 1
                        corr_count += 1
                        current_offset = data[-1, i]-data2[-1, current_ind]-offsets[0]

                if current_offset <= period/2 and current_offset >= -period/2:
                    offsets.append(current_offset)
                    off_time.append(data[-1, current_ind])
                else:
                    print(current_offset)
                    print('Houston, we seem to have a problem :/')
                    # exit(0)
                    pass

            else:
                print("Houston, we have a problem ... !!!")

    cdef double p10 = time()
    print("Offset calculation took: {}".format(p10-p9))

    print('TOTAL TIME: {}'.format(p10-p1))

    cdef list per = []
    if debug:
        # getting time diff between samples (sanity check)
        #per = get_samp_per(data2, 20)
        #print("Minumum and maximum sampling intervals are {} and {} with range of {}".format(min(per), max(per), max(per)-min(per)))
        # getting data plots to visualize
        ax = plt.subplot(3, 1, 1)
        #plt.plot(scale_to_sec(data[-1]), data[0], 'g+')
        #plt.plot(scale_to_sec(data2[-1]), data2[0], 'b+')
        ax.plot(range(data.shape[1]), data[0, :], 'g+')
        ax.plot(range(data2.shape[1]), data2[0, :], 'b+')
        if data_type == b'imu':
            ax2 = plt.subplot(3, 1, 2)
            #plt.plot(scale_to_sec(data[-1]), data[0], 'g+')
            #plt.plot(scale_to_sec(data2[-1]), data2[0], 'b+')
            ax2.plot(range(len(data[1])), data[1], 'g+')
            ax2.plot(range(len(data2[1])), data2[1], 'b+')

            ax3 = plt.subplot(3, 1, 3)
            #plt.plot(scale_to_sec(data[-1]), data[0], 'g+')
            #plt.plot(scale_to_sec(data2[-1]), data2[0], 'b+')
            ax3.plot(range(len(data[2])), data[2], 'g+')
            ax3.plot(range(len(data2[2])), data2[2], 'b+')
        plt.show()

    # plot offsets
    #print(offsets)
    #print(off_time)
    cdef float drift = round((offsets[-1] - offsets[1])*1000000/(off_time[-1] - off_time[1]), 2)
    
    plt.figure()
    plt.plot(scale_to_sec(off_time[1:]), offsets[1:], 'r+', label='Raw Offsets with drift: {:.2f} ppm/sec'.format(drift))
    plt.xlabel("Time (secs)")
    plt.ylabel("Error (ppm)")
    plt.title("Synchronization Error with {} sensor at {} Hz".format(data_type.upper(), data_rate))
    plt.legend()
    plt.show()

    return


# if __name__ == "__main__":

def main():   

    ######################## OPTICAL #########################

    '''cdef char* data_type = 'opt'           # aud, opt, acc, motion, rssi
    cdef char* device_type = 'stk'         # stk, esp
    cdef char* device_type2 = 'stk'         # stk, esp
    cdef int data_rate =  10             # sampling rate (hz)
    cdef int lowest_data_rate = 5
    cdef int order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    cdef float alpha = 0.3                 # moving average co-efficient   [TUNE]
    cdef int offset = 29                 # start index of second data series     [TUNE]

    cdef char* file_path_01 = 'data/data_opt_stk_10hz_1325.txt'           # data from sensor 1
    cdef char* file_path_02 = 'data/data_opt_stk_5hz_1325.txt'           # data from sensor 2'''

    ######################## IMU #########################

    '''cdef char* data_type = 'imu'           # aud, opt, acc, motion, rssi
    cdef char* device_type = 'stk'         # stk, esp
    cdef char* device_type2 = 'stk'         # stk, esp
    cdef int data_rate =  200             # sampling rate (hz)
    cdef float alpha = 0.3                 # moving average co-efficient
    cdef int lowest_data_rate = 20
    cdef int order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    cdef int offset = 50                 # start index of second data series     [TUNE]

    cdef char* file_path_01 = 'data/data_imu_stk_200hz_1523.txt'           # data from sensor 1
    cdef char* file_path_02 = 'data/data_imu_stk_20hz_1523-2.txt'           # data from sensor 2'''

    '''######################## Audio #########################

    cdef char* data_type = 'aud'           # aud, opt, acc, motion, rssi
    cdef char* device_type = 'esp'         # stk, esp
    cdef char* device_type2 = 'stk'         # stk, esp
    cdef int data_rate =  16000             # sampling rate (hz)
    cdef float alpha = 0.3                 # moving average co-efficient
    cdef int lowest_data_rate = 16000
    cdef int order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    cdef int offset = 0                 # start index of second data series     [TUNE]

    cdef char* file_path_02 = 'data/data_aud_stk_16khz_1848.pkl'           # data from sensor 1
    cdef char* file_path_01 = 'data/data_aud_esp_16khz_1848'           # data from sensor 2
    #file_path_01 = 'data/esp_aud_test4'           # data from sensor 2'''

    ######################## RSSI #########################

    cdef char* data_type = 'rssi'           # aud, opt, acc, motion, rssi
    cdef char* device_type = 'esp'         # stk, esp
    cdef char* device_type2 = 'esp'         # stk, esp
    cdef int data_rate =  1000             # sampling rate (hz)
    cdef float alpha = 0.0001                 # moving average co-efficient
    cdef int lowest_data_rate = 1000
    cdef int order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    cdef int offset = 1300                 # start index of second data series     [TUNE]

    cdef char* file_path_02 = 'data/data_rssi_esp_1k_0003-2'           # data from sensor 1
    cdef char* file_path_01 = 'data/data_rssi_esp_1k_0003'           # data from sensor 2

    process_data(file_path_01, file_path_02, data_type, data_rate, order, device_type, device_type2, False, alpha, offset)

    return