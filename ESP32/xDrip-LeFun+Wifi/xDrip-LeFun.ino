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
// select the display class to use, only one
//#include <GxGDEP015OC1/GxGDEP015OC1.h>    // 1.54" b/w
#include <GxGDEW0154Z04/GxGDEW0154Z04.h> // 1.54" b/w/r 200x200
//#include <GxGDEW0154Z17/GxGDEW0154Z17.h>  // 1.54" b/w/r 152x152
//#include <GxGDEW0213I5F/GxGDEW0213I5F.h>  // 2.13" b/w 104x212 flexible
//#include <GxGDE0213B1/GxGDE0213B1.h>      // 2.13" b/w
//#include <GxGDEH0213B72/GxGDEH0213B72.h>  // 2.13" b/w new panel
//#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w newer panel
//#include <GxGDEW0213Z16/GxGDEW0213Z16.h>  // 2.13" b/w/r
//#include <GxGDEH029A1/GxGDEH029A1.h>      // 2.9" b/w
//#include <GxGDEW029T5/GxGDEW029T5.h>      // 2.9" b/w IL0373
//#include <GxGDEW029Z10/GxGDEW029Z10.h>    // 2.9" b/w/r
//#include <GxGDEW026T0/GxGDEW026T0.h>      // 2.6" b/w
//#include <GxGDEW027C44/GxGDEW027C44.h>    // 2.7" b/w/r
//#include <GxGDEW027W3/GxGDEW027W3.h>      // 2.7" b/w
//#include <GxGDEW0371W7/GxGDEW0371W7.h>    // 3.7" b/w
//#include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
//#include <GxGDEW042Z15/GxGDEW042Z15.h>    // 4.2" b/w/r
//#include <GxGDEW0583T7/GxGDEW0583T7.h>    // 5.83" b/w
//#include <GxGDEW075T8/GxGDEW075T8.h>      // 7.5" b/w
//#include <GxGDEW075T7/GxGDEW075T7.h>      // 7.5" b/w 800x480
//#include <GxGDEW075Z09/GxGDEW075Z09.h>    // 7.5" b/w/r
//#include <GxGDEW075Z08/GxGDEW075Z08.h>    // 7.5" b/w/r 800x480

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

// SPIFFS for Storage
//#include "SPIFFS.h"
//#include "FS.h"

// NVS_Flash for Storage
#include <nvs_flash.h>
#include <nvs.h>

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

#define BUTTON_PIN_BITMASK_GPIO_32 0x100000000 // 2^32 in hex
#define BUTTON_PIN_BITMASK_GPIO_33 0x200000000 // 2^33 in hex
#define BUTTON_PIN_BITMASK_GPIO_34 0x400000000 // 2^34 in hex
#define BUTTON_PIN_BITMASK_GPIO_35 0x800000000 // 2^35 in hex
#define BUTTON_PIN_BITMASK BUTTON_PIN_BITMASK_GPIO_32 | BUTTON_PIN_BITMASK_GPIO_33 | BUTTON_PIN_BITMASK_GPIO_35
//#define BUTTON_PIN_BITMASK BUTTON_PIN_BITMASK_GPIO_33
#define BTN_1 35
#define BTN_2 32
#define BTN_3 33
#define BTN_UP BTN_1
#define BTN_ENTER BTN_2
#define BTN_DOWN BTN_3
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define ROTATION 1
#define TZ utcOffset

#if DEBUG

#define TIME_TO_SLEEP 270 /* Time ESP32 will go to sleep (in seconds) */
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
RTC_DATA_ATTR int8_t utcOffset = 1; // set default Timezone to Central European Time

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

void shutdown()
{
#if DEBUG
  Serial.println("Shutting down");
  //Serial.println(BUTTON_PIN_BITMASK);

  Serial.flush();
  delay(250);
#endif
  WiFi.mode(WIFI_OFF);
  esp_bt_controller_disable();
  btStop();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
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

RTC_DATA_ATTR bool utcOffsetSave = false;
RTC_DATA_ATTR bool utcOffsetChanged = false;
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

  display.setRotation(ROTATION);
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
            return;
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

            if (val != 0.0)
            {

              shutdown();
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

void IRAM_ATTR UP_PRESSED()
{
  utcOffset = utcOffset + 1;
  utcOffsetChanged = true;
  Serial.print("utcOffset = ");
  Serial.println(utcOffset);
}

void IRAM_ATTR DOWN_PRESSED()
{
  utcOffset = utcOffset - 1;
  utcOffsetChanged = true;
  Serial.print("utcOffset = ");
  Serial.println(utcOffset);
}

void IRAM_ATTR ENTER_PRESSED()
{
  utcOffsetSave = true;
}

void setup()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  attachInterrupt(BTN_UP, UP_PRESSED, HIGH);
  attachInterrupt(BTN_ENTER, ENTER_PRESSED, HIGH);
  attachInterrupt(BTN_DOWN, DOWN_PRESSED, HIGH);

#if DEBUG
  Serial.begin(115200);
  display.init(115200);  // enable diagnostic output on Serial
  print_wakeup_reason(); //Print the wakeup reason for ESP32
#else
  display.init();
#endif

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) //Wakeup via Button Pressed
  {
#if DEBUG
    Serial.println("Wakeup by button ");
    print_GPIO_wake_up();
//    displayValues(0.0, /*Source_bt:*/ false, 99, 99, 99, true);
#endif
    uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
    switch ((int)(log(GPIO_reason) / log(2)))
    {
    case BTN_UP:
      utcOffset = utcOffset + 1;
#if DEBUG
      Serial.println("Button up pressed");
      Serial.println(utcOffset);
#endif
      displayTimeZone();
      break;
    case BTN_ENTER:
#if DEBUG
      Serial.println("Button Enter pressed");
#endif
      displayValues(last_val, last_source_bt, last_hour, last_minute, last_second, current_value_isOld);
      StartBLEAdvertising();
      break;
    case BTN_DOWN:
      utcOffset = utcOffset - 1;
#if DEBUG
      Serial.println("Button down pressed");
      Serial.println(utcOffset);
#endif
      displayTimeZone();
      break;
    default:
#if DEBUG
      Serial.println("Other Button pressed");
#endif
      break;
    }
  }
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
  {
#if DEBUG
    //displayValues(0.0, /*Source_bt:*/ false, 99, 99, 99, true);
#endif
    ReadFromSPIFFS();
    displayTimeZone();
    readWiFiandDisplay();
  }
  StartBLEAdvertising();
}

