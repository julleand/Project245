// Written by K. M. Knausgård 2023-10-21

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>

#include "mas245_logo_bitmap.h"

namespace carrier
{
  namespace pin
  {
    constexpr uint8_t joyLeft{18};
    constexpr uint8_t joyRight{17};
    constexpr uint8_t joyClick{19};
    constexpr uint8_t joyUp{22};
    constexpr uint8_t joyDown{23};

    constexpr uint8_t oledDcPower{6};
    constexpr uint8_t oledCs{10};
    constexpr uint8_t oledReset{5};
  }

  namespace oled
  {
    constexpr uint8_t screenWidth{128}; // OLED display width in pixels
    constexpr uint8_t screenHeight{64}; // OLED display height in pixels
  }
}

namespace game
{
  // Paddle and ball settings
  constexpr int paddleHeight = 15;
  constexpr int paddleWidth = 2;
  constexpr int ballSize = 3;
  constexpr float initialBallSpeedX = -2.0; // Ensure the ball always starts towards player 1
  constexpr float initialBallSpeedY = 1.0;

  // Paddle and ball positions
  int paddle1Y = 20; // Y position of player 1 paddle
  int paddle2Y = 20; // Y position of player 2 paddle
  int ballX = carrier::oled::screenWidth / 2;
  int ballY = carrier::oled::screenHeight / 2;
  float ballSpeedX = initialBallSpeedX;
  float ballSpeedY = initialBallSpeedY;
}

namespace communication
{
  // CAN message setup
  CAN_message_t msg;
  FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> can0;
  // FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;
}

// Display setup
Adafruit_SSD1306 display(carrier::oled::screenWidth,
                         carrier::oled::screenHeight,
                         &SPI,
                         carrier::pin::oledDcPower,
                         carrier::pin::oledReset,
                         carrier::pin::oledCs);

bool isMaster = false;
constexpr int Gruppenr = 1; // Erstatt med korrekt gruppenummer
constexpr int MotstanderGruppenr = 2; // Erstatt med korrekt motstander gruppenummer

void drawPaddlesAndBall();
void updateBallPosition();
void handleInput();
void handleCANInput();
void drawSplash();
void sendBallPosition();

