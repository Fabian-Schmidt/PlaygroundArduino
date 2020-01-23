#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "secret.h"
#define ARDUINOJSON_USE_DOUBLE 1 
#include "ArduinoJson.h"

bool wifiConnect()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }
#ifdef RE_INIT_NEEDED
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
#endif

  if (!WiFi.getAutoConnect() || (WiFi.getMode() != WIFI_STA) || ((WiFi.SSID() != ssid) && String(ssid) != "........"))
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

  int ConnectTimeout = 30; // 30 seconds
  while (WiFi.status() != WL_CONNECTED)
  {
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

bool setClock()
{
  time_t nowSecs = time(nullptr);
  if (nowSecs < validMinTime)
  {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print("Waiting for NTP time sync: ");
    nowSecs = time(nullptr);
    int ConnectTimeout = 30; // 15 seconds
    while (nowSecs < validMinTime)
    {
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

bool getValue(int &sgv, time_t &sgv_date)
{
  bool ret = false;

  WiFiClientSecure *client = new WiFiClientSecure;
  if (client)
  {
    client->setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
      HTTPClient http;

      Serial.print("[HTTPS] begin...\n");
      if (http.begin(*client, api_url))
      {
        Serial.print("[HTTPS] GET...\n");

        http.addHeader("accept", "application/json");
        int httpCode = http.GET(); //Make the request

        if (httpCode < 0)
        { //Check for the returning code
          Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
          //Serial.print("Error on HTTP request ");
          //Serial.println(httpCode);
        }
        else
        {
          String payload = http.getString();
          Serial.println(payload);
          if (httpCode != HTTP_CODE_OK)
          {
            Serial.print("Error on HTTP request ");
            Serial.println(httpCode);
            Serial.println(payload);
          }
          else
          {
            // Allocate the JSON document
            // Use https://arduinojson.org/v6/assistant/ to compute the capacity.
            const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(13) + 600;
            DynamicJsonDocument doc(capacity);
            auto error = deserializeJson(doc, payload); //Parse message
            if (error)
            { //Check for errors in parsing
              Serial.println("Parsing HTTP response failed");
              Serial.println(payload);
            }
            else
            {
              JsonObject root_0 = doc[0];
              const char *root_0_type = root_0["type"];
              if (strcmp(root_0_type, "sgv") != 0)
              {
                Serial.print("JSON response not of type 'sgv' ");
                Serial.println(root_0_type);
              }
              else
              {
                uint64_t date = root_0["date"].as<uint64_t>();
                //Serial.println(date);
                date = date / 1000;

                {
                  int totalSeconds = (int)date;
                  sgv_date = (time_t)totalSeconds;
                }

                Serial.print("Date value ");
                Serial.println(sgv_date);

                //const char* dateString = root_0["dateString"];
                const char *direction = root_0["direction"];
                //int sgv = root_0["sgv"];
                sgv = root_0["sgv"];
                Serial.print("SGV value ");
                Serial.println(sgv);
                //            showValue(date, sgv, direction);

                ret = true;
              }
            }
          }
        }
        http.end(); //Free the resources
      }
      else
      {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
    }
  }
  return ret;
}

#define SECS_PER_HOUR (3600UL)

byte dstOffset(byte d, byte m, unsigned int y, byte h);

/* This function returns the DST offset for the current UTC time.
 * This is valid for the EU, for other places see
 * http://www.webexhibits.org/daylightsaving/i.html
 * 
 * Results have been checked for 2012-2030 (but should work since
 * 1996 to 2099) against the following references:
 * - http://www.uniquevisitor.it/magazine/ora-legale-italia.php
 * - http://www.calendario-365.it/ora-legale-orario-invernale.html
 */
byte dstOffset(byte d, byte m, unsigned int y, byte h)
{
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
