import pygatt
import time

# devices' MAC address
MAC_02 = "98:07:2D:40:25:06"
MAC_01 = "A4:34:F1:29:B5:B9"
MAC_01 = "A4:34:F1:29:BC:8A"
MAC_01 = "F0:F8:F2:86:21:87"

# enabling handles
handle_ACC = 0x27
handle_OPT = 0x2f
#handle_hum = 0x27
handle_AUD = 0x2c

# enabling values
# value_ACC = bytearray([0x7F, 0x03]) 	# little endian notation used, enables acc,gyro,mag and range +/- 16g
value_ACC = bytearray([0x38, 0x01]) 	# little endian notation used, enables acc,gyro,mag and range +/- 16g
value_OPT = bytearray([0x01])
value_hum = bytearray([0x01])

# read characteristics
uuid_ACC = "f000aa81-0451-4000-b000-000000000000"
uuid_OPT = "f000aa71-0451-4000-b000-000000000000"
uuid_hum = "f000aa21-0451-4000-b000-000000000000"
uuid_AUD = "f000b002-0451-4000-b000-000000000000"

# current mode
mode = 'AUD' # "ACC" or "OPT"
length = 900 # 5 minutes


# data_buffer
f01 = open('data_audio.txt', 'wb')
f01.close()
f01 = open('data_audio.txt', 'ab')

f02 = open('data_audio02.txt', 'wb')
f02.close()
f02 = open('data_audio02.txt', 'ab')


count = 0

def not_handle_02(handle, value):
	
	f01.write(value)
	return

def not_handle_01(handle, value):
	global count
	count += 1
	if count%150 == 0:
		print(value)
	f02.write(value)
	return

# getting GATTTool backend
adapter01 = pygatt.GATTToolBackend()
#adapter02 = pygatt.GATTToolBackend()

# starting the GATTTool processes
adapter01.start()
#adapter02.start()

# connecting to the devices
device01 = adapter01.connect(MAC_01)
#device01.exchange_mtu(250)
#device02 = adapter02.connect(MAC_02)
updated_mtu = device01.exchange_mtu(250)
print("Updated MTU value: ", updated_mtu)

if mode == 'ACC':
	value = value_ACC
	handle = handle_ACC
	uuid = uuid_ACC
elif mode == 'AUD':
	handle = handle_AUD
	uuid = uuid_AUD
else:
	value = value_hum
	handle = handle_hum
	uuid = uuid_hum

# enabling sensors

if mode != "AUD":
	# device01.char_write_handle(handle, value)
	device02.char_write_handle(handle, value)

# subscribing to notifications
device01.subscribe(uuid, callback=not_handle_01)
#device02.subscribe(uuid, callback=not_handle_01)

start = time.time()
lapsed = 0
while(lapsed < length):
	lapsed = time.time()-start
	continue

adapter01.stop()
#adapter02.stop()
