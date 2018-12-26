// RTC Libraries - https://github.com/Makuna/Rtc
#include <Wire.h>
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);

//   DS3231       ESP32
//      GND ----> GND
//      VCC ----> 3v3 (3.3V)
//      SDA ----> GPIO 21 - Wire SDA
//      SCL ----> GPIO 22 - Wire SCL

void setup() {
  Serial.begin(115200);
  Rtc.Begin();
}

bool RTC_Valid(){
  bool boolCheck = true; 
  if (!Rtc.IsDateTimeValid()){
    Serial.println("RTC lost confidence in the DateTime!  Updating DateTime");
    boolCheck = false;  
  }
  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now.  Updating Date Time");
    Rtc.SetIsRunning(true);
    boolCheck = false;
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  if (RTC_Valid()){
    RtcDateTime currTime = Rtc.GetDateTime();
    printDateTime(currTime);
  }
  delay(10000);
}

// Utility print function
#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.println(datestring);
}
