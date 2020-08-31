
import os
import pickle

import numpy as np

from math import sqrt
from time import time
from matplotlib import pyplot as plt


def to_lux(bites):

    # fix endian-ness endian
    bites = bites[::-1]

    m = int.from_bytes(bites, 'big') & 0x0FFF
    e = (int.from_bytes(bites, 'big') & 0xF000) >> 12

    if e == 0:
        e = 1
    else:
        e = 2 << (e-1)

    return m * (0.01 * e)


def decode_opt_stk(filename, data_rate):
    
    data_len = 10
    period = 1000000/data_rate      # in micro-seconds

    with open(filename, 'rb') as f:
        stream = f.read()
    
    output = ([], [])
    for i in range(0, len(stream), data_len):

        data = to_lux(stream[i:i+2])
        time_lo = int.from_bytes(stream[i+2:i+4][::-1]+stream[i+4:i+6][::-1], 'big')
        time_hi = int.from_bytes(stream[i+6:i+8][::-1]+stream[i+8:i+10][::-1], 'big')

        # replacing bad data points
        if data == 0 and i > 0:
            output[0].append(output[0][-1])
            output[1].append(output[1][-1]+period)

        # do normal operation
        elif data != 0:
            output[0].append(data)
            output[1].append(int((time_hi*(2**32))/48 + time_lo/48))
        
        else:
            print("Houston we have a problem ...")
    
    return output


def decode_imu_stk(filename, data_rate):

    data_len = 14
    period = 1000000/data_rate      # in micro-seconds

    with open(filename, 'rb') as f:
        stream = f.read()

    output = ([], [], [], [])
    for i in range(0, len(stream), data_len):

        # axis data
        data_x = int.from_bytes(stream[i:i+2][::-1], 'big', signed=True)/(32768/8)
        data_y = int.from_bytes(stream[i+2:i+4][::-1], 'big', signed=True)/(32768/8)
        data_z = int.from_bytes(stream[i+4:i+6][::-1], 'big', signed=True)/(32768/8)

        # timestamps
        time_hi = int.from_bytes(stream[i+6:i+8][::-1]+stream[i+8:i+10][::-1], 'big')
        time_lo = int.from_bytes(stream[i+10:i+12][::-1]+stream[i+12:i+14][::-1], 'big')

        if time_hi == 0 and time_lo == 0 and i > 0:
            output[0].append(output[0][-1])
            output[1].append(output[1][-1])
            output[2].append(output[2][-1])
            output[-1].append(output[-1][-1]+period)

        elif time_hi != 0 and time_lo != 0:
            output[0].append(data_x)
            output[1].append(data_y)
            output[2].append(data_z)
            output[-1].append(int((time_hi*(2**32))/48 + time_lo/48))
        
        else:
            print("Houston we have a problem ...")

    return output


def decode_imu_esp(filename, data_len):

    data_len = 20
    period = 1000000/data_rate      # in micro-seconds

    with open(filename, 'rb') as f:
        stream = f.read()

    output = ([], [], [], [])
    for i in range(0, len(stream), data_len):

        # axis data
        data_x = int.from_bytes(stream[i+6:i+8][::-1], 'big', signed=True)/(32768/8)
        data_y = int.from_bytes(stream[i+8:i+10][::-1], 'big', signed=True)/(32768/8)
        data_z = int.from_bytes(stream[i+10:i+12][::-1], 'big', signed=True)/(32768/8)

        # timestamps
        time_lo = int.from_bytes(stream[i+14:i+16][::-1]+stream[i+12:i+14][::-1], 'big')
        time_hi = int.from_bytes(stream[i+18:i+20][::-1]+stream[i+16:i+18][::-1], 'big')

        if time_hi == 0 and time_lo == 0 and i > 0:
            output[0].append(output[0][-1])
            output[1].append(output[1][-1])
            output[2].append(output[2][-1])
            output[1].append(output[-1][-1]+period)

        elif not (time_hi != 0 and time_lo != 0):
            output[0].append(data_x)
            output[1].append(data_y)
            output[2].append(data_z)
            output[-1].append(time_hi*(2**32) + time_lo)
        
        else:
            print("Houston we have a problem ...")

    return output


