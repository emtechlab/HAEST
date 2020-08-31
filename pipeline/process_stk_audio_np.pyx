'''
 * Filename: audio_frame_serial_print.py
 *
 * Description: This tool is used to decode audio frames from the
 * CC2650ARC, the CC2650STK development kits and the CC2650 LaunchPad with 
 * CC3200AUDBOOST booster pack. These frames will saved to a wav file for 
 * playback. This script expects audio compressed in ADPCM format.
 *
 * Copyright (C) 2016-2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
'''

from struct import unpack, pack
import struct
import wave
from serial import Serial
from serial import SerialException
import time
import numpy as np
import sys
import pickle


cdef list tic1_stepsize_Lut = [
  7,    8,    9,   10,   11,   12,   13,   14, 16,   17,   19,   21,   23,   25,   28,   31,
  34,   37,   41,   45,   50,   55,   60,   66, 73,   80,   88,   97,  107,  118,  130,  143,
  157,  173,  190,  209,  230,  253,  279,  307, 337,  371,  408,  449,  494,  544,  598,  658,
  724,  796,  876,  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,10442,11487,12635,13899,
  15289,16818,18500,20350,22385,24623,27086,29794, 32767
]

cdef list tic1_IndexLut = [
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
]

cdef int SI_Dec = 0
cdef int PV_Dec = 0

def tic1_DecodeSingle(nibble):
    
    global SI_Dec
    global PV_Dec

    cdef int step = tic1_stepsize_Lut[SI_Dec]
    cdef int cum_diff  = step>>3

    SI_Dec += tic1_IndexLut[nibble]

    if SI_Dec < 0:
        SI_Dec = 0
    if SI_Dec > 88:
        SI_Dec = 88

    if nibble & 4:
        cum_diff += step
    if nibble & 2:
        cum_diff += step>>1
    if nibble & 1:
        cum_diff += step>>2

    if nibble & 8:
        if PV_Dec < (-32767+cum_diff):
           PV_Dec = -32767
        else:
            PV_Dec -= cum_diff
    else:
        if PV_Dec > (0x7fff-cum_diff):
            PV_Dec = 0x7fff
        else:
            PV_Dec += cum_diff

    return PV_Dec

cdef list decoded = []
cdef list time_series = []
cdef char* buf = ''
cdef char* input_file = 'data/data_aud_stk_16khz_1848.txt'


def decode_adpcm(_buf):
    global decoded
    global buf
    global SI_Dec
    global PV_Dec

    buf = _buf

    #print(type(_buf), len(_buf))
    #exit(0)
    for b in _buf:
        b,= unpack('B', b.to_bytes(1, 'big'))
        #print(b)
        #print(type(pack('h', tic1_DecodeSingle(b & 0xF))))
        #exit(0)
        decoded.append(pack('h', tic1_DecodeSingle(b & 0xF)))
        decoded.append(pack('h', tic1_DecodeSingle(b >> 4)))


