import os
from statistics import mean
from matplotlib import pyplot as plt


def decode(file_path):
    with open(file_path, 'rb') as f:
        stream = f.read()
    pack_len = 5
    data = int(len(stream)//pack_len)*[0]
    time = int(len(stream)//pack_len)*[0]
    ign = 0
    for i in range(0, len(stream), pack_len):
        ind = int(i//pack_len)
        if ind < len(data):
            time[ind] = int.from_bytes(stream[i+1:i+5], 'little')
            #if ind > 0:
            #    print(time[ind] - time[ind-1])
            if ind > 0 and int.from_bytes(bytes([stream[i]]), 'big', signed = True) < -70:
                ign += 1
                data[ind] = data[ind-1]
                continue
            data[ind] = int.from_bytes(bytes([stream[i]]), 'big', signed = True)
            #data[ind] = stream[i]
            #print(data[ind])
    print("ignored {} samples".format(ign))
    return data, time


def decode2(file_path):
    with open(file_path, 'rb') as f:
        stream = f.read()[21:]
    frame_len = 16384
    pack_len = 9
    valid_frames = int(frame_len/pack_len)
    data = int(frame_len//pack_len)*int(len(stream)//frame_len)*[0]
    time = int(frame_len//pack_len)*int(len(stream)//frame_len)*[0]
    ign = 0
    count = 0
    ind = 0
    for i in range(0, len(stream), frame_len):
        count = 0
        for j in range(i, i+frame_len, pack_len):
            count += 1
            if count > valid_frames:
                break
            if ind < len(data):
                time[ind] = int.from_bytes(stream[j+1:j+9], 'little')
                # debugging if condition
                if ind > 0 and (time[ind] - time[ind-1] <= 0 or time[ind] - time[ind-1] > 5000):
                    #print(int.from_bytes(bytes([stream[j]]), 'big', signed = True), time[ind], time[ind-1], time[ind]-time[ind-1], ind, time[ind] - time[ind-3])
                    pass
                if ind > 0 and (int.from_bytes(bytes([stream[j]]), 'big', signed = True) < -70 or int.from_bytes(bytes([stream[j]]), 'big', signed = True) >= -10):
                    ign += 1
                    data[ind] = data[ind-1]
                    ind += 1
                    continue
                data[ind] = int.from_bytes(bytes([stream[j]]), 'big', signed = True)
                ind += 1
        #break
    print("ignored {} samples".format(ign))
    return data, time


def scale_sec(time):
    return [x/1000000 for x in time]


def get_duration(time):
    output = (len(time)-1)*[0]
    for i in range(len(time)):
        if i > 0:
            output[i-1] = time[i] - time[i-1]
    return output


def main():
    file_path = 'data/data_rssi_esp_1k_0003'
    file_path2 = 'data/data_rssi_esp_1k_0003-2'
    file_path3 = 'rssi_test14'
    data, time = decode2(file_path)
    data2, time2 = decode2(file_path2)
    #data3, time3 = decode2(file_path3)
    dur = get_duration(time)
    dur2 = get_duration(time2)
    print("duration stats, min: {}, max: {}, avg: {}.".format(min(dur), max(dur), mean(dur)))
    print("duration stats, min: {}, max: {}, avg: {}.".format(min(dur2), max(dur2), mean(dur2)))
    plt.figure()
    plt.subplot(2,1,1)
    #plt.plot(range(len(time)), time, 'g+')
    #plt.plot(range(len(data2)), data2, 'g--')
    plt.plot(scale_sec(time), data, 'b--')
    plt.subplot(2,1,2)
    #plt.plot(range(len(data2)), data2, 'r+')
    plt.plot(scale_sec(time2), data2, 'r--')
    #plt.plot(range(len(data3)), data3, 'b+')
    #plt.plot(scale_sec(time2), data2, 'r--')
    plt.show()
    return 


if __name__ == "__main__":
    main()