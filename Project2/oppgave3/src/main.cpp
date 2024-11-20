#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>
#include "mas245_logo_bitmap.h"

// Namespace declarations remain unchanged
namespace carrier {
  namespace pin {
    constexpr uint8_t joyLeft{18};
    constexpr uint8_t joyRight{17};
    constexpr uint8_t joyClick{19};
    constexpr uint8_t joyUp{22};
    constexpr uint8_t joyDown{23};

    constexpr uint8_t oledDcPower{6};
    constexpr uint8_t oledCs{10};
    constexpr uint8_t oledReset{5};
  }

  namespace oled {
    constexpr uint8_t screenWidth{128};
    constexpr uint8_t screenHeight{64};
  }
}

namespace images {
  namespace pumpkin {
    constexpr uint8_t width{16};
    constexpr uint8_t height{18};

    constexpr static uint8_t PROGMEM bitmap[] = {
      0b00000000, 0b00100000,
      0b00000000, 0b11100000,
      0b00000001, 0b10000000,
      0b00000001, 0b10000000,
      0b00000001, 0b10000000,
      0b00000011, 0b11100000,
      0b00001111, 0b11111000,
      0b00111111, 0b11111000,
      0b01111111, 0b11111110,
      0b01111111, 0b11111110,
      0b11111111, 0b11111111,
      0b11111111, 0b11111111,
      0b11111111, 0b11111111,
      0b01111111, 0b11111110,
      0b01111111, 0b11111110,
      0b00011111, 0b11111000,
      0b00011111, 0b11110000,
      0b00000111, 0b11100000,
    };
  };
}

namespace {
  CAN_message_t msg;
  FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> can0;

  Adafruit_SSD1306 display(carrier::oled::screenWidth,
                           carrier::oled::screenHeight,
                           &SPI,
                           carrier::pin::oledDcPower,
                           carrier::pin::oledReset,
                           carrier::pin::oledCs);
  uint32_t receivedMessageCount = 0;
  uint32_t lastReceivedMessageID = 0;
}

struct Message {
  uint8_t sequenceNumber;
  float temperature;
};

void drawSplash();
void demoMessage();
void receiveCan();
void drawFrameWithTitleAndArc();
void sendCan(int16_t x, int16_t y);

void setup() {
  Serial.begin(9600);
  can0.begin();
  can0.setBaudRate(250000);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("ERROR: display.begin(SSD1306_SWITCHCAPVCC) failed."));
    for (;;) { }
  }

  display.clearDisplay();
  display.display();
  delay(2000);

  drawSplash();
  delay(2000);
  display.invertDisplay(true);
  delay(500);
  display.invertDisplay(false);
  delay(1000);
  display.invertDisplay(true);
  delay(100);
  display.invertDisplay(false);
  delay(1000);

  Serial.print(F("float size in bytes: "));
  Serial.println(sizeof(float));
}

void loop() {
  demoMessage(); 
  // lager størelsen til sirkelen og banen
  const int16_t centerX = carrier::oled::screenWidth / 2;
  const int16_t centerY = 60;
  const int16_t radiusX = 50;
  const int16_t radiusY = 4;
  const int16_t circleRadius = 3;
  int16_t lastX = 0, lastY = 0;

  for (float angle = 0; angle <= 360; angle += 5) {
    float radians = angle * PI / 180.0;
    int16_t x = centerX + radiusX * cos(radians);
    int16_t y = centerY + radiusY * sin(radians);
    receiveCan();
        // Fjerner sirkelen
    display.fillCircle(lastX, lastY, circleRadius, SSD1306_BLACK);

    // Tegner sirkelen
    display.fillCircle(x, y, circleRadius, SSD1306_WHITE);

    display.display();

    lastX = x;
    lastY = y;
    
    delay(100);
    
    sendCan(x, y);
  }
  // Dette gjør det samme bare andre veien
  for (float angle = 360; angle >= 0; angle -= 5) {
    float radians = angle * PI / 180.0;
    int16_t x = centerX + radiusX * cos(radians);
    int16_t y = centerY + radiusY * sin(radians);
    receiveCan();
    
    display.fillCircle(lastX, lastY, circleRadius, SSD1306_BLACK);

   
    display.fillCircle(x, y, circleRadius, SSD1306_WHITE);

    display.display();

    lastX = x;
    lastY = y;
    
    delay(100);
   
    sendCan(x, y);
  }
}
// sender kordinatene til PCAN view
void sendCan(int16_t x, int16_t y) {
  CAN_message_t msg;
  msg.id = 0x245;
  msg.len = 4;

  msg.buf[0] = x & 0xFF;
  msg.buf[1] = (x >> 8) & 0xFF;
  msg.buf[2] = y & 0xFF;
  msg.buf[3] = (y >> 8) & 0xFF;

  if (can0.write(msg) < 0) {
    Serial.println("CAN send failed.");
  } else {
    Serial.print("Sent Coordinates - X: ");
    Serial.print(x);
    Serial.print(", Y: ");
    Serial.println(y);
  }
}

void receiveCan() {
  if (can0.read(msg)) {
    receivedMessageCount++;
    lastReceivedMessageID = msg.id;
  }
}

void demoMessage() {
  drawFrameWithTitleAndArc();
  // Setter opp Displayet
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.println(F("CAN-statistikk"));
  display.println(F("-------------------"));
  display.print(F("Antall mottatt: "));
  display.println(receivedMessageCount);
  display.print(F("Mottok sist ID: 0x"));
  display.println(lastReceivedMessageID, HEX);
  display.println(F("-------------------"));

  display.display();
}

void drawSplash() {
  namespace splash = images::mas245splash;
  display.clearDisplay();
  display.drawBitmap(0, 0, splash::bitmap, splash::width, splash::height, 1);
  display.display();
}

void drawFrameWithTitleAndArc() {
  display.clearDisplay();
  // lager overskriften
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 2);
  display.println(F("MAS245 - Gruppe 3"));
  // lager linjen under overskriften
  for (int16_t x = 0; x < carrier::oled::screenWidth; x++) {
    int16_t y = 15 - (int16_t)(0.1 * (x - 64) * (x - 64) / 64);
    display.drawPixel(x, y, SSD1306_WHITE);
  }

  display.display();
}