def process_time(stream):

    cdef int extras = 192
    cdef int data_len = 50
    cdef int packet_len = 58
    cdef int std_gap = 120000

    cdef long prev_time = 0
    cdef list timestamps = ((int(len(stream)//(2*packet_len))*192)+192)*[0]
    cdef int i
    cdef long time_lo
    cdef long time_hi
    cdef long base_time
    cdef double gap
    cdef int order
    cdef int intervals
    cdef double current_time
    cdef int j

    for i in range(0, len(stream), 2*packet_len):

        time_hi = int.from_bytes(stream[i+data_len+2:i+data_len+4][::-1]+stream[i+data_len:i+data_len+2][::-1], 'big')
        time_lo = int.from_bytes(stream[i+data_len+6:i+data_len+8][::-1]+stream[i+data_len+4:i+data_len+6][::-1], 'big')
        base_time = int((time_hi*(2**32))/4.8 + time_lo/4.8)

        if i == 0:
            gap = std_gap
        elif i>0 and (base_time-prev_time) < std_gap+10000:
            gap = base_time-prev_time
        elif i>0 and (base_time-prev_time) > std_gap+10000:
            order = (base_time-prev_time)//(std_gap-10000)
            gap = (base_time-prev_time)/order

        intervals = round(gap/extras, 0)
        current_time = base_time - gap
        for j in range(extras):
            ind = (i//(2*packet_len))*extras+j
            timestamps[ind] = current_time+intervals
            current_time += intervals

        prev_time = base_time

    return timestamps


def save_wav():
    global decoded
    global input_file
    global time_series
    #cdef char* decoded_writable = b''.join(decoded)
    decoded_writable = b''.join(decoded)

    # cdef char* filename = time.strftime("pdm_test_%Y-%m-%d_%H-%M-%S_adpcm")
    filename = time.strftime("pdm_test_%Y-%m-%d_%H-%M-%S_adpcm")

    print("saving file")
    w = wave.open("" + filename + ".wav", "wb")
    w.setnchannels(1)
    w.setframerate(16000)
    w.setsampwidth(2)

    # from python 3.4 and above
    w.writeframes(decoded_writable)
    w.close()
    print("...DONE...")

    spf = wave.open(filename+'.wav', "r")

    # Extract Raw Audio from Wav File
    signal = spf.readframes(-1)
    signal = np.frombuffer(signal, np.int16)
    # final_data = (signal, time_series)
    time_s = np.array(time_series, np.double)
    final_data = np.vstack((signal.astype(np.double), time_s[:-192]))

    #print(type(input_file))
    #print(type(input_file.split('.')))
    #print(type(input_file.split(b'.')))
    #print(type(input_file.split(b'.')[0]))
    #print(type(input_file.split('.')[0]))
    #print(type(b'.pkl'))
    #print(type('wb'))
    #exit(0)
    f = open(input_file.split(b'.')[0]+b'.pkl', 'wb')
    pickle.dump(final_data, f)

    #clear stuff for next stream
    SI_Dec = 0
    PV_Dec = 0
    decoded = []
    missedFrames = 0


def main():

    global decoded
    global buf
    global SI_Dec
    global PV_Dec
    global time_series

    #cdef bytearray indata = bytearray()
    cdef bytes indata = bytes()
    cdef bytearray inbuffer = bytearray()
    cdef int frameNum = 1
    #cdef int bufLen = 100
    cdef int bufLen = 116

    cdef float lastByteTime = 0

    cdef int seqNum = 0
    cdef int prevSeqNum = 0
    cdef int missedFrames = 0

    cdef int mFramesCount = 0

    #ser = None
    cdef int readSoFar = 0
    cdef int iteration = 0
    cdef int packet_len = 58
    cdef int data_len = 50
    cdef bytes frame = bytes()
    cdef bytes timestamps = bytes()
    cdef char* input_file = b'data/data_aud_stk_16khz_1848.txt'

    cdef bytes data
    cdef int i
    cdef int start
    cdef int end

    try:
        s1 = time.time()
        with open(input_file, 'rb') as f:  # adjust file name here
            data = f.read()
        s2 = time.time()
        print("Read raw data: {}".format(s2-s1))

        #for i in range(0, len(data), packet_len):
        #    frame += data[i:i+data_len]
        #    timestamps += data[i+data_len:i+packet_len]
        s3 = time.time()
        #print("Seperating time and data: {}".format(s3-s2))

        time_series = process_time(data)
        s4 = time.time()
        print("Processed time: {}".format(s4-s3))

        while True:
            start = bufLen*iteration
            end = bufLen+bufLen*iteration
            #indata = frame[start:end]
            indata = data[start:start+data_len] + data[start+packet_len:start+packet_len+data_len]
            readSoFar = len(indata)
            
            #print("Debugging 1")

            #if end > len(frame) and len(decoded):
            if end > len(data) and len(decoded):
                # print("Debugging 2")
                # print(time.time(), lastByteTime)
                if time.time() - lastByteTime > 2:
                    #save wav file
                    s5 = time.time()
                    print("Decoding data: {}".format(s5-s4))
                    print("Missed {} frames".format(mFramesCount))
                    save_wav()
                    s6 = time.time()
                    print("Saved file: {}".format(s6-s5))
                    print("Total time: {}".format(s6-s1))
                    exit(0)
            elif indata:
                inbuffer += indata
                #print("Debugging 3")

            if len(inbuffer) == bufLen-16:
                #print("Debugging 4")
                seqNum, SI_received, PV_received = struct.unpack('BBh', inbuffer[0:4])
                seqNum = (seqNum >> 3)
                #print("Frame sequence number: %d" % seqNum)

                #print("HDR_1 local: %d, HDR_1 received: %d" % (SI_Dec, SI_received))
                #print("HDR_2 local: %d, HDR_2 received: %d" % (PV_Dec, PV_received))

                #always use received PV and SI 
                PV_Dec = PV_received
                SI_Dec = SI_received

                if seqNum > prevSeqNum:
                    missedFrames = (seqNum - prevSeqNum -1)
                else:
                    missedFrames = ((seqNum + 32) - prevSeqNum - 1)

                prevSeqNum = seqNum

                if missedFrames > 0:
                    mFramesCount += missedFrames
                    #print("######################### MISSED #########################")
                    #print(missedFrames)
                    #print("##########################################################")

                decode_adpcm(inbuffer[4:])
                inbuffer = bytearray()
                readSoFar = 0
                iteration += 1

                lastByteTime = time.time()

    except SerialException as e:
        print ("Serial port error")
        print (e)

    #finally:
    #    if ser is not None: ser.close()

    return

if __name__=="__main__":

    main()