void setup()
{
  Serial.begin(9600);
  communication::can0.begin();
  communication::can0.setBaudRate(250000);
  // communication::can1.begin();
  // communication::can1.setBaudRate(250000);

  // Gen. display voltage from 3.3V (https://adafruit.github.io/Adafruit_SSD1306/html/_adafruit___s_s_d1306_8h.html#ad9d18b92ad68b542033c7e5ccbdcced0)
  if (!display.begin(SSD1306_SWITCHCAPVCC))
  {
    Serial.println(F("ERROR: display.begin(SSD1306_SWITCHCAPVCC) failed."));

    for (;;)
    {
      // Blink angry LED pattern or something, initializing LED failed.
    }
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

  // Set joystick pins as input
  pinMode(carrier::pin::joyUp, INPUT_PULLUP);
  pinMode(carrier::pin::joyDown, INPUT_PULLUP);
  pinMode(carrier::pin::joyLeft, INPUT_PULLUP);
  pinMode(carrier::pin::joyRight, INPUT_PULLUP);
  pinMode(carrier::pin::joyClick, INPUT_PULLUP);
}

void loop()
{
  handleInput();
  handleCANInput();
  updateBallPosition();
  drawPaddlesAndBall();

  if (isMaster)
  {
    sendBallPosition();
  }

  delay(50);
}

void handleInput()
{
  bool moved = false;

  // Check if we are the master by pressing joystick click
  if (digitalRead(carrier::pin::joyClick) == LOW && !isMaster)
  {
    isMaster = true;
  }

  // Read joystick inputs to control paddle 1
  if (digitalRead(carrier::pin::joyUp) == LOW)
  {
    game::paddle1Y -= 2;
    if (game::paddle1Y < 0)
    {
      game::paddle1Y = 0;
    }
    moved = true;
  }

  if (digitalRead(carrier::pin::joyDown) == LOW)
  {
    game::paddle1Y += 2;
    if (game::paddle1Y > carrier::oled::screenHeight - game::paddleHeight)
    {
      game::paddle1Y = carrier::oled::screenHeight - game::paddleHeight;
    }
    moved = true;
  }

  // If we moved the paddle, send a CAN message
  if (moved)
  {
    communication::msg.id = Gruppenr + 20; // Bruk ID = Gruppenr + 20
    communication::msg.len = 2; // Lengde på meldingen
    communication::msg.buf[0] = game::paddle1Y & 0xFF; // Posisjonen til paddelen (liten del)
    communication::msg.buf[1] = (game::paddle1Y >> 8) & 0xFF; // Posisjonen til paddelen (stor del)
    communication::can0.write(communication::msg); // Send CAN-melding
  }
}

void handleCANInput()
{
  // Read CAN message to control paddle 2
  if (communication::can0.read(communication::msg))
  {
    if (communication::msg.id == MotstanderGruppenr + 20) // Bruk CAN-ID fra motstandergruppen
    {
      int receivedY = communication::msg.buf[0] | (communication::msg.buf[1] << 8); // Rekonstruer Y-posisjonen
      game::paddle2Y = receivedY;

      // Begrens Y-posisjon til skjermens høyde
      if (game::paddle2Y < 0)
      {
        game::paddle2Y = 0;
      }
      else if (game::paddle2Y > carrier::oled::screenHeight - game::paddleHeight)
      {
        game::paddle2Y = carrier::oled::screenHeight - game::paddleHeight;
      }
    }
    else if (communication::msg.id == MotstanderGruppenr + 50) // Motta ballposisjon
    {
      game::ballX = communication::msg.buf[0] | (communication::msg.buf[1] << 8);
      game::ballY = communication::msg.buf[2] | (communication::msg.buf[3] << 8);
    }
  }
}

void updateBallPosition()
{
  if (!isMaster)
    return;

  // Update ball position
  game::ballX += game::ballSpeedX;
  game::ballY += game::ballSpeedY;

  // Check for collisions with top and bottom walls
  if (game::ballY <= 0 || game::ballY >= carrier::oled::screenHeight - game::ballSize)
  {
    game::ballSpeedY = -game::ballSpeedY;
  }

  // Check for collisions with paddles
  if (game::ballX <= game::paddleWidth)
  {
    if (game::ballY >= game::paddle1Y && game::ballY <= game::paddle1Y + game::paddleHeight)
    {
      game::ballSpeedX = -game::ballSpeedX;
    }
  }
  else if (game::ballX >= carrier::oled::screenWidth - game::paddleWidth - game::ballSize)
  {
    if (game::ballY >= game::paddle2Y && game::ballY <= game::paddle2Y + game::paddleHeight)
    {
      game::ballSpeedX = -game::ballSpeedX;
    }
  }

  // Check for scoring
  if (game::ballX < 0 || game::ballX > carrier::oled::screenWidth)
  {
    // Reset ball to center and ensure it starts towards player 1
    game::ballX = carrier::oled::screenWidth / 2;
    game::ballY = carrier::oled::screenHeight / 2;
    game::ballSpeedX = game::initialBallSpeedX;
    game::ballSpeedY = game::initialBallSpeedY * (random(0, 2) == 0 ? 1 : -1); // Randomize initial Y direction
  }
}

void sendBallPosition()
{
  communication::msg.id = Gruppenr + 50; // ID for ballmelding
  communication::msg.len = 4; // Lengde på meldingen
  communication::msg.buf[0] = game::ballX & 0xFF;
  communication::msg.buf[1] = (game::ballX >> 8) & 0xFF;
  communication::msg.buf[2] = game::ballY & 0xFF;
  communication::msg.buf[3] = (game::ballY >> 8) & 0xFF;
  communication::can0.write(communication::msg); // Send ballposisjonen via CAN
}

void drawPaddlesAndBall()
{
  display.clearDisplay();

  // Draw paddles
  display.fillRect(0, game::paddle1Y, game::paddleWidth, game::paddleHeight, SSD1306_WHITE);
  display.fillRect(carrier::oled::screenWidth - game::paddleWidth, game::paddle2Y, game::paddleWidth, game::paddleHeight, SSD1306_WHITE);

  // Draw ball
  display.fillRect(game::ballX, game::ballY, game::ballSize, game::ballSize, SSD1306_WHITE);

  display.display();
}

void drawSplash(void)
{
  namespace splash = images::mas245splash;
  display.clearDisplay();
  display.drawBitmap(0, 0, splash::bitmap, splash::width, splash::height, 1);
  display.display();
}
