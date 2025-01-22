import time
import json
import os
import socket

current_date = time.strftime('%d_%m_%Y')
filename = f'espsonda_{current_date}.json'

#filename = 'new_json_test.json'

# Check if the file already exists
if os.path.exists(filename) and os.path.getsize(filename) > 0 :
    # If the file exists, load its content
    with open(filename, 'r') as file:
        json_data = json.load(file)
else:
    # If the file doesn't exist, initialize the JSON structure
    json_data = {'epochtime': [], 'board': []}

#connect to espsonda
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_address = ('192.168.137.50', 2048)
print('connecting to %s port %s' % server_address)
sock.connect(server_address)

# Append new data multiple times
for x in range(36000):
    # Append the current epoch time
    epochtime = int(time.time())
    json_data['epochtime'].append(epochtime)

    message = '?'
    sock.sendall(message.encode(encoding='utf-8'))

    time.sleep(0.1)
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
    print("New data received.")

# Write the updated JSON data back to the file
with open(filename, 'w') as file:
    json.dump(json_data, file, indent=4)

print("Data updated succesfully.")
