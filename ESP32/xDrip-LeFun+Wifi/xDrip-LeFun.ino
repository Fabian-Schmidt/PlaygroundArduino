// Bluetooth LE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// WIFI
//#include <WiFi.h>
#include <esp_wifi.h>

// EPaper
#include <GxEPD.h>
#include <GxGDEW0154Z04/GxGDEW0154Z04.h> // 1.54" b/w/r 200x200
// FreeFonts from Adafruit_GFX
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "FreeSansBold50pt7b.h"

// EPaper helper
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

// Nightscout Client
#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include "Nightscout.h"

// for SPI pin definitions see e.g.:
// C:\Users\xxx\Documents\Arduino\hardware\espressif\esp32\variants\lolin32\pins_arduino.h

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);        // arbitrary selection of (16), 4

#if defined(_GxGDEW0154Z04_H_)
#define HAS_RED_COLOR
#endif

#define DEBUG false
#define DISPLAY_HEIGHT 200
#define DISPLAY_WIDTH 200
#define DISPLAY_MARGIN 8
#define DISPLAY_VALUE_FONT FreeSansBold50pt7b
#define DISPLAY_VALUE_FONT_HEIGHT 50
#define DISPLAY_TIME_FONT FreeMonoBold24pt7b
#define DISPLAY_TIME_FONT_HEIGHT 24
#define DISPLAY_TIME_FONT_WIDTH 26

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */

#if DEBUG

#define TIME_TO_SLEEP 10  /* Time ESP32 will go to sleep (in seconds) */
#define VALUE_OLD_TIME 15 /* Value will be crossed out / marked as too old. */

#else

#define TIME_TO_SLEEP 270    /* Time ESP32 will go to sleep (in seconds) 4 minutes and 30 seconds */
#define VALUE_OLD_TIME 39600 /* 11 mintues. Value will be crossed out / marked as too old. */

#endif

// EPaper V-Low (3.3V), BS1-Low (4-Line)
//   EPaper       ESP32
//      VCC ----> 3v3 (3.3V)
//      GND ----> GND
//      SDI ----> GPIO 23 MOSI
//     SCLK ----> GPIO 18 SCK (SPI Clock)
//       CS ----> GPIO  5 Chip Select
//      D/C ----> GPIO 17 Data/Command select
//    Reset ----> GPIO 16 Reset
//     Busy ----> GPIO  4

//#define RES  16
//#define DC   17
//#define CS    5

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
//const int utcOffset = 1; // Central European Time
const int utcOffset = 11; // GMT +11 (Melbourne)
#define SECS_PER_HOUR (3600UL);
#define SERVICE_UUID "000018d0-0000-1000-8000-00805f9b34fb"           // LeFun
#define CHARACTERISTIC_UUID_RX "00002d01-0000-1000-8000-00805f9b34fb" // REPLY_CHARACTERISTIC
#define CHARACTERISTIC_UUID_TX "00002d00-0000-1000-8000-00805f9b34fb" // WRITE_CHARACTERISTIC

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

uint8_t calculateCRC(const uint8_t input[], byte length)
{
  uint8_t result = 0;
  int index = 0;
  int bit = 0;

  for (index = 0; index < length; index++)
  {
    for (bit = 0; bit < 8; bit++)
    {
      result = ((input[index] >> bit ^ result) & 1) == 0 ? result >> 1 : (result ^ 0x18) >> 1 | 0x80;
    }
  }
  return result;
}

void printHexValue(std::string data)
{
#if DEBUG
  Serial.print(F("HEX Value:"));
  for (int i = 0; i < data.length(); i++)
  {
    Serial.print(F(" 0x"));
    Serial.print(data[i], HEX);
  }
  Serial.println();
#endif
}

