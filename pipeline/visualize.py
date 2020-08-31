''' Visualize accelerometer data '''

import sys

from statistics import mean, stdev
from matplotlib import pyplot as plt


def to_lux(raw):

	# little indian to big indian
	raw = raw[::-1]
	# expect 4 bytes ['XX', 'XX']
	m = int.from_bytes(bytes.fromhex('0'+(''.join(raw)[1:])), 'big', signed=False)
	e = int.from_bytes(bytes.fromhex('0'+(''.join(raw)[0])), 'big', signed=False)

	if e == 0:
		e = 1
	else:
		e = 2**(e-1)

	lux = m*(0.01*e)

	return lux


def extract_data(file_path, opt=False):

	with open(file_path, 'r') as f:
		data = f.readlines()

	data = [x.strip().split(' ') for x in data]

	# process data into axis information and timestamps

	# axis data
	t_ind = 2
	if not opt:
		t_ind = 6
		data_x = [int.from_bytes(bytes.fromhex(''.join(x[:2])), 'little', signed=True)/(32768/4) for x in data]
		data_y = [int.from_bytes(bytes.fromhex(''.join(x[2:4])), 'little', signed=True)/(32768/4) for x in data]
		data_z = [int.from_bytes(bytes.fromhex(''.join(x[4:6])), 'little', signed=True)/(32768/4) for x in data]
	else:
		data_z = [to_lux(x[:2]) for x in data]
		data_y = []
		data_x = []
	# timestamps
	time_lo = [int.from_bytes(bytes.fromhex(''.join(x[t_ind:t_ind+2][::-1]+x[t_ind+2:t_ind+4][::-1])), 'big', signed=False) for x in data]
	time_hi = [int.from_bytes(bytes.fromhex(''.join(x[t_ind+4:t_ind+6][::-1]+x[t_ind+6:][::-1])), 'big', signed=False) for x in data]

	# converting timestamps to microseconds
	# time_hi is incremented once every 4294967296 counts 48 MHz
	time_stamp = []
	for ind in range(len(time_hi)):
		time_stamp.append(int((time_hi[ind]*4294967296 + time_lo[ind])/48))

	return data_z, data_y, data_x, time_stamp


def get_durations(data, time, threshold):

	meta = []
	el = {}
	prev = False
	for ind in range(len(data)):
		# print(threshold)
		if (data[ind] > threshold or data[ind] < -1*threshold) and not prev:
			# print('well')
			el['start'] = time[ind]
			el['final'] = time[ind]
			prev = True
		elif (data[ind] > threshold or data[ind] < -1*threshold) and prev: 
		    el['final'] = time[ind]
		else:
			if prev:
				meta.append(el)
				el = {}
			prev = False

	for x in meta:
		x["duration"] = int((x["final"] - x["start"])/1000)

	return meta


def moving_avg(data, window_len, two_sided=False):

	avg = []
	if two_sided:
		lo_ind = int(window_len/2)
		hi_ind = int(window_len/2)
	else:
		lo_ind = int(window_len)
		hi_ind = 0

	for ind in range(lo_ind, len(data)-hi_ind):
		avg.append(sum(data[ind-lo_ind:ind+hi_ind+1])/window_len)

	return avg


def normalize(data):

	avg = mean(data)
	return [x-avg for x in data]


def apx(data):

	return (max(data)-min(data))/4.0


def stat(data):

	avg = []
	std = []

	w = 5
	for i, x in enumerate(data):
		if i < w:
			avg.append(x)
			std.append(x)
		else:
			avg.append(mean(data[i-w:i]))
			std.append(mean(data[i-w:i])+6*apx(data[i-w:i]))
	
	return avg, std


def process_data_opt(file_path, window_len=0, norm=True, two_sided=False):

	
	data_z, _, _, time = extract_data(file_path, opt=True)

	#if norm:
	#	data_z = normalize(data_z)

	if window_len:
		data_z = moving_avg(data_z, window_len, two_sided)
	
	# remove periodic zero readings
	data_f = []
	for ind, x in enumerate(data_z):
		if ind > 0 and ind < len(data_z)-1 and x == 0:
			data_f.append(min(data_z[ind-1], data_z[ind+1]))
		elif ind == 0 and x == 0:
			data_f.append(data_z[1])
		elif ind == len(data_z)-1 and x == 0:
			data_f.append(data_z[len(data_z)-2])
		else:
			data_f.append(x)

	indices = range(0,len(data_f))
	# changing time's reference (alternative to indices)
	'''cut = int((len(time)-len(indices))/2)
	end_cut = len(time)-cut
	time = time[cut:end_cut]
	start = time[0]
	indices = [int(x-start) for x in time]'''
	
	plt.figure()
	plt.plot(indices, data_f, 'r')
	plt.show()



