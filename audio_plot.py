import matplotlib
matplotlib.use("TkAgg")
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

PORT = "/dev/ttyUSB3"      # CHANGE THIS
BAUD = 115200

ser = serial.Serial(PORT, BAUD)

buffer = []

def update(frame):
    global buffer

    try:
        line = ser.readline().decode().strip()
        sample = int(line)

        buffer.append(sample)

        if len(buffer) > 200:
            buffer.pop(0)

        plt.cla()
        plt.plot(buffer)
        plt.ylim(-100, 100)
        plt.title("ESP32 Microphone Signal")

    except:
        pass

fig = plt.figure()
ani = animation.FuncAnimation(fig, update, interval=10, cache_frame_data=False)
plt.show()