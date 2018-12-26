// https://github.com/ThingPulse/esp8266-oled-ssd1306
//#include "SSD1306Spi.h" //SPI mode
#include "SSD1306Wire.h"  //I2C mode

// Include custom images
#include "images.h"

//My module pintout from TOP
// SPI-mode:  MOSI  SCLK  D/C RST CS  3.3 Vin GND
// I2c-mode:  SDA   SCL   SAO RST NC  3.3 Vin GND

// Initialize the OLED display using SPI
//  SSD1306       ESP32
//      GND ----> GND
//      VCC ----> 3v3 (3.3V)
//       CS ----> GPIO  5 Chip Select
//      RST ----> GPIO 16 Reset
//      D/C ----> GPIO 17 Data/Command select
//     SCLK ----> GPIO 18 SCK (SPI Clock) 
//     MOSI ----> GPIO 23 MOSI
//#define RES  16
//#define DC   17
//#define CS    5
//SSD1306Spi        display(RES, DC, CS);


// Initialize the OLED display using I2C (connected)
//  SSD1306       ESP32
//      GND ----> GND
//      VCC ----> 3v3 (3.3V)
//      RST ----> GPIO 16 Reset
//      SAO ----> N/C
//      SCL ----> GPIO 22 - Wire SCL
//      SDA ----> GPIO 21 - Wire SDA
#define ADDRESS   0x3d
#define SDA   21
#define SDC   22
SSD1306Wire  display(ADDRESS, SDA, SDC);

#define DEMO_DURATION 3000
typedef void (*Demo)(void);

int demoMode = 0;
int counter = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup5");

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}

void drawProgressBarDemo() {
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}

void drawImageDemo() {
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files
    display.drawXbm(34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
}

Demo demos[] = {drawProgressBarDemo, drawImageDemo};
int demoLength = (sizeof(demos) / sizeof(Demo));
long timeSinceLastModeSwitch = 0;

void loop() {
  //Serial.println("loop");
  // clear the display
  display.clear();
  
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(10, 128, String(millis()));
  // write the buffer to the display
  display.display();
  
  if (millis() - timeSinceLastModeSwitch > DEMO_DURATION) {
    demoMode = (demoMode + 1)  % demoLength;
    timeSinceLastModeSwitch = millis();
  }
  counter++;
  delay(10);
}
