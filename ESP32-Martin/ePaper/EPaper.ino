// https://github.com/ZinggJM/GxEPD
// https://github.com/ZinggJM/GxEPD/blob/master/examples/GxEPD_Example/GxEPD_Example.ino
#include <GxEPD.h>
#include <GxGDEW027C44/GxGDEW027C44.h>  // 2.7" b/w/r 264x176

const int16_t arrow_pos_x = 145;
const int16_t arrow_pos_y = 100;
const int16_t value_pos_x = 20;
const int16_t value_pos_y = 100;
const int utcOffset = 1;        // Central European Time

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
#define TIME_TO_SLEEP  60       /* Time ESP32 will go to sleep (in seconds) */

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

  showValue(0, 0, "Flat");
  delay(5000);
  showValue(0, 0, "FortyFiveUp");
  delay(5000);
  showValue(0, 0, "FortyFiveDown");
  delay(5000);
  showValue(0, 0, "SingleUp");
  delay(5000);
  showValue(0, 0, "SingleDown");
  delay(5000);
  showValue(0, 0, "DoubleUp");
  delay(5000);
  showValue(0, 0, "DoubleDown");

  int retry = 3;
  while (--retry > 0) {
    delay(2000);
    /*if(!wifiConnect()) {
      continue;
    }

    if(!setClock()) {
      continue;
    }

    if(!getValue()) {
      continue;
    }*/
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush();
  delay(250);
  esp_deep_sleep_start();
  // Next line is never executed
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

bool wifiConnect() {
  if(WiFi.status() == WL_CONNECTED) {
    return true;
  }
#ifdef RE_INIT_NEEDED
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
#endif

  if (!WiFi.getAutoConnect() || ( WiFi.getMode() != WIFI_STA) || ((WiFi.SSID() != ssid) && String(ssid) != "........"))
  {
    Serial.println();
    Serial.print("WiFi.getAutoConnect()=");
    Serial.println(WiFi.getAutoConnect());
    Serial.print("WiFi.SSID()=");
    Serial.println(WiFi.SSID());
    WiFi.mode(WIFI_STA); // switch off AP
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  }

  int ConnectTimeout = 30; // 15 seconds
  while (WiFi.status() != WL_CONNECTED) {
    if (--ConnectTimeout <= 0)
    {
      Serial.println();
      Serial.println("WiFi connect timeout");
      return false;
    }
    delay(500);
    Serial.print(".");
    Serial.print(WiFi.status());
  }
  Serial.println();
  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(2000);

  return true;
}

const int validMinTime = (8 * 3600 * 2);

bool setClock() {
  time_t nowSecs = time(nullptr);
  if(nowSecs < validMinTime) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
    Serial.print("Waiting for NTP time sync: ");
    nowSecs = time(nullptr);
    int ConnectTimeout = 30; // 15 seconds
    while (nowSecs < validMinTime) {
      if (--ConnectTimeout <= 0)
      {
        Serial.println();
        Serial.println("Waiting for NTP time timeout");
        return false;
      }
      delay(500);
      Serial.print(".");
      yield();
      nowSecs = time(nullptr);
    }
    Serial.println();
  }

  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print("Current time: ");
  Serial.println(asctime(&timeinfo));
  return true;
}

RTC_DATA_ATTR double lastValueDate = 0;

bool getValue() {
  bool ret = false;
  HTTPClient http;
  http.begin(api_url, root_ca); //Specify the URL and certificate
  http.addHeader("accept", "application/json");
  http.addHeader("api_secret", api_secret);
  int httpCode = http.GET();    //Make the request

  if (httpCode < 0) { //Check for the returning code
    Serial.print("Error on HTTP request ");
    Serial.println(httpCode);
  } else {
    String payload = http.getString();
    if (httpCode != 200) {
      Serial.print("Error on HTTP request ");
      Serial.println(httpCode);
      Serial.println(payload);
    } else {
      // Allocate the JSON document
      // Use https://arduinojson.org/v6/assistant/ to compute the capacity.
      const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 600;
      DynamicJsonDocument doc(capacity);
      auto error = deserializeJson(doc, payload); //Parse message
      if (error) { //Check for errors in parsing
        Serial.println("Parsing HTTP response failed");
        Serial.println(payload);
      } else {
        JsonObject root_0 = doc[0];
        const char* root_0_type = root_0["type"];
        if(strcmp(root_0_type, "sgv") != 0) {
          Serial.print("JSON response not of type 'sgv' ");
          Serial.println(root_0_type);
        } else {
          double date = root_0["date"].as<double>();
          date = date / 1000;
          if(lastValueDate == date) {
            Serial.print("Date value unchanged ");
            Serial.println(date);
          } else {
            lastValueDate = date;
            Serial.print("Date value changed ");
            Serial.println(date);

            //const char* dateString = root_0["dateString"];
            const char* direction = root_0["direction"];
            int sgv = root_0["sgv"];
            Serial.print("SGV value ");
            Serial.println(sgv);
            showValue(date, sgv, direction);
          }
          ret = true;
        }
      }
    }
  }
  http.end(); //Free the resources
  return ret;
}

