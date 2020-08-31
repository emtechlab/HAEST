''' Visualize accelerometer data '''

import sys

from statistics import mean, stdev
from matplotlib import pyplot as plt



def extract_data(file_path):

    with open(file_path, 'r') as f:
        data = f.readlines()

    data = [x.strip().split(' ') for x in data]

    # process data into axis information and timestamps

    # axis data
    t_ind = 6
    data_x = [int.from_bytes(bytes.fromhex(''.join(x[:2])), 'little', signed=True)/(32768/8) for x in data]
    data_y = [int.from_bytes(bytes.fromhex(''.join(x[2:4])), 'little', signed=True)/(32768/8) for x in data]
    data_z = [int.from_bytes(bytes.fromhex(''.join(x[4:6])), 'little', signed=True)/(32768/8) for x in data]

    # timestamps
    time_lo = [int.from_bytes(bytes.fromhex(''.join(x[t_ind:t_ind+2][::-1]+x[t_ind+2:t_ind+4][::-1])), 'big', signed=False) for x in data]
    time_hi = [int.from_bytes(bytes.fromhex(''.join(x[t_ind+4:t_ind+6][::-1]+x[t_ind+6:t_ind+8][::-1])), 'big', signed=False) for x in data]

    time_rec = [int(x[-1].split(".")[0]) for x in data]

    # converting timestamps to microseconds
    # time_hi is incremented once every 4294967296 counts 48 MHz
    time_stamp = []
    for ind in range(len(time_hi)):
        time_stamp.append(int((time_hi[ind]*4294967296 + time_lo[ind])/48))

    return data_z[:-1], data_y[:-1], data_x[:-1], time_stamp[:-1], time_rec



def apx(data):

	return (max(data)-min(data))


def stat(data, window_len=5):

	avg = []
	rng = []

	w = window_len
	for i, x in enumerate(data):
		if i < w:
			avg.append(x)
			rng.append(x)
		else:
			avg.append(mean(data[i-w:i]))
			rng.append(apx(data[i-w:i]))
	
	return avg, rng


def get_durations(data, time, avg, rnge, window_len=5, fac=1.5):

    meta = []

    for ind in range(window_len, len(data)-1):

        thresh_hi = avg[ind-1]+fac*rnge[ind-1]
        thresh_lo = avg[ind-1]-fac*rnge[ind-1]
        el = {}
        if (data[ind] > thresh_hi or data[ind] < thresh_lo):
            el['start'] = time[ind]
            el['final'] = time[ind+1]
            el['ind'] = ind
            meta.append(el)

    for x in meta:
        x["duration"] = int((x["final"] - x["start"])/1000)

    return meta


def filter_durations(meta):

    new = []
    for i, _ in enumerate(meta):
        if i == 0:
            new.append(meta[i])
            peak = meta[i]
        else:
            if meta[i]["ind"] - peak["ind"] < 21:
                continue
            else:
                new.append(meta[i])
                peak = meta[i]

    return new


def get_events(data_z, data_y, data_x, time, time_rec, window_len=5, fac=1.5):

    meta = {}

    avg_z, rnge_z = stat(data_z, window_len)
    avg_y, rnge_y = stat(data_y, window_len)
    avg_x, rnge_x = stat(data_x, window_len)

    peak = {}
    peak["ind"] = 0
    for ind in range(window_len, len(data_z)-1):

        thresh_hiz = avg_z[ind-1]+fac*rnge_z[ind-1]
        thresh_loz = avg_z[ind-1]-fac*rnge_z[ind-1]
        thresh_hiy = avg_y[ind-1]+fac*rnge_y[ind-1]
        thresh_loy = avg_y[ind-1]-fac*rnge_y[ind-1]
        thresh_hix = avg_x[ind-1]+fac*rnge_x[ind-1]
        thresh_lox = avg_x[ind-1]-fac*rnge_x[ind-1]

        el = {}
        if ((data_z[ind] > thresh_hiz or data_z[ind] < thresh_loz or data_y[ind] > thresh_hiy
            or data_y[ind] < thresh_loy or data_x[ind] > thresh_hix or data_x[ind] < thresh_lox)
            and ind - peak["ind"] > 21):
            el['start'] = time[ind]
            el['final'] = time[ind+1]
            el['ind'] = ind
            peak = el
            meta[time_rec[ind]] = el

    for x in meta:
        meta[x]["duration"] = int((meta[x]["final"] - meta[x]["start"])/1000)

    return meta