def decode_aud_stk(filename, data_rate):

    f = open(filename, 'rb')
    data = pickle.load(f)
    #if len(data) != 2 or len(data[0]) != len(data[-1][:-192]):
    if len(data) != 2 or len(data[0]) != len(data[-1]):
        print(len(data), len(data[0]), len(data[-1][:-192]))
        print("Huoston, we have a problem !!!")
        exit(0)

    time = []
    for t in data[-1][:-192]:
        time.append(int(t//10))

    output = (data[0], time)

    return output


def decode_aud_esp(filename, data_rate):

    data_len = 33280
    frame_len = 1030
    #period = 1000000/data_rate      # in micro-seconds

    with open(filename, 'rb') as f:
        stream = f.read()[22:]

    output = ([], [])
    missing_frames = 0

    prev_fr_no = 0
    duplicates = 0

    for i in range(0, len(stream)-data_len, data_len):

        count = 0
        # tracking missing time
        if i > 0 and (stream[i+data_len-1] - prev_fr_no > 1 or stream[i+data_len-1] - prev_fr_no + 256 > 1):
            missing_frames += (stream[i+data_len-1] - prev_fr_no)
            if stream[i+data_len-1] - prev_fr_no == 0:
                duplicates += 1
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

            output[0].append(base_data)
            output[-1].append(base_time)
            count += 1

            for k in range(j+10, j+frame_len, 4):

                # only 8000 valid samples in space of 8192
                if count >= 8000:
                    # print (i, j, k, count)
                    break

                data_point = int.from_bytes(stream[k:k+2][::-1], 'big')
                off_time = int.from_bytes(stream[k+2:k+4][::-1], 'big', signed=True)
                #count += 1

                output[0].append(data_point)
                output[-1].append(base_time-off_time)
                count += 1

        prev_fr_no = stream[i+data_len-1]
        #if i > 0:
        #    break
    #print("{} duplicates and missed {} frames".format(duplicates, missing_frames))
    #exit()

    return output


def decode_mot_esp(filename, data_rate):

    data_len = 8320
    frame_len = 1030
    max_sample_count = 2000
    #period = 1000000/data_rate      # in micro-seconds

    with open(filename, 'rb') as f:
        #stream = f.read()[22:]
        stream = f.read()[20:]

    output = ([], [])
    missing_frames = 0

    prev_fr_no = 0
    duplicates = 0

    for i in range(0, len(stream)-data_len, data_len):

        count = 0
        # tracking missing time
        if i > 0 and (stream[i+data_len-1] - prev_fr_no > 1 or stream[i+data_len-1] - prev_fr_no + 256 > 1):
            missing_frames += (stream[i+data_len-1] - prev_fr_no)
            if stream[i+data_len-1] - prev_fr_no == 0:
                duplicates += 1
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

            output[0].append(base_data)
            output[-1].append(base_time)
            count += 1

            for k in range(j+10, j+frame_len, 4):

                # only 8000 valid samples in space of 8192
                if count >= max_sample_count:
                    # print (i, j, k, count)
                    break

                data_point = int.from_bytes(stream[k:k+2][::-1], 'big')
                off_time = int.from_bytes(stream[k+2:k+4][::-1], 'big', signed=True)
                #count += 1

                output[0].append(data_point)
                output[-1].append(base_time-off_time)
                count += 1

        prev_fr_no = stream[i+data_len-1]
        #if i > 0:
        #    break
    #print("{} duplicates and missed {} frames".format(duplicates, missing_frames))
    #exit()

    return output

def decode_rssi_esp():
    return

data_cbs = {
    'opt': {'stk': decode_opt_stk},
    'imu': {'stk': decode_imu_stk, 'esp': decode_imu_esp},
    'aud': {'stk': decode_aud_stk, 'esp': decode_aud_esp},
    'mot': {'esp': decode_mot_esp},
    'rssi': {'esp': decode_rssi_esp}
}


def down_sample(data, order):

    # intializing output data structure
    output = []
    for i in range(len(data)):
        output.append([])

    for i in range(0, len(data[-1]), order):

        if i < len(data[-1]):
            for j in range(len(data)):
                output[j].append(data[j][i])

    return tuple(output)


# filter esp32 audio [temp]
def fix_esp_aud(data, data_rate):

    output = ([], [])
    time = data[-1]

    period = 1000000/data_rate
    for i, t in enumerate(time):
        if i == 0:
            output[0].append(data[0][i])
            output[-1].append(data[-1][i])
        elif i > 0 and t - output[-1][-1] > period/2:
            output[0].append(data[0][i])
            output[-1].append(data[-1][i])
        #elif t - time[i-1] < -400000:
        #    print(i, t, time[i-1], t-time[i-1])
            

    return output


# get time between samples
def get_samp_per(time):

    output = []
    count = 0

    for i, t in enumerate(time):
        if i > 0:
            output.append(t-time[i-1])

    return output


def fill_in_time(data, data_type, samp_freq):


    # sampling period
    period = 1000000/samp_freq

    # getting stats on data
    if data_type in ['opt', 'aud']:
        output = ([], [])
    elif data_type == 'imu':
        output = ([], [], [], [])

    for i, t in enumerate(data[-1]):

        # add the first elements readily
        if i == 0:
            output[0].append(data[0][i])
            output[-1].append(data[-1][i])
            if data_type == 'imu':
                output[1].append(data[1][i])
                output[2].append(data[2][i])

        # add the element directly if no gap is detected
        elif i > 0 and (t - data[-1][i-1]) < (period+period/2):

            output[0].append(data[0][i])
            output[-1].append(data[-1][i])
            if data_type == 'imu':
                output[1].append(data[1][i])
                output[2].append(data[2][i])
        
        # interval between consecutive samples is larger than the expected add samples in between
        elif i > 0 and (t - data[-1][i-1]) >= (period+period/2):

            #print("Before: Filled a gap of {} with {} samples".format(t-data[-1][i-1], (t-data[-1][i-1])/period))
            #continue
            # start time

            base_time = data[-1][i-1]
            count = 0
            # should continue until the gap is brigded
            while (t - base_time) > period/2:

                count += 1
                current_time = base_time + int(count*period)
                #print(t - current_time)
                if t - current_time < period+period/2:
                    break

                output[0].append(output[0][-1])
                output[-1].append(current_time)
                if data_type == 'imu':
                    output[1].append(output[1][-1])
                    output[2].append(output[2][-1])

            #print("After: Filled a gap of {} with {} samples".format(t-data[-1][i-1], count))

    return output


# exponentially weighted mean and corresponding st. dev.
def ewms(data, alpha=0.5):

    mean = [data[0]]
    st_dev = [0]
    
    for i, d in enumerate(data):

        if i > 0:
            mean.append(mean[-1]+alpha*(d - mean[-1]))
            st_dev.append((1 - alpha)*(st_dev[-1]+ alpha*(d - mean[-1])**2))

    for i, sd in enumerate(st_dev):
        st_dev[i] = sqrt(sd)

    return (mean, st_dev)


# detect events from a signal
def detect_events(data, data_type, samp_freq, thresh=1, alpha=0.5, data_axis=0):

    # sampling period
    # period = 1000000/data_rate

    # getting stats on data
    stats = ewms(data[data_axis], alpha)

    debug = True
    if debug:

        ss = []
        for i in range(len(stats[0])):
            ss.append(stats[0][i]+thresh*stats[1][i])

        plt.figure()
        plt.plot(range(len(data[1])), data[data_axis], 'g+')
        #plt.plot(range(len(stats[0])), ss, 'b+')
        #plt.plot(range(len(stats[1])), stats[1], 'b+')
        plt.show()
        # exit()


    # two-sided
    two_sided = False
    if data_type in ['aud', 'opt']:
        two_sided = True
    
    # event gaps (micro-seconds) [TUNE]
    if data_type == 'opt':
        event_gap = 1000000
    if data_type == 'imu':
        event_gap = 2000000
    if data_type == 'aud':
        event_gap = 1000000
    
    output = []
    # intialize with first timestamp, ignoring potential false positives in initial data-points
    event_start = data[-1][0]
    for i, t in enumerate(data[-1]):

        if data[data_axis][i] > stats[0][i]+thresh*stats[1][i] and t - event_start > event_gap:

            event_start = t
            output.append(1)

        elif two_sided and data[data_axis][i] < stats[0][i]-thresh*stats[1][i] and t - event_start > event_gap:

            event_start = t
            output.append(1)
        
        else:

            output.append(0)

    return output


# scale the axis
def scale_to_sec(time):
    return [x/1000000 for x in time]


def process_data(file_path_01, file_path_02, data_type, data_rate, order, device_type, device_type2, debug=True, alpha=0.5, offset=0):

    # decoding data
    p1 = time()
    data = data_cbs[data_type][device_type](file_path_01, data_rate)
    print(len(data[0]))
    plt.figure()
    plt.plot(range(len(data[0])), data[0], 'g+')
    plt.show()
    exit()
    p2 = time()
    print("decoding first stream took: {}".format(p2-p1))
    #print(data_cbs[data_type].keys())
    temp_data2 = data_cbs[data_type][device_type2](file_path_02, data_rate/order)
    p3 = time()
    print("decoding second stream took: {}".format(p3-p2))

    # truncate second data stream to synchronize its starting point with first stream
    data2 = []
    for el in temp_data2:
        data2.append(el[offset:])
    data2 = tuple(data2)
    p4 = time()
    print("Truncating data took: {}".format(p4-p3))

    print(len(data[0]), len(data2[0]))
    if data_type == 'aud' and device_type == 'esp':
        data = fix_esp_aud(data, data_rate)
    if data_type == 'aud' and device_type2 == 'esp':
        data2 = fix_esp_aud(data2, data_rate/order)
    p5 = time()
    print("Fixing esp audio took: {}".format(p5-p4))

    # fill in time gaps
    print(len(data[0]), len(data2[0]))
    data = fill_in_time(data, data_type, data_rate)
    data2 = fill_in_time(data2, data_type, data_rate/order)
    print(len(data[0]), len(data2[0]))
    p6 = time()
    print("Filling in time gap took: {}".format(p6-p5))
    #exit()

    # downsample audio for plotting it out
    if debug and data_type == 'aud':
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
    if data_type in ['opt', 'aud']:
        data_axis = 0
        data_axis2 = 0
    elif data_type == 'imu':
        data_axis = 2               # [TUNE]
        data_axis2 = 2 

    if data_type == 'opt':
        thresh = 2
    if data_type == 'imu':
        thresh = 2
    if data_type == 'aud':
        thresh = 2.15

    # detect events
    events = detect_events(data, data_type, data_rate, thresh=thresh, alpha=alpha, data_axis=data_axis)
    events2 = detect_events(data2, data_type, data_rate, thresh=thresh, alpha=alpha, data_axis=data_axis2)
    p9 = time()
    print("Event Detection: {}".format(p9-p8))

    if debug:
        # getting event indices

        #print([i for i, x in enumerate(events) if x == 1])
        #print([i for i, x in enumerate(events2) if x == 1])

        print(sum([x for x in events if x == 1]))
        print(sum([x for x in events2 if x == 1]))


    # matching the two detecting events for synchronization error
    
    # sampling period
    period = 1000000/data_rate
    offsets = []
    off_time = []
    
    if data_type == 'opt':
        event_gap = 1000000
        search_wind = int(event_gap/period)
    elif data_type == 'imu':
        event_gap = 1000000
        search_wind = int(event_gap/period)
    elif data_type == 'aud':
        event_gap = 500000
        search_wind = int(event_gap/period)

    
    for i, a in enumerate(events):

        # if we have an event at given index and check for the event in the parallel stream in the given window
        if a == 1 and sum([x for x in events2[i-search_wind:i+search_wind] if x == 1]) > 0:

            ind = (i-search_wind)+events2[i-search_wind:i+search_wind].index(1)
            if not offsets:
                offsets.append(data[-1][i]-data2[-1][ind])
                off_time.append(data[-1][i])

            elif offsets:

                current_ind = ind
                current_offset = data[-1][i]-data2[-1][ind]-offsets[0]
                #print("Indices: {}-{} and {}-{}".format(i, data[-1][i], ind, data2[-1][ind]))
                # print(current_offset, i, data[-1][i], data2[-1][i], data[-1][i]-data2[-1][i+1]-offsets[0])
                # if the error is greater than one sampling period fixxxxx iiiiittttt 
                corr_count = 0
                if current_offset > period/2:          

                    #print("Starting point: {}".format(current_offset))
                    while( current_offset > period/2 and corr_count <= 2*search_wind):
                        
                        current_ind += 1
                        corr_count += 1
                        current_offset = data[-1][i]-data2[-1][current_ind]-offsets[0]
                        #print("Corrected point: {}".format(current_offset))

                if current_offset < -period/2:

                    #print("Starting point: {}".format(current_offset))
                    while( current_offset < -period/2 and corr_count <= search_wind):
                        
                        current_ind -= 1
                        corr_count += 1
                        current_offset = data[-1][i]-data2[-1][current_ind]-offsets[0]
                        #print("Corrected point: {}".format(current_offset))

                if current_offset <= period/2 and current_offset >= -period/2:
                    offsets.append(current_offset)
                    off_time.append(data[-1][current_ind])
                else:
                    #print(corr_count, current_offset)
                    #print('Houston, we seem to have a problem :/')
                    #exit(0)
                    pass

            else:
                print("Houston, we have a problem ... !!!")

    p10 = time()
    print("Offset calculation took: {}".format(p10-p9))

    if debug:
        # getting time diff between samples (sanity check)
        # per = get_samp_per(data[-1])
        # print("Minumum and maximum sampling intervals are {} and {} with range of {}".format(min(per), max(per), max(per)-min(per)))
        # getting data plots to visualize
        ax = plt.subplot(3, 1, 1)
        #plt.plot(scale_to_sec(data[-1]), data[0], 'g+')
        #plt.plot(scale_to_sec(data2[-1]), data2[0], 'b+')
        ax.plot(range(len(data[0])), data[0], 'g+')
        ax.plot(range(len(data2[0])), data2[0], 'b+')
        if data_type == 'imu':
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
    # print(offsets)
    # print(off_time)
    drift = round((offsets[-1] - offsets[1])*1000000/(off_time[-1] - off_time[1]), 2)
    
    plt.figure()
    plt.plot(scale_to_sec(off_time[1:]), offsets[1:], 'r+', label='Raw Offsets with drift: {} ppm/sec'.format(drift))
    plt.xlabel("Time (secs)")
    plt.ylabel("Error (ppm)")
    plt.title("Synchronization Error with {} sensor at {} Hz".format(data_type.upper(), data_rate))
    plt.legend()
    plt.show()

    return


if __name__ == "__main__":
    

    '''######################## OPTICAL #########################

    data_type = 'opt'           # aud, opt, acc, motion, rssi
    device_type = 'stk'         # stk, esp
    device_type2 = 'stk'         # stk, esp
    data_rate =  10             # sampling rate (hz)
    lowest_data_rate = 5
    order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    alpha = 0.3                 # moving average co-efficient   [TUNE]
    offset = 29                 # start index of second data series     [TUNE]

    file_path_01 = 'data/data_opt_stk_10hz_1325.txt'           # data from sensor 1
    file_path_02 = 'data/data_opt_stk_5hz_1325.txt'           # data from sensor 2'''

    '''######################## IMU #########################

    data_type = 'imu'           # aud, opt, acc, motion, rssi
    device_type = 'stk'         # stk, esp
    device_type2 = 'stk'         # stk, esp
    data_rate =  200             # sampling rate (hz)
    alpha = 0.3                 # moving average co-efficient
    lowest_data_rate = 20
    order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    offset = 50                 # start index of second data series     [TUNE]

    file_path_01 = 'data/data_imu_stk_200hz_1523.txt'           # data from sensor 1
    file_path_02 = 'data/data_imu_stk_20hz_1523-2.txt'           # data from sensor 2'''

    '''######################## Audio #########################

    data_type = 'aud'           # aud, opt, acc, motion, rssi
    device_type = 'esp'         # stk, esp
    device_type2 = 'stk'         # stk, esp
    data_rate =  16000             # sampling rate (hz)
    alpha = 0.3                 # moving average co-efficient
    lowest_data_rate = 16000
    order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    offset = 0                 # start index of second data series     [TUNE]

    file_path_02 = 'data/data_aud_stk_16khz_1848.pkl'           # data from sensor 1
    #file_path_01 = 'data/esp_aud_test5'                          # data from sensor 2
    file_path_01 = 'data/data_aud_esp_16khz_1848'               # data from sensor 2'''

    ######################## Motion #########################

    data_type = 'mot'           # aud, opt, acc, motion, rssi
    device_type = 'esp'         # stk, esp
    device_type2 = 'stk'         # stk, esp
    data_rate =  1000             # sampling rate (hz)
    alpha = 0.3                 # moving average co-efficient
    lowest_data_rate = 1000
    order = int(data_rate/lowest_data_rate)       # downsampling first device data to match lower sampling rate
    offset = 0                 # start index of second data series     [TUNE]

    file_path_02 = 'data/data_aud_stk_16khz_1848.pkl'           # data from sensor 1
    #file_path_01 = 'data/esp_aud_test5'                          # data from sensor 2
    file_path_01 = 'data/mot_test'               # data from sensor 2

    process_data(file_path_01, file_path_02, data_type, data_rate, order, device_type, device_type2, True, alpha, offset)