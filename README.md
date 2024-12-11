# ESP-NOW Differential Probe Project

K. Dudek

### Framework: arduino-espressif32
>ver: 2.0.17
#### Lib Fixes:

```
in Ethernet.h
    class EthernetServer : public Server
    ->
    class EthernetServer : public Print
```

OR
```
in Ethernet.h in line 261:
	virtual void begin();
    ->
	virtual void begin(uint16_t port=0);

and in EthernetServer.cpp in line 28:
    void EthernetServer::begin()
    ->
    void EthernetServer::begin(uint16_t port_not_used)
```


#### Problems in framework version 3.x:
```
spi3 problem, not setting pinMode MISO (6)
[ ][E][esp32-hal-gpio.c:190] __digitalRead(): IO 6 is not set as GPIO.
```
### tool-esptoolpy @ 4.8.1