def process_data(file_path, window_len=0, thresh=0.5, file_path2=''):

    # convert raw data into timestamps and acceleration
    data_z, data_y, data_x, time, _ = extract_data(file_path)
    if file_path2:
        data_z2, data_y2, data_x2, time2, _ = extract_data(file_path2)

    #print(len(data_z))

    avg_z, range_z = stat(data_z, window_len)
    avg_y, range_y = stat(data_y, window_len)
    avg_x, range_x = stat(data_x, window_len)

    if file_path2:
        avg_z2, range_z2 = stat(data_z2, window_len)
        avg_y2, range_y2 = stat(data_y2, window_len)
        avg_x2, range_x2 = stat(data_x2, window_len)

    indices = range(0,len(data_z))
    if file_path2:
        indices2 = range(0,len(data_z2))
    #print(len(indices))
    ''' # preparing to plot time on two x-axis
	cut = window_len
	end_cut = len(time)
	time = time[cut:end_cut]
	start = time[0]
	indices = [int(x-start) for x in time] '''
	
    # get events when cross threshold
    dur_z = get_durations(data_z, time, avg_z, range_z, window_len, thresh)
    dur_y = get_durations(data_y, time, avg_y, range_y, window_len, thresh)
    dur_x = get_durations(data_x, time, avg_x, range_x, window_len, thresh)
    #exit()

    if file_path2:
        dur_z2 = get_durations(data_z2, time2, avg_z2, range_z2, window_len, thresh)
        dur_y2 = get_durations(data_y2, time2, avg_y2, range_y2, window_len, thresh)
        dur_x2 = get_durations(data_x2, time2, avg_x2, range_x2, window_len, thresh)

    # yielding duration graphs.
    plt.figure()
    if file_path2:
        axis = 'z'
    
        if axis == 'z':
            dur_z = filter_durations(dur_z)
            plt.subplot(211)
            plt.plot(indices, data_z, 'ro', markersize=0.5)
            for _, x in enumerate(dur_z):
                    m = data_z[x["ind"]] if data_z[x["ind"]] > 0 else data_z[x["ind"]]
                    # print(x["duration"], x["ind"], "z")
                    plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

            dur_z2 = filter_durations(dur_z2)
            plt.subplot(212)
            plt.plot(indices2, data_z2, 'ro', markersize=0.5)
            for _, x in enumerate(dur_z2):
                m = data_z2[x["ind"]] if data_z2[x["ind"]] > 0 else data_z2[x["ind"]]
                # print(x["duration"], x["ind"], "z")
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

        elif axis == 'y':
            plt.subplot(211)
            plt.plot(indices, data_y, 'go', markersize=0.5)
            for _, x in enumerate(dur_y):
                m = data_y[x["ind"]] if data_y[x["ind"]] > 0 else data_y[x["ind"]]
                # print(x["duration"], x["ind"], "y")
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

            plt.subplot(212)
            plt.plot(indices2, data_y2, 'go', markersize=0.5)
            for _, x in enumerate(dur_y2):
                m = data_y2[x["ind"]] if data_y2[x["ind"]] > 0 else data_y2[x["ind"]]
                # print(x["duration"], x["ind"], "y")
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

        else:
            plt.subplot(211)
            plt.plot(indices, data_x, 'bo', markersize=0.5)
            for _, x in enumerate(dur_x):
                # print(x["duration"], x["ind"], "z")
                m = data_x[x["ind"]] if data_x[x["ind"]] > 0 else data_x[x["ind"]]
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

            plt.subplot(212)
            plt.plot(indices2, data_x2, 'bo', markersize=0.5)
            for _, x in enumerate(dur_x2):
                # print(x["duration"], x["ind"], "z")
                m = data_x2[x["ind"]] if data_x2[x["ind"]] > 0 else data_x2[x["ind"]]
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

    else:
        plt.subplot(311)
        plt.plot(indices, data_z, 'ro', markersize=0.5)
        for _, x in enumerate(dur_z):
                m = data_z[x["ind"]] if data_z[x["ind"]] > 0 else data_z[x["ind"]]
                # print(x["duration"], x["ind"], "z")
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

        plt.subplot(312)
        plt.plot(indices, data_y, 'go', markersize=0.5)
        for _, x in enumerate(dur_y):
                m = data_y[x["ind"]] if data_y[x["ind"]] > 0 else data_y[x["ind"]]
                # print(x["duration"], x["ind"], "y")
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))
        plt.subplot(313)
        plt.plot(indices, data_x, 'bo', markersize=0.5)
        for _, x in enumerate(dur_x):
                # print(x["duration"], x["ind"], "z")
                m = data_x[x["ind"]] if data_x[x["ind"]] > 0 else data_x[x["ind"]]
                plt.text(x["ind"], m, str(x["duration"])+"-"+str(x["ind"]))

    plt.show()


