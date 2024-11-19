#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>

// Constants for screen and paddle properties
namespace carrier
{
  namespace pin
  {
    constexpr uint8_t joyClick{19}; // Button to switch to master
    constexpr uint8_t oledDcPower{6};
    constexpr uint8_t oledCs{10};
    constexpr uint8_t oledReset{5};
  }

  namespace oled
  {
    constexpr uint8_t screenWidth{128};
    constexpr uint8_t screenHeight{64};
  }
}

namespace game
{
  constexpr int paddleHeight = 15;
  constexpr int paddleWidth = 2;
  constexpr int ballSize = 4;
  constexpr float initialBallSpeedX = -2.0;
  constexpr float initialBallSpeedY = 1.0;

  int paddle1Y = 20; // Initial Y position of player 1 paddle
  int ballX = carrier::oled::screenWidth / 2;
  int ballY = carrier::oled::screenHeight / 2;
  float ballSpeedX = initialBallSpeedX;
  float ballSpeedY = initialBallSpeedY;
}

// CAN communication setup
namespace communication
{
  CAN_message_t msg;
  FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> can0;
}

// Display setup
Adafruit_SSD1306 display(carrier::oled::screenWidth,
                         carrier::oled::screenHeight,
                         &SPI,
                         carrier::pin::oledDcPower,
                         carrier::pin::oledReset,
                         carrier::pin::oledCs);

bool isMaster = false;
constexpr int Gruppenr = 1;           // Set this to your Player 1 group number
constexpr int MotstanderGruppenr = 2; // Group number of Player 2

void drawPaddleAndBall();
void handleCANInput();
void updateBallPosition();
void sendBallPosition();

void setup()
{
  Serial.begin(9600);
  communication::can0.begin();
  communication::can0.setBaudRate(250000);

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC))
  {
    Serial.println(F("ERROR: display.begin(SSD1306_SWITCHCAPVCC) failed."));
    for (;;);
  }

  display.clearDisplay();
  display.display();
  delay(1000);

  // Set joystick button as input to switch to master mode
  pinMode(carrier::pin::joyClick, INPUT_PULLUP);
}

void loop()
{
  // Check if Player 1 has pressed the joystick button to become the master
  if (digitalRead(carrier::pin::joyClick) == LOW && !isMaster)
  {
    isMaster = true;
  }

  if (isMaster)
  {
    updateBallPosition();
    sendBallPosition();
  }
  else
  {
    handleCANInput();
  }

  drawPaddleAndBall();
  delay(50); // Adjust refresh rate as needed
}

void handleCANInput()
{
  // Check for CAN messages
  if (communication::can0.read(communication::msg))
  {
    // If message is from Player 2 controlling paddle movements
    if (communication::msg.id == MotstanderGruppenr + 20)
    {
      // Check first byte to determine paddle movement direction
      if (communication::msg.buf[0] == 1) // 1 for UP
      {
        game::paddle1Y -= 2;
        if (game::paddle1Y < 0) game::paddle1Y = 0;
      }
      else if (communication::msg.buf[0] == 2) // 2 for DOWN
      {
        game::paddle1Y += 2;
        if (game::paddle1Y > carrier::oled::screenHeight - game::paddleHeight)
          game::paddle1Y = carrier::oled::screenHeight - game::paddleHeight;
      }
    }
    // If message is the ball position update from Player 2
    else if (communication::msg.id == MotstanderGruppenr + 50)
    {
      // Read the ball position from CAN message
      game::ballX = communication::msg.buf[0] | (communication::msg.buf[1] << 8);
      game::ballY = communication::msg.buf[2] | (communication::msg.buf[3] << 8);
    }
  }
}

void updateBallPosition()
{
  // Update ball position based on speed
  game::ballX += game::ballSpeedX;
  game::ballY += game::ballSpeedY;

  // Check for collisions with top and bottom walls
  if (game::ballY <= 0 || game::ballY >= carrier::oled::screenHeight - game::ballSize)
  {
    game::ballSpeedY = -game::ballSpeedY;
  }

  // Check for paddle collisions
  if (game::ballX <= game::paddleWidth)
  {
    // Collide with Player 1's paddle (left)
    if (game::ballY >= game::paddle1Y && game::ballY <= game::paddle1Y + game::paddleHeight)
    {
      game::ballSpeedX = -game::ballSpeedX;
    }
  }
  else if (game::ballX >= carrier::oled::screenWidth - game::paddleWidth - game::ballSize)
  {
    // Collide with Player 2's paddle (right)
    if (game::ballY >= game::paddle1Y && game::ballY <= game::paddle1Y + game::paddleHeight)
    {
      game::ballSpeedX = -game::ballSpeedX;
    }
  }

  // Check for scoring
  if (game::ballX < 0 || game::ballX > carrier::oled::screenWidth)
  {
    // Reset ball to center
    game::ballX = carrier::oled::screenWidth / 2;
    game::ballY = carrier::oled::screenHeight / 2;
    game::ballSpeedX = game::initialBallSpeedX;
    game::ballSpeedY = game::initialBallSpeedY * (random(0, 2) == 0 ? 1 : -1);
  }
}

void sendBallPosition()
{
  communication::msg.id = Gruppenr + 50; // Message ID for ball position
  communication::msg.len = 4;
  communication::msg.buf[0] = game::ballX & 0xFF;
  communication::msg.buf[1] = (game::ballX >> 8) & 0xFF;
  communication::msg.buf[2] = game::ballY & 0xFF;
  communication::msg.buf[3] = (game::ballY >> 8) & 0xFF;
  communication::can0.write(communication::msg);
}

void drawPaddleAndBall()
{
  display.clearDisplay();

  // Draw Player 1 paddle on the left side of the screen
  display.fillRect(0, game::paddle1Y, game::paddleWidth, game::paddleHeight, SSD1306_WHITE);

  // Draw the ball
  display.fillRect(game::ballX, game::ballY, game::ballSize, game::ballSize, SSD1306_WHITE);

  display.display();
}
