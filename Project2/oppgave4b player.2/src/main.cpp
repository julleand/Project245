#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SPI.h>
#include <Wire.h>

// Constants for screen and paddle properties
namespace carrier
{
  namespace pin
  {
    constexpr uint8_t joyUp{22};
    constexpr uint8_t joyDown{23};
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

  int paddle1Y = 20; // Player 1 paddle position
  int paddle2Y = 20; // Opponent paddle position (from Player 2)
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
bool otherIsMaster = false; // Indicates if the other player is master
constexpr int Gruppenr = 2;           // Set this to Player 2's number
constexpr int MotstanderGruppenr = 1; // Player 1's number

void handleInput();
void handleCANInput();
void gameMasterControll();
void sendGameState();
void drawPaddlesAndBall();
void checkIfMaster();

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

  // Set joystick pins as input
  pinMode(carrier::pin::joyUp, INPUT_PULLUP);
  pinMode(carrier::pin::joyDown, INPUT_PULLUP);
  pinMode(carrier::pin::joyClick, INPUT_PULLUP);
}

void loop()
{
  checkIfMaster(); // Check if other player is master

  if (!otherIsMaster)
  {
    // Check if Player 1 has pressed the joystick button to become the master
    if (digitalRead(carrier::pin::joyClick) == LOW && !isMaster)
    {
      isMaster = true;
    }
  }

  handleInput(); // Control paddle

  if (isMaster)
  {
    gameMasterControll(); // uppdate ball position on screen 
  }

  sendGameState(); // Send game state to slve 
  handleCANInput(); // Receive game state from other player  
  drawPaddlesAndBall();
  delay(50); // Adjust refresh rate as needed
}

void checkIfMaster()
{
  // If this player is the master, announce it over CAN
  if (isMaster)
  {
    communication::msg.id = 100; // ID for master announcement
    communication::msg.len = 1;
    communication::msg.buf[0] = 1; // Indicates this player is master
    communication::can0.write(communication::msg);
  }

  // If this player is not the master, send a message confirming this
  if (!isMaster)
  {
    communication::msg.id = 101; // ID for slave response
    communication::msg.len = 1;
    communication::msg.buf[0] = 0; // Indicates this player is not master
    communication::can0.write(communication::msg);
  }

  // Check for messages from the other player
  if (communication::can0.read(communication::msg))
  {
    // Other player announces they are the master
    if (communication::msg.id == 100 && communication::msg.buf[0] == 1)
    {
      otherIsMaster = true;
    }
    // Other player confirms they are not master
    else if (communication::msg.id == 101 && communication::msg.buf[0] == 0)
    {
      otherIsMaster = false;
    }
  }
}

void handleInput()
{
  // Read joystick inputs to control paddle movement
  if (digitalRead(carrier::pin::joyUp) == LOW) // UP
  {
    game::paddle2Y -= 2;
    if (game::paddle2Y < 0)
      game::paddle2Y = 0;
  }

  if (digitalRead(carrier::pin::joyDown) == LOW) // DOWN
  {
    game::paddle2Y += 2;
    if (game::paddle2Y > carrier::oled::screenHeight - game::paddleHeight)
      game::paddle2Y = carrier::oled::screenHeight - game::paddleHeight;
  }
}

void handleCANInput()
{
  // Check if a CAN message has been received
  if (communication::can0.read(communication::msg))
  {
    // Case 1: Receive paddle position update from Player 1 (Slave sends this)
    if (communication::msg.id == MotstanderGruppenr + 20) // CAN ID = 21 (Player 1 + 20)
    {
      // Update Player 1's paddle position (paddle1Y)
      game::paddle1Y = communication::msg.buf[0] | (communication::msg.buf[1] << 8);
    }
    // Case 2: Receive game state update from Player 1 (Master sends this)
    else if (communication::msg.id == MotstanderGruppenr + 50) // CAN ID = 51 (Player 1 + 50)
    {
      // Update ball position
      game::ballX = communication::msg.buf[0] | (communication::msg.buf[1] << 8);
      game::ballY = communication::msg.buf[2] | (communication::msg.buf[3] << 8);

      // Update Player 1's paddle position (paddle1Y)
      game::paddle1Y = communication::msg.buf[4] | (communication::msg.buf[5] << 8);
    }
  }
}

void gameMasterControll()
{
  game::ballX += game::ballSpeedX;
  game::ballY += game::ballSpeedY;

  // Collision with top and bottom walls
  if (game::ballY <= 0 || game::ballY >= carrier::oled::screenHeight - game::ballSize)
    game::ballSpeedY = -game::ballSpeedY;

  // Collision with Player 2 paddle
  if (game::ballX >= carrier::oled::screenWidth - game::paddleWidth - game::ballSize)
  {
    if (game::ballY >= game::paddle2Y && game::ballY <= game::paddle2Y + game::paddleHeight)
      game::ballSpeedX = -game::ballSpeedX;
  }

  // Collision with Player 1 paddle
  if (game::ballX <= game::paddleWidth)
  {
    if (game::ballY >= game::paddle1Y && game::ballY <= game::paddle1Y + game::paddleHeight)
      game::ballSpeedX = -game::ballSpeedX;
  }

  // Scoring (ball out of bounds)
  if (game::ballX < 0 || game::ballX > carrier::oled::screenWidth)
  {
    game::ballX = carrier::oled::screenWidth / 2;
    game::ballY = carrier::oled::screenHeight / 2;
    game::ballSpeedX = game::initialBallSpeedX;
    game::ballSpeedY = game::initialBallSpeedY * (random(0, 2) == 0 ? 1 : -1);
  }
}

void sendGameState()
{
if (!isMaster)
  {
  // Send paddle position if in slave mode
  communication::msg.id = Gruppenr + 20; // CAN ID for Player 1's paddle position
  communication::msg.len = 2;
  communication::msg.buf[0] = game::paddle2Y & 0xFF;          // Lower byte
  communication::msg.buf[1] = (game::paddle2Y >> 8) & 0xFF;   // Upper byte
  communication::can0.write(communication::msg);
  }

if (isMaster)
  {
  communication::msg.id = Gruppenr + 50;
  communication::msg.len = 6;
  communication::msg.buf[0] = game::ballX & 0xFF;
  communication::msg.buf[1] = (game::ballX >> 8) & 0xFF;
  communication::msg.buf[2] = game::ballY & 0xFF;
  communication::msg.buf[3] = (game::ballY >> 8) & 0xFF;
  communication::msg.buf[4] = game::paddle2Y & 0xFF;
  communication::msg.buf[5] = (game::paddle2Y >> 8) & 0xFF;
  communication::can0.write(communication::msg);
  }
}

void drawPaddlesAndBall()
{
  display.clearDisplay();

  // Draw Player 1 paddle on the left side of the screen
  display.fillRect(0, game::paddle1Y, game::paddleWidth, game::paddleHeight, SSD1306_WHITE);

  // Draw Player 2 paddle on the right side of the screen
  display.fillRect(carrier::oled::screenWidth - game::paddleWidth, game::paddle2Y,
                   game::paddleWidth, game::paddleHeight, SSD1306_WHITE);

  // Draw the ball
  display.fillRect(game::ballX, game::ballY, game::ballSize, game::ballSize, SSD1306_WHITE);

  display.display();
}
