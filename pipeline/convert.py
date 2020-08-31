import os


def get_time(data):

	t_ind = 50
	'''for i, x in enumerate(data):
		if i > (len(data)/2)-20 and i < (len(data)/2)+20 :
			print(x[50:])'''
		#if i > 20:
		#	break
	#exit()
	# timestamps
	time_hi = [int.from_bytes(bytes.fromhex(''.join(x[t_ind+2:t_ind+4][::-1]+x[t_ind:t_ind+2][::-1])), 'big', signed=False) for x in data]
	time_lo = [int.from_bytes(bytes.fromhex(''.join(x[t_ind+6:t_ind+8][::-1]+x[t_ind+4:t_ind+6][::-1])), 'big', signed=False) for x in data]

	# converting timestamps to microseconds
	# time_hi is incremented once every 4294967296 counts 48 MHz
	time_stamp = []
	for ind in range(len(time_hi)):
		time_stamp.append(int((time_hi[ind]*4294967296 + time_lo[ind])/48))

	return time_stamp


def get_durations(time):

	duration = []
	for i in range(len(time)):
		if i > 0 and time[i]-time[i-1] > 0:
			duration.append((time[i]-time[i-1])//1000)
	
	return duration


def fix_endianness(data):

	output = []
	for i, d in enumerate(data):
		if i%2 == 0 and i:
			#print(i)
			output.extend([data[i+1], d])

	return output


def main():

	input_file = 'data_audio.txt'
	output_file = 'processed_audio.txt'

	with open(input_file, 'rb') as f:
		data = f.read()

	packet_len = 58
	data_len = 50
	output_stream = bytes()

	for i in range(0, len(data), packet_len):
		output_stream += data[i:i+data_len]
	
	with open(output_file, 'wb') as f:
		f.write(output_stream)
	
	exit()

	data = [d.strip() for d in data]
	data = [d.split(" ") for d in data]

	# get time
	time  = get_time(data)
	#for i, t in enumerate(time):
	#	print(t/1000000)
	#	if i > 20:
	#		break
	#exit()
	duration = set(get_durations(time))
	print(duration)
	#exit()

	for i, d in enumerate(data):
		data[i] = fix_endianness(d)

	data = [''.join(d) for d in data]

	# identifying incomplete samples
	timestamp = ''
	new_sample = True
	# processed = []
	count = 0
	for d in data:
		#print(len(d))
		if new_sample:
			timestamp = d[100:]
			new_sample = False
		elif not new_sample and timestamp != d[100:]:
			#print(timestamp, d[100:])
			count += 1
			new_sample = True
			# timestamp = d[50:]
		else:
			new_sample = True

	print("incomplete samples: {} out of {}".format(count, len(data)))

	# original
	data = [d[:100] for d in data]
	data = ''.join(data)

	with open(output_file, 'wb') as f:
		f.write(bytearray.fromhex(data))


if __name__ == "__main__":

	main()