void StartBLEAdvertising()
{
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

  if (utcOffsetChanged == true)
  {

    if (utcOffset >= 13)
    {
      utcOffset = -12;
    }
    if (utcOffset <= -13)
    {
      utcOffset = 12;
    }
    utcOffsetChanged = false;
    displayTimeZone();
  }

  if (utcOffsetSave == true)
  {
    WriteToSPIFFS();
    utcOffsetSave = false;
#if DEBUG
    Serial.println("utcOffset saved");
#endif
    displayValues(last_val, last_source_bt, last_hour, last_minute, last_second, current_value_isOld);
  }

  if (!current_value_isOld && last_time_too_old != 0x00 && last_time_too_old < time(NULL))
  {
    if (!readWiFiandDisplay())
    {
      current_value_isOld = true;
      displayValues(last_val, last_source_bt, last_hour, last_minute, last_second, current_value_isOld);
    }
    shutdown();
  }
}

bool readWiFiandDisplay()
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
    last_time = time(NULL);
    last_time_too_old = last_time + VALUE_OLD_TIME;
    current_value_isOld = false;
    last_val = val;
    last_hour = timeinfo->tm_hour;
    last_minute = timeinfo->tm_min;
    last_second = timeinfo->tm_sec;
    bool last_source_bt = false;
    displayValues(val, source_bt, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, false);
    return true;
  }

  return false;

  /*last_time = time(NULL);
    last_time_too_old = last_time + VALUE_OLD_TIME;
    current_value_isOld = false;
    displayValues(val, last_dec2, last_hour, last_minute, last_second, current_value_isOld);
    
    //Display update
*/
}

void print_GPIO_wake_up()
{
  uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  Serial.print("GPIO that triggered the wake up: GPIO ");
  Serial.println((log(GPIO_reason)) / log(2), 0);
}

void displayTimeZone()
{
  display.setRotation(ROTATION);

  display.fillScreen(GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);

  display.setFont(&DISPLAY_TIME_FONT);
  display.setCursor(DISPLAY_MARGIN, DISPLAY_MARGIN + DISPLAY_TIME_FONT_HEIGHT);
  display.println("TZ:");
  display.println();
  display.setFont(&DISPLAY_VALUE_FONT);
  display.print("  ");
  display.println(utcOffset);
  display.update();
}

void WriteToSPIFFS()
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  nvs_handle my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
  {
    Serial.printf("FATAL ERROR: Unable to open NVS\n");
  }
  err = nvs_set_i8(my_handle, "offset", utcOffset);
  if (err != ESP_OK)
  {
    Serial.printf("FATAL ERROR: Unable to open NVS\n");
  }
  err = nvs_commit(my_handle);
#if DEBUG
  nvs_stats_t nvs_stats;
  nvs_get_stats(NULL, &nvs_stats);
  Serial.printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
                nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
#endif
  nvs_close(my_handle);
}

void ReadFromSPIFFS()
{
  //delay(100);
  esp_err_t err = nvs_flash_init();
  //Serial.print(esp_err_to_name(err));
  if (err == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  nvs_handle my_handle;
  err = nvs_open("storage", NVS_READONLY, &my_handle);
  if (err != ESP_OK)
  {
    Serial.printf("FATAL ERROR: Unable to open NVS\n");
    Serial.print(esp_err_to_name(err));
  }

  err = nvs_get_i8(my_handle, "offset", &utcOffset);
  if (err != ESP_OK)
  {
    Serial.printf("FATAL ERROR: Unable to open NVS\n");
  }
  nvs_close(my_handle);
  Serial.print("utcOffset from SPIFFS: ");
  Serial.println(utcOffset);
}