void printHexValue(uint8_t data[], byte length)
{
#if DEBUG
  Serial.print(F("- "));
  for (int i = 0; i < length; i++)
  {
    Serial.print(F(" 0x"));
    Serial.print(data[i], HEX);
  }
  Serial.println();
#endif
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason()
{
#if DEBUG
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
#endif
}

RTC_DATA_ATTR time_t last_time = 0x00;
RTC_DATA_ATTR time_t last_time_too_old = 0x00;
RTC_DATA_ATTR bool current_value_isOld = false;
RTC_DATA_ATTR float last_val = 0x00;
RTC_DATA_ATTR bool last_source_bt = false;
RTC_DATA_ATTR uint8_t last_hour = 0x00;
RTC_DATA_ATTR uint8_t last_minute = 0x00;
RTC_DATA_ATTR uint8_t last_second = 0x00;

void displayValues(float val, bool source_bt, uint8_t hour, uint8_t minute, uint8_t second, bool isOld)
{
  char data[10];
  uint16_t oldMarkColor = GxEPD_BLACK;

  display.setRotation(1);
  if (val < 4.0 || val > 14.0)
  {
    display.fillScreen(GxEPD_RED);
    display.setTextColor(GxEPD_WHITE);
    oldMarkColor = GxEPD_WHITE;
  }
  else
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    oldMarkColor = GxEPD_BLACK;
  }

  // Display value
  //display.fillRect(0, ((DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN) / 2 + DISPLAY_VALUE_FONT_HEIGHT) - 1, 200, 2, GxEPD_BLACK);
  display.setCursor(0, ((DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN) / 2 + DISPLAY_VALUE_FONT_HEIGHT));
  display.setFont(&DISPLAY_VALUE_FONT);
  if (val == 0)
  {
    display.println(F("NA"));
  }
  else
  {
    sprintf(data, " %.1f", val);
    display.println(data);
    if (isOld)
    {
      display.fillRect(0, ((DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN) / 2) + DISPLAY_VALUE_FONT_HEIGHT / 2 - 5, 200, 10, oldMarkColor);
    }
  }

  if (hour != 0xff)
  {
    // Display time
    display.setCursor(DISPLAY_WIDTH - DISPLAY_MARGIN - (DISPLAY_TIME_FONT_WIDTH * /*number of char*/ 5), DISPLAY_MARGIN + DISPLAY_TIME_FONT_HEIGHT);
    display.setFont(&DISPLAY_TIME_FONT);
    sprintf(data, "%2d:%02d", hour, minute);
    display.println(data);
  }

  {
    // Display is source_bt
    //display.setCursor(DISPLAY_WIDTH - DISPLAY_MARGIN - (DISPLAY_TIME_FONT_WIDTH * /*number of char*/ 5), DISPLAY_MARGIN + DISPLAY_TIME_FONT_HEIGHT);
    display.setCursor(DISPLAY_MARGIN, DISPLAY_MARGIN + DISPLAY_TIME_FONT_HEIGHT);
    display.setFont(&DISPLAY_TIME_FONT);
    display.println(source_bt ? "B" : "W");
  }

  display.update();
}

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

#if DEBUG
    if (rxValue.length() > 0)
    {
      Serial.println(F("*********"));
      time_t now;
      time(&now);
      Serial.println(now);
      printHexValue(rxValue);
    }
#endif

    if (rxValue.length() > 3 && rxValue[0] == 0xAB && rxValue[1] == rxValue.length() && rxValue[rxValue.length() - 1] == calculateCRC(reinterpret_cast<const uint8_t *>(rxValue.data()), rxValue.length() - 1))
    {
#if DEBUG
      Serial.print(F("OpCode: 0x"));
      Serial.println(rxValue[2], HEX);
#endif
      if (rxValue[2] == 0x00 && rxValue[1] == (1 + 3))
      { //TxPing
#if DEBUG
        Serial.println(F("Ping"));
#endif
        uint8_t rxPing[] = {
            /*RxStartByte*/ 0x5A,
            /*Length*/ 0x14,
            /*OpCode*/ 0x00,
            /*ff1*/ 0x00,
            /*othree*/ 0x00,
            /*zero2*/ 0x00,
            /*zero3*/ 0x00,
            /*3xmodel*/ 0x45, 0x53, 0x50,
            /*vers1*/ 0x00,
            /*2x hwVersion*/ 0x33, 0x32,
            /*2x swVersion*/ 0x30, 0x31,
            /*4x manufacturer*/ 0x45, 0x53, 0x50, 0x20,
            /*CRC*/ 0x00};
        rxPing[sizeof(rxPing) - 1] = calculateCRC(rxPing, sizeof(rxPing) - 1);
        printHexValue(rxPing, sizeof(rxPing));

        pTxCharacteristic->setValue(rxPing, sizeof(rxPing));
        pTxCharacteristic->notify();
      }
      else if (rxValue[2] == 0x02 && rxValue[1] == (5 + 3))
      { //TxSetLocaleFeature
#if DEBUG
        Serial.println(F("SetLocaleFeature"));
#endif
      }
      else if (rxValue[2] == 0x04 && rxValue[1] == (8 + 3))
      { //TxSetTime
#if DEBUG
        Serial.println(F("SetTime"));
#endif
        if (rxValue[3] == 0x01 /*WRITE*/)
        {
          //rxValue[4]  Year ignore
          uint8_t decimal1 = 0x00;
          uint8_t decimal2 = 0x00;
          if (rxValue[5] == 0x00 && rxValue[6] == 0x00)
          {
            //no value
          }
          else if (rxValue[5] == 0x00)
          {
            //value is rxValue[6] only
            decimal1 = rxValue[6];
          }
          else
          {
            //value is rxValue[5].rxValue[6]
            decimal1 = rxValue[5];
            decimal2 = rxValue[6];
          }
          uint8_t hour = rxValue[7];
          uint8_t minute = rxValue[8];
          uint8_t second = rxValue[9];
          float val = decimal1 + float(decimal2) / 10;

          if (last_val != val || last_hour != hour || last_minute != minute || last_second != second)
          {
            last_time = time(NULL);
            last_time_too_old = last_time + VALUE_OLD_TIME;
            current_value_isOld = false;
            last_val = val;
            last_hour = hour;
            last_minute = minute;
            last_second = second;
            bool source_bt = true;
            displayValues(val, source_bt, hour, minute, second, false);

            if (decimal1 != 0x00)
            {
              // disable WiFi and BT
              WiFi.mode(WIFI_OFF);
              esp_bt_controller_disable();
              btStop();
              esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
#if DEBUG
              Serial.flush();
              delay(250);
#endif
              esp_deep_sleep_start();
            }
          }
        }
      }
      else if (rxValue[2] == 0x07 && rxValue[1] == (4 + 3))
      { //TxSetScreens
#if DEBUG
        Serial.println(F("SetScreens"));
#endif
      }
      else if (rxValue[2] == 0x08 && rxValue[1] == (4 + 3))
      { //TxSetFeatures
#if DEBUG
        Serial.println(F("SetFeatures"));
#endif
      }
    }

