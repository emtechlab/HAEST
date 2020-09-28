
import os

from matplotlib import pyplot as plt


def scale_to_sec(time):
    return [x/1000000 for x in time]


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


def decode_aud_esp(filename, data_rate):

    data_len = 33280
    frame_len = 1030

    with open(filename, 'rb') as f:
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
                if count >= 8000:
                    # print (i, j, k, count)
                    break

                base = 234
                data_point = int.from_bytes(stream[k:k+2][::-1], 'big')
                if data_point > base+5 and data_point <= base+15:
                    data_point -= 10
                elif data_point > base+15 and data_point <= base+25:
                    data_point -= 20
                if data_point > base+25 and data_point <= base+35:
                    data_point -= 30
                elif data_point > base+35:
                    data_point -= 40
                elif data_point < base-5 and data_point >= base-15:
                    data_point += 10
                elif data_point < base-15 and data_point >= base-25:
                    data_point += 20
                elif data_point < base-25 and data_point >= base-35:
                    data_point += 30
                elif data_point < base-35:
                    data_point += 40

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


def decode_opt_esp(filename, data_rate):

    data_len = 8320
    frame_len = 1540
    max_sample_count = 500

    with open(filename, 'rb') as f:
        stream = f.read()[20:]

    period = 1000000/data_rate      # in micro-seconds
    samples_len = (len(stream)//data_len)*max_sample_count
    output = (samples_len*[0], samples_len*[0])

    missing_frames = 0
    processed_frames = 0

    prev_fr_no = 0
    duplicates = 0

    for i in range(0, len(stream)-data_len, data_len):

        count = 0
        processed_frames += 1
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
            base_data = int.from_bytes(stream[j:j+2][::-1], 'big')*200*3.3/4096.0
            base_hi = int.from_bytes(stream[j+2:j+4][::-1]+stream[j+4:j+6][::-1], 'big')
            base_lo = int.from_bytes(stream[j+6:j+8][::-1]+stream[j+8:j+10][::-1], 'big')
            base_time = base_hi*(2**32)+base_lo
            #print(base_time)

            output[0][(processed_frames-1)*500+count] = base_data
            output[-1][(processed_frames-1)*500+count] = base_time
            count += 1

            for k in range(j+10, j+frame_len, 6):

                # only 8000 valid samples in space of 8192
                if count >= max_sample_count:
                    # print (i, j, k, count)
                    break

                data_point = int.from_bytes(stream[k:k+2][::-1], 'big')*200*3.3/4096.0
                print(data_point)
                off_time = int.from_bytes(stream[k+2:k+4][::-1]+stream[k+4:k+6][::-1], 'big', signed=True)
                #count += 1

                output[0][(processed_frames-1)*500+count] = data_point
                output[-1][(processed_frames-1)*500+count] = base_time-off_time
                count += 1

            if count >= 500:
                break

        prev_fr_no = stream[i+data_len-1]

    return output


def main():

    filename = '../../data-time-sync/aud_test34'
    data_rate = 800
    data = decode_aud_esp(filename, data_rate)

    filename2 = '../../data-time-sync/aud_test21'
    data_rate2 = 800
    data2 = decode_aud_esp(filename2, data_rate2)

    #filename2 = '../../data-time-sync/aud_test19'
    #data2 = decode_aud_esp(filename2, data_rate)

    plt.figure()
    #plt.subplot(2,1,1)
    #plt.plot(range(len(data[0])), data[0], 'gx')
    plt.plot(scale_to_sec(data[1]), data[0], 'g+')
    #plt.subplot(2,1,2)
    #plt.plot(range(len(data2[0])), data2[0], 'rx')
    plt.show()

    return


if __name__ == "__main__":
    
    main()