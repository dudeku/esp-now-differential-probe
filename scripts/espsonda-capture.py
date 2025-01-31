import time
import json
import os
import socket

from tango import DeviceProxy, DevFailed, ConnectionFailed
from typing import Tuple
from collections import namedtuple

import sys

# Initialise tango proxy objects
bim1 = DeviceProxy("tango.machine:10000/R1-ALL/DIA/R1-ALL-DIA-BIM1")

# Named tuple, storing information about a device
Device = namedtuple(
    'Device', ['name', 'proxy', 'attribute', 'error_value']
)

devices = (
    Device('R1-ALL-DIA-BIM1', bim1, 'BeamCurrent', -1.),
)

readings = []
device_failure = []

# Fetch data from tango
for device in devices:
	try:
		reading = device.proxy.read_attribute(device.attribute).value
		print("Device server value read")
	except (DevFailed, ConnectionFailed):
		reading = device.error_value
		device_failure.append(device.name)
		print("Error reading device server")

print(reading * 1e3)

time.sleep(1)

# readings.append(reading )

#capture_t = int(sys.argv[1])
if (int(sys.argv[1]) != 0):
    capture_t = int(sys.argv[1])
else:
    capture_t = 1

if (int(sys.argv[2]) != 0):
    capture_f = int(sys.argv[2])
else:
    capture_f = 1
    
print(f"t: {capture_t}")
print(f"f: {capture_f}")


output_dir = "measurements"
os.makedirs(output_dir, exist_ok=True)

current_date = time.strftime('%H-%M-%S_%d-%m-%Y')
filename = os.path.join(output_dir, f'espsonda_{current_date}.json')
#filename = f'espsonda_{current_date}.json'

#filename = 'new_json_test.json'

# Check if the file already exists
if os.path.exists(filename) and os.path.getsize(filename) > 0 :
    # If the file exists, load its content
    with open(filename, 'r') as file:
        json_data = json.load(file)
else:
    # If the file doesn't exist, initialize the JSON structure
    json_data = {'epochtime': [],'current': [], 'board': []}

#connect to espsonda
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_address = ('192.168.137.50', 2048)
print('connecting to %s port %s' % server_address)
sock.connect(server_address)

# Append new data multiple times
for x in range(int(capture_t*capture_f)):
    # Append the current epoch time
    epochtime = int(time.time())
    timing1 = time.time()
    # print(timing1)
    json_data['epochtime'].append(epochtime)

    for device in devices:
        try:
            reading = device.proxy.read_attribute(device.attribute).value
            print("Device server value read")
        except (DevFailed, ConnectionFailed):
            reading = device.error_value
            device_failure.append(device.name)
            print("Error reading device server")

    readings.append(reading)

    json_data['current'].append(reading)

    message = '?'
    sock.sendall(message.encode(encoding='utf-8'))

    #time.sleep(0.05)
    #time.sleep(0.2)
    time.sleep(float(1/capture_f))

    data = sock.recv(512).decode('utf-8')
    
    try:
        # Parse the received data as JSON
        received = json.loads(data)
        if "board" in received:
            json_data['board'].append(received["board"])
        else:
            print("Invalid data format: 'board' key not found.")
    except json.JSONDecodeError:
        print("Error decoding JSON from received data.")
    print(time.time() - timing1)
    print(f"[{x+1}/{capture_t*capture_f}]: New data received.")


# Write the updated JSON data back to the file
with open(filename, 'w') as file:
    json.dump(json_data, file, indent=4)

print("Data updated succesfully.")
