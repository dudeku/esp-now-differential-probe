#include "Arduino.h"
#include <SPI.h>
#include "LTC2449.h"
#include <Ethernet.h>
#include <esp_task_wdt.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoJSON.h>

#define SPI2_MOSI 11
#define SPI2_CS 10
#define SPI2_MISO 13
#define SPI2_CLK 12
#define W5500_INT 17
#define W5500_RST 18

#define SPI3_MOSI 4
#define SPI3_CS 5
#define SPI3_MISO 6
#define SPI3_CLK 7
#define LTC2449_CS SPI3_CS
#define ADC_BUSY_PIN 1
#define ADC_EXT_CLK_PIN 2

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define VSPI FSPI
#endif

#define DEBUG

// fspi = spi2 = SPI = w5500
// hspi = spi3 = ltc2448

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
static RTC_NOINIT_ATTR uint8_t ip_addr1;
static RTC_NOINIT_ATTR uint8_t ip_addr2;
static RTC_NOINIT_ATTR uint8_t ip_addr3;
static RTC_NOINIT_ATTR uint8_t ip_addr4;

// IPAddress ip(192, 168, 1, 177);
IPAddress ip;
// static RTC_NOINIT_ATTR

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
#define ETH_PORT 2048

EthernetServer server(ETH_PORT);

uint16_t adc_command = 0;
float adc_voltage = 0;
uint32_t adc_code = 0;
float LTC2449_lsb = 0.000001;
int32_t LTC2449_offset_code = 0;
int iteration = 0;
uint16_t adc_command_swipe[16];
uint32_t adc_code_swipe[16];
float adc_voltage_swipe[16];
float adc_voltage_corrected[16];

SPIClass *spi3 = NULL;

// ESP NOW structure
typedef struct struct_message
{
  uint8_t mess_int;
  float mess_float[16];
} struct_message;

struct_message mess_struct;

JsonDocument board;

int data_good = 0;
int rssi_display;

void beginSPI3()
{
  spi3 = new SPIClass(HSPI);
  spi3->begin(SPI3_CLK, SPI3_MISO, SPI3_MOSI, SPI3_CS);
  pinMode(spi3->pinSS(), OUTPUT); // HSPI SS
  // pinMode(SPI3_MISO, INPUT); // arduino framework problem 3.x not setting MISO pinmode when communicating with adc
}

void adc_command_prep()
{
  adc_command_swipe[0] = LTC2449_CH0;
  adc_command_swipe[1] = LTC2449_CH1;
  adc_command_swipe[2] = LTC2449_CH2;
  adc_command_swipe[3] = LTC2449_CH3;
  adc_command_swipe[4] = LTC2449_CH4;
  adc_command_swipe[5] = LTC2449_CH5;
  adc_command_swipe[6] = LTC2449_CH6;
  adc_command_swipe[7] = LTC2449_CH7;
  adc_command_swipe[8] = LTC2449_CH8;
  adc_command_swipe[9] = LTC2449_CH9;
  adc_command_swipe[10] = LTC2449_CH10;
  adc_command_swipe[11] = LTC2449_CH11;
  adc_command_swipe[12] = LTC2449_CH12;
  adc_command_swipe[13] = LTC2449_CH13;
  adc_command_swipe[14] = LTC2449_CH14;
  adc_command_swipe[15] = LTC2449_CH15;

  for (int i = 0; i < 15; i++)
  {
    adc_command_swipe[i] = adc_command_swipe[i] | LTC2449_OSR_8192 | LTC2449_SPEED_1X;
  }
}

void adc_command_change(int new_osr) // not working right now
{
  int set_osr = new_osr;
  // clear osr setting from command_swipe
  for (int i = 0; i < 15; i++)
  {
    adc_command_swipe[i] = adc_command_swipe[i] & 0xFF00;
  }
  // setting 32768 because of irregular register mapping
  if (new_osr == 32768)
  {
    for (int i = 0; i < 15; i++)
    {
      adc_command_swipe[i] = adc_command_swipe[i] | LTC2449_OSR_32768;
    }
    Serial.println("Changed OSR to 32768");
    return;
  }

  int osr = 0;
  while (!(new_osr % 2))
  {
    osr++;
    new_osr = new_osr >> 1;
#ifdef DEBUG
    Serial.println(new_osr);
    Serial.println(osr);
// delay(100);
#endif
  }

#ifdef DEBUG
  Serial.print("After while loop: ");
  Serial.println(new_osr);
  Serial.println(osr);
#endif

  osr = osr - 5;
  osr = osr << 4;

#ifdef DEBUG
  Serial.print("osr in hex: ");
  Serial.println(osr, HEX);
#endif

  for (int i = 0; i < 15; i++)
  {
    adc_command_swipe[i] = adc_command_swipe[i] | osr;
  }
  Serial.print("Changed OSR to ");
  Serial.println(set_osr);
  return;
}