void loop() {
  // Never executed 
}

//float convertSGV(int sgv) {
//  float val = (float)sgv / 18.0;
//  val = roundf(val / 10.0) * 10.0;
//  return val;
//}
void convertSGV(int sgv, char* buffer, int buffersize) {
  float val = (float)sgv / 18.0;
  //val = roundf(val / 10.0) * 10.0;
  snprintf(buffer, buffersize, "%.1f", val);
}

#define SECS_PER_HOUR (3600UL)

void convertDate(double date, char* buffer, int buffersize) {
  int totalSeconds = (int)date;
  time_t rawtime = (time_t)totalSeconds;
  struct tm * timeinfo;
  timeinfo = localtime (&rawtime);

  // UTC to local time
  rawtime += (utcOffset + dstOffset (timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year + 1900, timeinfo->tm_hour)) * SECS_PER_HOUR;
  timeinfo = localtime (&rawtime);
  
  //strftime(buffer, buffersize, "%Y-%m-%d %H:%M:%S", timeinfo);
  strftime(buffer, buffersize, "%d.%m %H:%M", timeinfo);
}

/* This function returns the DST offset for the current UTC time.
 * This is valid for the EU, for other places see
 * http://www.webexhibits.org/daylightsaving/i.html
 * 
 * Results have been checked for 2012-2030 (but should work since
 * 1996 to 2099) against the following references:
 * - http://www.uniquevisitor.it/magazine/ora-legale-italia.php
 * - http://www.calendario-365.it/ora-legale-orario-invernale.html
 */
byte dstOffset (byte d, byte m, unsigned int y, byte h) {
  // Day in March that DST starts on, at 1 am
  byte dstOn = (31 - (5 * y / 4 + 4) % 7);

  // Day in October that DST ends  on, at 2 am
  byte dstOff = (31 - (5 * y / 4 + 1) % 7);

  if ((m > 3 && m < 10) ||
      (m == 3 && (d > dstOn || (d == dstOn && h >= 1))) ||
      (m == 10 && (d < dstOff || (d == dstOff && h <= 1))))
    return 1;
  else
    return 0;
}

void showValue(const double date, const int sgv, const char* direction){
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold18pt7b);
  display.setCursor(0, 0);
  display.println();

  char dateString[12];
  convertDate(date, dateString, sizeof(dateString));
  display.println(dateString);
  //display.println();
  
  display.setFont(&FreeMonoBold24pt7b);
  char val[6];
  convertSGV(sgv, val, sizeof(val));
  Serial.print("mmol value ");
  Serial.println(val);
  if(sgv < 4.0) {
    display.setTextColor(GxEPD_RED);
  }
  display.setCursor(value_pos_x, value_pos_y);
  display.println();
  display.println(val);
  display.setTextColor(GxEPD_BLACK);

  drawDirection(direction);
  
  display.update();
}

