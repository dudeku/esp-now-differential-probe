#import serial
import time
import socket
#import sys
import json

#import pandas as pd

epochtime = int(time.time())

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#server_address = ('192.168.1.177', 2048)
server_address = ('192.168.137.50', 2048)
print('connecting to %s port %s' % server_address)
sock.connect(server_address)

#f = open('outputETH.txt','a')
#f = open('outputETHjson.json', 'a')
#f.write('{')
filename='new_out_eth.json'

with open(filename) as json_file:
    json_file = json.load(json_file)

    json_file['time'] = []
    json_file['board'] = []


    epochtime = int(time.time())
    #f.write('"time": ' + str(epochtime) + ',\n')
    #data_time = str(epochtime)
    time_json = json.loads(str(epochtime))
    json_file['time'] = [time_json]

    #
    message = '?'
    sock.sendall(message.encode(encoding='utf-8'))

    time.sleep(1)
    data = sock.recv(512)
    data = data.decode('utf-8')
    data = data[9:]
    data = data[:-1]
    print(data)
    received = json.loads(data)
    json_file['board'] = [received]

with open(filename, 'w') as filename:
    json.dump(json_file, filename)

filename='new_out_eth.json'

with open(filename) as json_file:
    json_file = json.load(json_file)

for x in range (50):
#while True:
    epochtime = int(time.time())
    #f.write('"time": ' + str(epochtime) + ',\n')
    #data_time = str(epochtime)
    time_json = json.loads(str(epochtime))
    json_file['time'].append(time_json)

    #
    message = '?'
    sock.sendall(message.encode(encoding='utf-8'))

    time.sleep(1)
    data = sock.recv(512)
    data = data.decode('utf-8')
    data = data[9:]
    data = data[:-1]
    print(data)
    received = json.loads(data)
    json_file['board'].append(received)
    #f.write(str(data) + ',\n')

print('closing socket')
sock.close()

with open(filename, 'w') as filename:
    json.dump(json_file, filename)
#print ("Received: {}".format(received))