void adc_voltage_correction()
{
  for (int i = 0; i <= 15; i++)
    adc_voltage_corrected[i] = adc_voltage_swipe[i] * 8;
}

bool adc_swipe_active_flag = 0;

void adc_data_to_json()
{
  board["board"][0]["id"] = 0;
  for (int i = 0; i < 16; i++)
  {
    String key = "CH" + String(i);
    board["board"][0]["voltages"][key] = adc_voltage_corrected[i];
  }

#ifdef DEBUG
  // serializeJsonPretty(board, Serial);
#endif
}

void adc_swipe_channels()
{
  adc_swipe_active_flag = 1;
  uint64_t a = millis(), b = 0;
  if (LTC2449_EOC_timeout(LTC2449_CS, 1000, spi3))                          // Check for EOC
    return;                                                                 // Exit if timeout is reached
  LTC2449_read(LTC2449_CS, adc_command_swipe[0], &adc_code_swipe[0], spi3); // clear buffer, send ch0 command to prepare next result

  for (int i = 0; i < 15; i++)
  {
    if (LTC2449_EOC_timeout(LTC2449_CS, 1000, spi3))                              // Check for EOC
      return;                                                                     // Exit if timeout is reached
    LTC2449_read(LTC2449_CS, adc_command_swipe[i + 1], &adc_code_swipe[i], spi3); // ch[i] result, send next chan command
    adc_voltage_swipe[i] = LTC2449_code_to_voltage(adc_code_swipe[i], LTC2449_lsb, LTC2449_offset_code);
  }
  if (LTC2449_EOC_timeout(LTC2449_CS, 1000, spi3))                           // Check for EOC
    return;                                                                  // Exit if timeout is reached
  LTC2449_read(LTC2449_CS, adc_command_swipe[0], &adc_code_swipe[15], spi3); // clear buffer, send ch0 command to prepare next result
  adc_voltage_swipe[15] = LTC2449_code_to_voltage(adc_code_swipe[15], LTC2449_lsb, LTC2449_offset_code);
  b = millis();
  adc_voltage_correction();
#ifdef DEBUG
  Serial.print("Swipe time: ");
  Serial.println(b - a);
#endif
  adc_data_to_json();
  adc_swipe_active_flag = 0;
}

void adc_ch0_test()
{
  uint16_t miso_timeout = 1000;
  adc_command = LTC2449_CH0 | LTC2449_OSR_32768 | LTC2449_SPEED_1X; // Build ADC command for channel 0
                                                                    // OSR = 32768*2 = 65536
  if (LTC2449_EOC_timeout(LTC2449_CS, miso_timeout, spi3))
  { // Check for EOC
    Serial.println("Timeout");
    return;
  } // Exit if timeout is reached
  LTC2449_read(LTC2449_CS, adc_command, &adc_code, spi3);                            // Throws out last reading
  adc_voltage = LTC2449_code_to_voltage(adc_code, LTC2449_lsb, LTC2449_offset_code); // Convert adc_code to voltage
  Serial.print("CH0: ");
  Serial.println(adc_voltage, 9);
}

void recv_data_print()
{
#ifdef DEBUG
  Serial.print("Bytes received: ");
  Serial.println(sizeof(mess_struct));
  Serial.print("Char: ");
  Serial.println(mess_struct.mess_int);
  for (int i = 0; i < 16; i++)
  {
    Serial.print("CH");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(mess_struct.mess_float[i]);
  }
  Serial.print("RSSI: ");
  Serial.println(rssi_display);
  Serial.println();
#endif
  data_good = 0;
}

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
  // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  // const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  // const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  int rssi = ppkt->rx_ctrl.rssi;
  rssi_display = rssi;
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  // Copies the sender mac address to a string
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(&mess_struct, incomingData, sizeof(mess_struct));

  /*// ESP NOW structure
  typedef struct struct_message
  {
    uint8_t mess_int;
    float mess_float[16];
  } struct_message;

  struct_message mess_struct;*/

  board["board"][mess_struct.mess_int]["id"] = mess_struct.mess_int;
  for (int i = 0; i < 16; i++)
  {
    String key = "CH" + String(i);
    board["board"][mess_struct.mess_int]["voltages"][key] = mess_struct.mess_float[i];
  }
}

uint32_t ip_addr32;

TaskHandle_t AdcSwipeHandle;

void AdcSwipeCode(void *pvParameters)
{
  for (;;)
  {
    // esp_task_wdt_reset();
    if (!adc_swipe_active_flag)
    {
      adc_swipe_channels();
    }
    delay(10);
  }
}

TaskHandle_t EthernetServerHandle;

