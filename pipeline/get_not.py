import pygatt
import time

# devices' MAC address
MAC_01 = "98:07:2D:40:25:06"
MAC_02 = "A4:34:F1:29:B5:B9"
MAC_02 = "24:6F:28:1A:D6:7E"

# enabling handles
handle_ACC = 0x2a
handle_OPT = 0x32
handle_MIC = 0x49

# enabling values
# value_ACC = bytearray([0x7F, 0x03]) 	# little endian notation used, enables acc,gyro,mag and range +/- 16g
value_ACC = bytearray([0x3F, 0x02]) 	# little endian notation used, enables acc,gyro,mag and range +/- 16g
value_OPT = bytearray([0x01])
value_MIC = bytearray([0x01, 0x00])

# read characteristics
uuid_ACC = "f000aa81-0451-4000-b000-000000000000"
uuid_ESP = "bebaa83e-36e1-4688-b7f5-ea07361b26a8"
uuid_OPT = "f000aa71-0451-4000-b000-000000000000"
uuid_MIC = "f000b002-0451-4000-b000-000000000000"


# current mode
mode = 'ACC' # "ACC" or "OPT" or "MIC"
length = 900 # 5 minutes


# data_buffer
f01 = open('data_session01.txt', 'wb')
f01.close()
f01 = open('data_session01.txt', 'ab')

f02 = open('data_session02.txt', 'wb')
f02.close()
f02 = open('data_session02.txt', 'ab')

def not_handle_01(handle, value):
	
	f01.write(value)
	return

def not_handle_02(handle, value):
	
	f02.write(value)
	return

# getting GATTTool backend
# adapter01 = pygatt.GATTToolBackend()
adapter02 = pygatt.GATTToolBackend()

# starting the GATTTool processes
# adapter01.start()
adapter02.start()

# connecting to the devices
# device01 = adapter01.connect(MAC_01)
# device01.exchange_mtu(250)
device02 = adapter02.connect(MAC_02)
updated_mtu = device02.exchange_mtu(27)
print("Updated MTU value: ", updated_mtu)

if mode == 'ACC':
	value = value_ACC
	handle = handle_ACC
	uuid = uuid_ACC
else:
	value = value_OPT
	handle = handle_OPT
	uuid = uuid_OPT

# enabling sensors
# device01.char_write_handle(handle, value)
#device01.char_write_handle(handle_OPT, value_OPT)
#device02.char_write_handle(handle, value)
#device02.char_write_handle(handle_OPT, value_OPT)

# subscribing to notifications
# device01.subscribe(uuid, callback=not_handle_01)
# device01.subscribe(uuid_OPT, callback=not_handle_01)
try:
	#device02.subscribe(uuid, callback=not_handle_02)
	device02.subscribe(uuid_ESP, callback=not_handle_02)
except Exception as e:
	print(e)

start = time.time()
lapsed = 0
while(lapsed < length):
	lapsed = time.time()-start
	continue

# adapter01.stop()
adapter02.stop()
