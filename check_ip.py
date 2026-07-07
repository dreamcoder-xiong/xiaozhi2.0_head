import serial, time
s = serial.Serial('COM8', 115200, timeout=1)
print("Listening 30s... PRESS RST BUTTON NOW on the ESP32 board!")
buf = ""
t = time.time()
while time.time() - t < 30:
    d = s.read(1024)
    if d:
        buf += d.decode('utf-8', 'ignore')
s.close()
# 找 IP / WiFi 信息
for line in buf.split('\n'):
    low = line.lower()
    if any(k in low for k in ['ip:', '192.168', 'sta', 'wifi connected', 'inet', 'dhcp', 'got ip', 'network', 'ipv4']):
        print(">>>", line.strip())
print("\n--- 启动日志尾部 ---")
print(buf[-1500:])