def sync_time(file_path, file_path2, window_len=5, thresh=0.5):

    # process data
    data_z, data_y, data_x, time, time_rec = extract_data(file_path)
    data_z2, data_y2, data_x2, time2, time_rec2 = extract_data(file_path2)

    # get events
    events01 = get_events(data_z, data_y, data_x, time, time_rec, window_len, thresh)
    events02 = get_events(data_z2, data_y2, data_x2, time2, time_rec2, window_len, thresh)

    # sync with reference to 02 
    offsets = []
    skews = []
    rec_win = 500
       
    # get offsets
    ''' for ev in events02:
        for ev01 in events01:
            # print(ev, " ",ev01)
            if ev > ev01-rec_win and ev in range(ev01-rec_win, ev01+rec_win):
                # print(events02[ev]["ind"])
                events01[ev01]["start"] += sum(offsets)
                offsets.append(events02[ev]["start"] - events01[ev01]["start"])'''

    # make event groups
    event_pairs = []
    for ev in events02:
        for ev01 in events01:
            if ev > ev01-rec_win and ev in range(ev01-rec_win, ev01+rec_win):
                event_pairs.append((events01[ev01], events02[ev]))
    

    for ind in range(0, len(event_pairs)):
        x = event_pairs[ind][0]
        y = event_pairs[ind][1]
        # print(ind)
        if ind > 1:
            offsets.append((y["start"]-sum(offsets) - x["start"])//skews[ind-1])
            skews.append((y["start"]-event_pairs[ind-1][1]["start"])/(x["start"]-event_pairs[ind-1][0]["start"]))
        elif ind == 1:
            offsets.append(y["start"] - offsets[ind-1] - x["start"])
            skews.append((y["start"]-event_pairs[ind-1][1]["start"])/(x["start"]-event_pairs[ind-1][0]["start"]))
        elif ind == 0:
            offsets.append(y["start"] - x["start"])
            skews.append(1)
    
    cum_offset = []
    for ind, _ in enumerate(offsets):
        cum_offset.append(sum(offsets[:ind+1]))


    plt.figure()
    plt.plot(range(len(offsets)-1), offsets[1:], "r--", range(len(skews)-1), skews[1:], "gx", range(len(cum_offset)-1), cum_offset[1:], 'b.')
    plt.show()

    return offsets, skews


if __name__ == "__main__":

    # if len(sys.argv) != 2:
    #	print("usage: python3 visualize.py <data/file/path>")
    #	exit()

    #file_path = sys.argv[1]
    file_path = 'data/data_session01_floor_5_df2_exp0.txt'
    file_path2 = 'data_session02_03.txt'
    file_path2 = ''
    window_len = 21
    thresh = 2
    process_data(file_path, window_len, thresh, file_path2)
    # offsets, skews = sync_time(file_path, file_path2, window_len, thresh)
    print(offsets)
    print(skews)
    # process_data_opt(file_path)
    # _, _, _, time = extract_data(file_path)