#define WDT_TIMEOUT 5

void EthernetServerCode(void *pvParameters)
{
  for (;;)
  {
    // esp_task_wdt_reset();
    //  Serial.print("Ethsrvr from task on core: ");
    //  Serial.println(xPortGetCoreID());
    char incbuff[BUFSIZ];
    int index = 0;
    // listen for incoming clients
    EthernetClient client = server.available();
    if (client)
    {
      Serial.println("new client");
      // an HTTP request ends with a blank line
      bool currentLineIsBlank = true;
      while (client.connected())
      {
        // watchdog reset
        esp_task_wdt_reset();
        if (client.available())
        {
          esp_task_wdt_reset();
          char c = client.read();
          Serial.write(c);
          Serial.print(" ");
          if (c == '\n' && currentLineIsBlank)
          {
            // send a standard HTTP response header
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close"); // the connection will be closed after completion of the response
            client.println("Refresh: 15");       // refresh the page automatically every 5 sec
            client.println();
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            // adc_swipe_channels();
            // for (int i = 0; i < 16; i++)
            // {
            //   client.print("Channel ");
            //   client.print(i + 1);
            //   client.print(": ");
            //   client.print(adc_voltage_swipe[i], 9);
            //   client.println("<br />");
            // }
            serializeJsonPretty(board, client);
            client.println("</html>");
            break;
          }
          if (c == '?')
          {
            // client.println("PROBE_1 data: ");
            // for (int i = 0; i < 16; i++)
            // {
            //   client.print(i + 1);
            //   client.print(": ");
            //   client.println(adc_voltage_corrected[i], 6);
            // }
            serializeJson(board, client);
          }
          if (c == '\n')
          {
            // you're starting a new line
            currentLineIsBlank = true;
          }
          else if (c != '\r')
          {
            // you've gotten a character on the current line
            currentLineIsBlank = false;
          }
        }
      }
      // give the web browser time to receive the data
      delay(1);
      // close the connection:
      client.stop();
      Serial.println("client disconnected");
    }
  }
}

void setup()
{
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(EthernetServerHandle);
  esp_task_wdt_add(AdcSwipeHandle);
  esp_task_wdt_add(NULL);
  // esp_task_wdt_delete(NULL);

  ip_addr1 = 192;
  ip_addr2 = 168;
  ip_addr3 = 1;
  ip_addr4 = 177;
  ip_addr32 = (ip_addr4 << 24) + (ip_addr3 << 16) + (ip_addr2 << 8) + ip_addr1;

  ip = ip_addr32;
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  // while (!Serial)
  // {
  //   ; // wait for serial port to connect. Needed for native USB port only
  // }

  beginSPI3();

  pinMode(ADC_BUSY_PIN, INPUT);
  pinMode(ADC_EXT_CLK_PIN, OUTPUT);
  digitalWrite(ADC_EXT_CLK_PIN, LOW);

  // LTC2449_cal_voltage(0x55CC00, 0x1853177, 0, 1.5, &LTC2449_lsb, &LTC2449_offset_code);
  // LTC2449_cal_voltage(0x200D6400, 0x30000000, 0, 1.25, &LTC2449_lsb, &LTC2449_offset_code);
  LTC2449_cal_voltage(0x200D0000, 0x30000000, 0, 2.048, &LTC2449_lsb, &LTC2449_offset_code);
  adc_command_prep();

  // osr set
  delay(100);
  // adc_command_change(8196); //osr can be between 128 and 32768, so between 1<<(7+(0-8))

  delay(2000); //
  pinMode(W5500_INT, INPUT);
  pinMode(W5500_INT, PULLUP);
  pinMode(W5500_RST, OUTPUT);
  pinMode(W5500_RST, PULLUP);
  digitalWrite(W5500_RST, HIGH);

  digitalWrite(W5500_RST, LOW);
  delay(10);
  digitalWrite(W5500_RST, HIGH);
  delay(10);

  Ethernet.init(SPI2_CS);

  Serial.println("Ethernet WebServer Example");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true)
    {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected.");
  }

  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  xTaskCreatePinnedToCore(AdcSwipeCode, "AdcSwipeHandle", 10000, NULL, 1, &AdcSwipeHandle, 0);
  xTaskCreatePinnedToCore(EthernetServerCode, "EthernetServerHandle", 10000, NULL, 1, &EthernetServerHandle, 1);

  // esp now init
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
}

void loop()
{
  esp_task_wdt_reset();
#ifdef DEBUG
  Serial.print("Main loop on core: ");
  Serial.println(xPortGetCoreID());
#endif
  delay(2000);
  serializeJsonPretty(board, Serial);
  delay(2000);
  // if (data_good == 1)
  // {
  //   recv_data_print();
  // }
}
