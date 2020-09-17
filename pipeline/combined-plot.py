import os
import pickle

import numpy as np

from math import sqrt
from time import time
from matplotlib import pyplot as plt


data_type = 'imu'           # 'imu', 'opt', 'aud', 'rssi'
same_same = '../../time-sync-data/{}-same-same.pkl'.format(data_type)
diff_same = '../../time-sync-data/{}-diff-same.pkl'.format(data_type)
same_diff = '../../time-sync-data/{}-same-diff.pkl'.format(data_type)
diff_diff = '../../time-sync-data/{}-diff-diff.pkl'.format(data_type)

labels = {
    'opt': {'same-same': 'Offsets 10Hz STK', 'same-diff': 'Offsets 10-5Hz STK', 'diff-same': 'Offsets 10Hz STK-ESP', 'diff-diff': 'Offsets 10-5Hz STK-ESP'},
    'imu': {'same-same': 'Offsets 200Hz STK', 'same-diff': 'Offsets 200-50Hz STK', 'diff-same': 'Offsets 200Hz STK-ESP', 'diff-diff': 'Offsets 200-50Hz STK-ESP'},
}

def scale_to_sec(time):
    return [x/1000000 for x in time]

def main():

    f = open(same_same, 'rb')
    output_1 = pickle.load(f)
    f2 = open(diff_same, 'rb')
    output_2 = pickle.load(f2)
    f3 = open(same_diff, 'rb')
    output_3 = pickle.load(f3)
    f4 = open(diff_diff, 'rb')
    output_4 = pickle.load(f4)

    # corrected_time
    time_1 = [x-output_1[1][0] for x in output_1[1]]
    time_2 = [x-output_2[1][0] for x in output_2[1]]
    time_3 = [x-output_3[1][0] for x in output_3[1]]
    time_4 = [x-output_4[1][0] for x in output_4[1]]

    '''z = np.polyfit(output_1[1][1:], output_1[0][1:], 1)
    p = np.poly1d(z)
    z2 = np.polyfit(output_2[1][1:], output_2[0][1:], 1)
    p2 = np.poly1d(z2)
    z3 = np.polyfit(output_3[1][1:], output_3[0][1:], 1)
    p3 = np.poly1d(z3)
    z4 = np.polyfit(output_4[1][1:], output_4[0][1:], 1)
    p4 = np.poly1d(z4)'''
    z = np.polyfit(time_1[1:], output_1[0][1:], 1)
    p = np.poly1d(z)
    z2 = np.polyfit(time_2[1:], output_2[0][1:], 1)
    p2 = np.poly1d(z2)
    z3 = np.polyfit(time_3[1:], output_3[0][1:], 1)
    p3 = np.poly1d(z3)
    z4 = np.polyfit(time_4[1:], output_4[0][1:], 1)
    p4 = np.poly1d(z4)

    plt.figure()
    #plt.plot(scale_to_sec(off_time[1:]), offsets[1:], 'r+', label='Raw Offsets with drift: {:.2f} ppm/sec'.format(drift))
    plt.plot(scale_to_sec(time_1[1:]), p(time_1[1:]), 'g--', label=labels[data_type]['same-same'])
    plt.plot(scale_to_sec(time_2[1:]), p2(time_2[1:]), 'bx', label=labels[data_type]['same-diff'])
    plt.plot(scale_to_sec(time_3[1:]), p3(time_3[1:]), 'm--', label=labels[data_type]['diff-same'])
    plt.plot(scale_to_sec(time_4[1:]), p4(time_4[1:]), 'y--', label=labels[data_type]['diff-diff'])
    plt.xlabel("Time (secs)")
    plt.ylabel("Offset (ppm)")
    plt.title("Device Offsets with {}".format(data_type.upper()))
    plt.legend()
    plt.show()

    return

if __name__ == "__main__":
    main()