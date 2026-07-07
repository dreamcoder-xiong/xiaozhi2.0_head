import serial, time
s = serial.Serial('COM8', 115200, timeout=1)
print("Listening on COM8 (60s)... Say '看看周围' to xiao zhi!")
t = time.time()
while time.time() - t < 60:
    d = s.read(1024)
    if d:
        print(d.decode('utf-8', 'ignore'), end='', flush=True)
print("\n--- Done ---")
s.close()