#if DEBUG
    Serial.println(F("*********"));
#endif
  }
};

void setup()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

#if DEBUG
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  display.init(115200); // enable diagnostic output on Serial

  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
  {
    displayValues(15.0, /*Source_bt:*/ false, 20, 99, 00, true);
    readWiFiandDisplay();
  }
#else
  display.init();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
  {
    displayValues(0x00, 0x00, 0xFF, 0x00, 0x00, false);
    readWiFiandDisplay();
  }
#endif

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  esp_bt_controller_enable(ESP_BT_MODE_BLE);

  // Create the BLE Device
  BLEDevice::init("Lefun");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();

#if DEBUG
  Serial.println("Waiting a client connection to notify...");
#endif
}

//bool wifiConnect();

void loop()
{
  if (deviceConnected)
  {
    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
#if DEBUG
    Serial.println("start advertising");
#endif
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  if (!current_value_isOld && last_time_too_old != 0x00 && last_time_too_old < time(NULL))
  {
    readWiFiandDisplay();
    current_value_isOld = true;
  }
}

void readWiFiandDisplay()
{
  int retry = 3;
  while (--retry > 0)
  {
    delay(2000);
    if (!wifiConnect())
    {
      continue;
    }

    if (!setClock())
    {
      continue;
    }

    int sgv = 0x00;
    time_t sgv_date = 0x00;
    if (!getValue(sgv, sgv_date))
    {
      continue;
    }

    float val = (float)sgv / 18.0;
    struct tm *timeinfo;
    timeinfo = localtime(&sgv_date);

    // UTC to local time
    sgv_date += (utcOffset + dstOffset(timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year + 1900, timeinfo->tm_hour)) * SECS_PER_HOUR;
    timeinfo = localtime(&sgv_date);
    bool source_bt = false;
#if DEBUG
    Serial.print("val: ");
    Serial.println(val);
    Serial.print("sgv_date: ");
    Serial.println(sgv_date);
    Serial.print("timeinfo: ");
    Serial.println(timeinfo);
    Serial.print("tm_hour: ");
    Serial.println(timeinfo->tm_hour);
    Serial.print("tm_min: ");
    Serial.println(timeinfo->tm_min);
    Serial.print("tm_sec : ");
    Serial.println(timeinfo->tm_sec);
#endif
    displayValues(val, source_bt, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, false);

    return;

  }

  /*last_time = time(NULL);
    last_time_too_old = last_time + VALUE_OLD_TIME;
    current_value_isOld = false;
    displayValues(val, last_dec2, last_hour, last_minute, last_second, current_value_isOld);
    
    //Display update

    WiFi.mode(WIFI_OFF);
    esp_bt_controller_disable();
    btStop();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
#if DEBUG
    Serial.flush();
    delay(250);
#endif
    esp_deep_sleep_start();
  */
}