def process_data(file_path, window_len=0, thresh=0.5, norm=True, two_sided=False):

	
	data_z, data_y, data_x, time = extract_data(file_path)

	std_z = stdev(data_z)*thresh
	std_y = stdev(data_y)*thresh
	std_x = stdev(data_x)*thresh

	# print(std_z, std_y, std_x)

	if norm:
		#norm_z = normalize(data_z)
		#norm_y = normalize(data_y)
		#norm_x = normalize(data_x)
		_, norm_z = stat(data_z)
		_, norm_y = stat(data_y)
		_, norm_x = stat(data_x)

	if window_len:
		data_z = moving_avg(data_z, window_len, two_sided)
		data_y = moving_avg(data_y, window_len, two_sided)
		data_x = moving_avg(data_x, window_len, two_sided)

	indices = range(0,len(data_z))
	# changing time's reference (alternative to indices)
	if two_sided:
		cut = int((len(time)-len(indices))/2)
		end_cut = len(time)-cut
		time = time[cut:end_cut]
		start = time[0]
		indices = [int(x-start) for x in time]
		if norm:
			norm_z = norm_z[cut:end_cut]
			norm_y = norm_y[cut:end_cut]
			norm_x = norm_x[cut:end_cut]
	else:
		cut = window_len
		end_cut = len(time)
		time = time[cut:end_cut]
		start = time[0]
		indices = [int(x-start) for x in time]
		if norm:
			norm_z = norm_z[cut:end_cut]
			norm_y = norm_y[cut:end_cut]
			norm_x = norm_x[cut:end_cut]
	
	# get info
	if thresh:
		knocks_z = get_durations(data_z, indices, std_z)
		knocks_y = get_durations(data_y, indices, std_y)
		knocks_x = get_durations(data_x, indices, std_x)

		marker_zp = [std_z for x in indices]
		marker_zn = [-1*std_z for x in indices]
		marker_yp = [std_y for x in indices]
		marker_yn = [-1*std_y for x in indices]
		marker_xp = [std_x for x in indices]
		marker_xn = [-1*std_x for x in indices]

	# for ind, _ in enumerate(data_z):
	# 	print(data_z[ind], '\t', data_y[ind], '\t', data_x[ind])

	plt.figure()
	plt.subplot(311)
	if thresh:
		plt.plot(indices, data_z, 'r', indices, marker_zp, 'y--', indices, marker_zn, 'y--')
		for ind, x in enumerate(knocks_z):
			if x["duration"] < 100:
				continue
			m = (-0.3*ind)%1
			plt.text(x["start"], m*std_z, str(x["duration"]))
	else:
		plt.plot(indices, data_z, 'ro', indices, norm_z, 'yo')

	plt.subplot(312)
	if thresh:
		plt.plot(indices, data_y, 'g', indices, marker_yp, 'y--', indices, marker_yn, 'y--')
		for ind, x in enumerate(knocks_y):
			if x["duration"] < 100:
				continue
			m = (-0.3*ind)%1
			plt.text(x["start"], m*std_y, str(x["duration"]))
	else:
		plt.plot(indices, data_y, 'go', indices, norm_y, 'yo')

	plt.subplot(313)
	if thresh:
		plt.plot(indices, data_x, 'bo', indices, marker_xp, 'yo', indices, marker_xn, 'y--')
		# plt.ticklabel_format(style='plain')
		for ind, x in enumerate(knocks_x):
			if x["duration"] < 100:
				continue
			m = (0.3*ind)%1
			plt.text(x["start"], 2*std_x, str(x["duration"]))
	else:
		plt.plot(indices, data_x, 'b.', indices, norm_x, 'y.')

	plt.show()


if __name__ == "__main__":

	# if len(sys.argv) != 2:
	#	print("usage: python3 visualize.py <data/file/path>")
	#	exit()

	#file_path = sys.argv[1]
	file_path = 'data_session02.txt'
	window_len = 0
	thresh = 0
	process_data(file_path, window_len, thresh, True)
	# process_data_opt(file_path)
	# _, _, _, time = extract_data(file_path)
