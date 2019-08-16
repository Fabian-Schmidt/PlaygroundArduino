// https://github.com/ZinggJM/GxEPD
// https://github.com/ZinggJM/GxEPD/blob/master/examples/GxEPD_Example/GxEPD_Example.ino
#include <GxEPD.h>
#include <GxGDEW027C44/GxGDEW027C44.h>  // 2.7" b/w/r 264x176

//#include GxEPD_BitmapExamples

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include "secret.h"
#include "ArduinoJson.h"

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */


// for SPI pin definitions see e.g.:
// C:\Users\xxx\Documents\Arduino\hardware\espressif\esp32\variants\lolin32\pins_arduino.h

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

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

void setup() {
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor
  
  Serial.println();
  Serial.println("setup");

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  display.init(115200); // enable diagnostic output on Serial
  delay(2000);
  wifiConnect();

  Serial.println("setup done");
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void wifiConnect() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to '");
    Serial.print(ssid);
    Serial.println("'");
    WiFi.begin(ssid, password);
    int count = 30;
    while (count > 0 && WiFi.status() != WL_CONNECTED) {
      count--;
      delay(500);
      Serial.print(F("."));
    }
    Serial.println("");
    if(count == 0){
      delay(1000);
    }
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(2000);

  setClock();
}

// Not sure if WiFiClientSecure checks the validity date of the certificate. 
// Setting clock just to be sure...
void setClock() {
  time_t nowSecs = time(nullptr);
  if(nowSecs < 8 * 3600 * 2) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
    Serial.print(F("Waiting for NTP time sync: "));
    nowSecs = time(nullptr);
    int count = 30;
    while (count > 0 && nowSecs < 8 * 3600 * 2) {
      count--;
      delay(500);
      Serial.print(F("."));
      yield();
      nowSecs = time(nullptr);
    }
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

RTC_DATA_ATTR double lastValueDate = 0;

void loop() {
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;
    http.begin(api_url, root_ca); //Specify the URL and certificate
    http.addHeader("accept", "application/json");
    http.addHeader("api_secret", api_secret);
    int httpCode = http.GET();    //Make the request

    if (httpCode > 0) { //Check for the returning code
      String payload = http.getString();
      if (httpCode == 200) {
        // Allocate the JSON document
        // Use https://arduinojson.org/v6/assistant/ to compute the capacity.
        const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 600;
        DynamicJsonDocument doc(capacity);
        auto error = deserializeJson(doc, payload); //Parse message
        if (error) { //Check for errors in parsing
          Serial.println("Parsing failed");
          Serial.println(payload);
        } else {
          JsonObject root_0 = doc[0];
          const char* root_0_type = root_0["type"];
          if(strcmp(root_0_type, "sgv") == 0) {
            double date = root_0["date"].as<double>();
            date = date / 1000;
            Serial.println(date);
            if(lastValueDate != date) {
              lastValueDate = date;

              int totalSeconds = (int)date;
              time_t rawtime = (time_t)totalSeconds;
              struct tm * timeinfo;
              timeinfo = localtime (&rawtime);
              const char* dateString = asctime(timeinfo);
              
              //const char* dateString = root_0["dateString"];
              const char* direction = root_0["direction"];
              //DoubleDown, DoubleUp, SingleDown, SingleUp, FortyFiveDown, FortyFiveUp, Flat
              int sgv = root_0["sgv"];
              Serial.println(sgv);
              float val = convertSGV(sgv);
              Serial.println(val);
              showValue(dateString, val, direction, &FreeMonoBold18pt7b);
            } else {
              Serial.println("Value unchanged");
            }
          }
        }
      } else {
        Serial.println(httpCode);
        Serial.println(payload);
      }
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      Serial.flush();
      delay(100);
      esp_deep_sleep_start();
      delay(60000);
    } else {
      Serial.println("Error on HTTP request");
      Serial.println(httpCode);
      delay(1000);
    }

    http.end(); //Free the resources
  } else {
    delay(1000);
    wifiConnect();
  }
  
  //showBitmapExample();
  //delay(2000);
  //Serial.println("Without __AVR");
  //drawCornerTest();
  //showFont("FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
  //showFont("FreeMonoBold12pt7b", &FreeMonoBold12pt7b);
  //showFont("FreeMonoBold18pt7b", &FreeMonoBold18pt7b);
  //showFont("FreeMonoBold24pt7b", &FreeMonoBold24pt7b);
  //delay(60000);
}

//void showBitmapExample()
//{
//  display.drawExamplePicture(BitmapExample1, BitmapExample2, sizeof(BitmapExample1), sizeof(BitmapExample2));
//  delay(5000);
//}

float convertSGV(int sgv) {
  float val = (float)sgv / 18.0;
  val = roundf(val * 10) / 10;
  return val;
}

//String convertDate(long date) {
//  //Convert to Unix Time Stamp
//  date = date / 1000;
//
//  byte minute = epoch%60; epoch /= 60;
//  byte hour   = epoch%24; epoch /= 24;
//
//  return "";
//}

void showValue(const char name[],const float val, const char direction[], const GFXfont* f){
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(name);
  display.println(val);
  display.update();
}

void showFont(const char name[], const GFXfont* f)
{
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(name);
  display.println(" !\"#$%&'()*+,-./");
  display.println("0123456789:;<=>?");
  display.println("@ABCDEFGHIJKLMNO");
  display.println("PQRSTUVWXYZ[\\]^_");
#if defined(HAS_RED_COLOR)
  display.setTextColor(GxEPD_RED);
#endif
  display.println("`abcdefghijklmno");
  display.println("pqrstuvwxyz{|}~ ");
  display.update();
  delay(5000);
}

void drawCornerTest()
{
  display.drawCornerTest();
  delay(5000);
  uint8_t rotation = display.getRotation();
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, 8, 8, GxEPD_BLACK);
    display.fillRect(display.width() - 18, 0, 16, 16, GxEPD_BLACK);
    display.fillRect(display.width() - 25, display.height() - 25, 24, 24, GxEPD_BLACK);
    display.fillRect(0, display.height() - 33, 32, 32, GxEPD_BLACK);
    display.update();
    delay(5000);
  }
  display.setRotation(rotation); // restore
}