void drawDirection(const char* direction){
  //DoubleDown, DoubleUp, SingleDown, SingleUp, FortyFiveDown, FortyFiveUp, Flat
  if(strcmp(direction, "Flat") == 0) {
    const uint16_t color = GxEPD_BLACK;
    drawLineY5(arrow_pos_x + 00, arrow_pos_y + 20, arrow_pos_x + 60, arrow_pos_y + 20, color);
    drawLineX7(arrow_pos_x + 60, arrow_pos_y + 20, arrow_pos_x + 40, arrow_pos_y + 00, color);
    drawLineX7(arrow_pos_x + 60, arrow_pos_y + 20, arrow_pos_x + 40, arrow_pos_y + 40, color);
  } else if (strcmp(direction, "FortyFiveUp") == 0) {
    const uint16_t color = GxEPD_BLACK;
    drawLineX5(arrow_pos_x + 20, arrow_pos_y + 40, arrow_pos_x + 60, arrow_pos_y + 00, color);
    drawLineX5(arrow_pos_x + 60, arrow_pos_y + 00, arrow_pos_x + 60, arrow_pos_y + 30, color);
    drawLineY5(arrow_pos_x + 60, arrow_pos_y + 00, arrow_pos_x + 30, arrow_pos_y + 00, color);
  } else if (strcmp(direction, "FortyFiveDown") == 0) {
    const uint16_t color = GxEPD_BLACK;
    drawLineX5(arrow_pos_x + 20, arrow_pos_y + 00, arrow_pos_x + 60, arrow_pos_y + 40, color);
    drawLineX5(arrow_pos_x + 60, arrow_pos_y + 40, arrow_pos_x + 60, arrow_pos_y + 10, color);
    drawLineY5(arrow_pos_x + 60, arrow_pos_y + 40, arrow_pos_x + 30, arrow_pos_y + 40, color);
  } else if (strcmp(direction, "SingleUp") == 0) {
    const uint16_t color = GxEPD_RED;
    drawLineX5(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 30, arrow_pos_y + 00, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 10, arrow_pos_y + 20, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 50, arrow_pos_y + 20, color);
  } else if (strcmp(direction, "SingleDown") == 0) {
    const uint16_t color = GxEPD_RED;
    drawLineX5(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 30, arrow_pos_y + 60, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 10, arrow_pos_y + 40, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 50, arrow_pos_y + 40, color);
  } else if (strcmp(direction, "DoubleUp") == 0) {
    const uint16_t color = GxEPD_RED;
    drawLineX5(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 30, arrow_pos_y + 00, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 10, arrow_pos_y + 20, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 50, arrow_pos_y + 20, color);
    
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 20, arrow_pos_x + 10, arrow_pos_y + 40, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 20, arrow_pos_x + 50, arrow_pos_y + 40, color);
  } else if (strcmp(direction, "DoubleDown") == 0) {
    const uint16_t color = GxEPD_RED;
    drawLineX5(arrow_pos_x + 30, arrow_pos_y + 00, arrow_pos_x + 30, arrow_pos_y + 60, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 10, arrow_pos_y + 40, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 60, arrow_pos_x + 50, arrow_pos_y + 40, color);

    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 40, arrow_pos_x + 10, arrow_pos_y + 20, color);
    drawLineX7(arrow_pos_x + 30, arrow_pos_y + 40, arrow_pos_x + 50, arrow_pos_y + 20, color);
  }
}

void drawLineX5(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  display.drawLine(x0 - 2, y0 - 0, x1 - 2, y1 - 0, color);
  display.drawLine(x0 - 1, y0 - 0, x1 - 1, y1 - 0, color);
  display.drawLine(x0 + 0, y0 + 0, x1 + 0, y1 + 0, color);
  display.drawLine(x0 + 1, y0 + 0, x1 + 1, y1 + 0, color);
  display.drawLine(x0 + 2, y0 + 0, x1 + 2, y1 + 0, color);
}

void drawLineX7(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  display.drawLine(x0 - 3, y0 - 0, x1 - 3, y1 - 0, color);
  display.drawLine(x0 - 2, y0 - 0, x1 - 2, y1 - 0, color);
  display.drawLine(x0 - 1, y0 - 0, x1 - 1, y1 - 0, color);
  display.drawLine(x0 + 0, y0 + 0, x1 + 0, y1 + 0, color);
  display.drawLine(x0 + 1, y0 + 0, x1 + 1, y1 + 0, color);
  display.drawLine(x0 + 2, y0 + 0, x1 + 2, y1 + 0, color);
  display.drawLine(x0 + 3, y0 + 0, x1 + 3, y1 + 0, color);
}

void drawLineY5(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  display.drawLine(x0 - 0, y0 - 2, x1 - 0, y1 - 2, color);
  display.drawLine(x0 - 0, y0 - 1, x1 - 0, y1 - 1, color);
  display.drawLine(x0 + 0, y0 + 0, x1 + 0, y1 + 0, color);
  display.drawLine(x0 + 0, y0 + 1, x1 + 0, y1 + 1, color);
  display.drawLine(x0 + 0, y0 + 2, x1 + 0, y1 + 2, color);
}

void drawLineY7(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  display.drawLine(x0 - 0, y0 - 3, x1 - 0, y1 - 3, color);
  display.drawLine(x0 - 0, y0 - 2, x1 - 0, y1 - 2, color);
  display.drawLine(x0 - 0, y0 - 1, x1 - 0, y1 - 1, color);
  display.drawLine(x0 + 0, y0 + 0, x1 + 0, y1 + 0, color);
  display.drawLine(x0 + 0, y0 + 1, x1 + 0, y1 + 1, color);
  display.drawLine(x0 + 0, y0 + 2, x1 + 0, y1 + 2, color);
  display.drawLine(x0 + 0, y0 + 3, x1 + 0, y1 + 3, color);
}
