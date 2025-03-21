#include <random>
#include <iostream>
#include <Arduino.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "levels.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "xtensa/xtensa_context.h"
#include "BluetoothSerial.h"


// Pin constants
#define BOOT_PIN 0 
#define TFT_CS   22
#define TFT_DC   21
#define TFT_LED   5
#define TFT_RST  33

// Screen constants
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 15
#define TOP_BUFFER 15
#define DISPLAY_OK 0xCA

// Game constants
#define MAX_SPEED 7.0
#define STARTER_SPEED 3.5
#define MAX_LIVES 6
#define STARTER_LIVES 3
#define PADDLE_SPEED_DAMPEN paddle_speed_fn
#define PADDLE_SPEED_CONST_MUL 1.45
#define BRICK_INCR_AMT 5
#define MIN_BRICK_HEIGHT 265
#define DEFAULT_INTERVAL 10000
#define MIN_INTERVAL 3000
#define DELTA_INTERVAL 500

// DEBUG
#define DEBUG

// ESP Globals
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
BluetoothSerial SerialBT;
GFXcanvas1 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);

// Macro functions
float paddle_speed_fn(float speed) {
    return sqrt(speed) * PADDLE_SPEED_CONST_MUL;
}


// Ball info
const int radius = 3;
float x = 30, y = 100;
float dx = 2, dy = -2;  
float speed = STARTER_SPEED;
bool ballOnPaddle = true;
float launchAngle = 150; // Angle in degrees
int prevEndX = -1;
int prevEndY = -1;
int collided_r, collided_c = -1;

// Paddle info
const int paddleWidth = 50;
const int paddleHeight = 5;
float paddleX = (SCREEN_WIDTH - paddleWidth) / 2;
const int paddleY = SCREEN_HEIGHT - 10; // 20 pixels from bottom
const float BOUNCE_FACTOR = 0.9; // Adjust how much the angle changes
float paddle_speed = PADDLE_SPEED_DAMPEN(speed);
bool left = true;
int leftbound = 5;
int rightbound = 5;
int target_coord = 0;

// Game info
LevelInfo CurrentLevel;
int currentLevel = 0;
int numLevels = sizeof(levels) / sizeof(LevelInfo);
int points = 0;
int lives = STARTER_LIVES;
bool redraw_header = false;
unsigned long lastInterval = 0;
float interval = DEFAULT_INTERVAL; // Start with 10 seconds (10,000 ms)
const float minInterval = MIN_INTERVAL; // Minimum interval of 5 seconds
bool game_started = false;

// Debug info
int dbgline = 10;
bool setup_complete = false;
bool screen_init = false;

// Debug functions
void drawdebugtext(const char* text) {
    tft.setTextSize(1);
    tft.setCursor(10, dbgline);
    dbgline += 10;
    tft.println(text);
}

// Function to display each line separately
void draw_panic_log(const char *text) {
    char buffer[256];  
    strncpy(buffer, text, sizeof(buffer));  
    buffer[sizeof(buffer) - 1] = '\0';  

    char *line = strtok(buffer, "\n");  

    while (line != NULL) {
        drawdebugtext(line);  // Call drawdebugtext for each line
        line = strtok(NULL, "\n");  
    }
}

// Format and display the panic message
void display_panic_message(const XtExcFrame *exc_frame) {
    char line[64];

    sprintf(line, "KERNEL PANIC, ABORTING...");
    drawdebugtext(line);

    // Program Counter, Stack Pointer, Link Register
    sprintf(line, "PC : 0x%08X", exc_frame->pc);
    drawdebugtext(line);

    sprintf(line, "LR : 0x%08X", exc_frame->a0);
    drawdebugtext(line);

    sprintf(line, "SP : 0x%08X", exc_frame->a1);
    drawdebugtext(line);

    // Print general-purpose registers A2–A15
    for (int i = 2; i <= 15; i++) {
        sprintf(line, "A%-2d: 0x%08X", i, *((uint32_t *)exc_frame + i));
        drawdebugtext(line);
    }

    // Exception cause and address
    sprintf(line, "EXCCAUSE : %d", exc_frame->exccause);
    drawdebugtext(line);

    sprintf(line, "EXCVADDR : 0x%08X", exc_frame->excvaddr);
    drawdebugtext(line);

    // Print special-purpose registers (if accessible)
    uint32_t ps;
    asm volatile("rsr.ps %0" : "=r"(ps));  // Processor State Register
    sprintf(line, "PS : 0x%08X", ps);
    drawdebugtext(line);

    uint32_t sar;
    asm volatile("rsr.sar %0" : "=r"(sar));  // Shift Amount Register
    sprintf(line, "SAR : 0x%08X", sar);
    drawdebugtext(line);

    char msg[40];
    sprintf(msg, "FREE HEAP MEMORY: %dB (%dKB)", esp_get_free_heap_size(), esp_get_free_heap_size()/1000);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    sprintf(msg, "FREE STACK MEMORY: %dB (%dKB)", stack_remaining, stack_remaining/1000);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif
}

// Panic Handler
extern "C" void __wrap_esp_panic_handler(void *frame) {
    tft.fillScreen(ILI9341_BLUE);
    dbgline = 10;

    // Cast frame to a CPU context structure
    XtExcFrame *exc_frame = (XtExcFrame *)frame;

    // Print a warning message
    if (screen_init)
        display_panic_message(exc_frame);

    // Print a panic message
    ESP_LOGE("PANIC", "KERNEL PANIC, ABORTING...");

    // Extract and print CPU register values
    ESP_LOGE("PANIC", "PC        (Program Counter)  : 0x%08X", exc_frame->pc);
    ESP_LOGE("PANIC", "LR        (Link Register)    : 0x%08X", exc_frame->a0);
    ESP_LOGE("PANIC", "SP        (Stack Pointer)    : 0x%08X", exc_frame->a1);

    // Print all general-purpose registers (A2–A15)
    for (int i = 2; i <= 15; i++) {
        ESP_LOGE("PANIC", "A%-2d       : 0x%08X", i, *((uint32_t *)exc_frame + i));
    }

    ESP_LOGE("PANIC", "EXCCAUSE  (Exception Cause)  : %d", exc_frame->exccause);
    ESP_LOGE("PANIC", "EXCVADDR  (Faulting Address) : 0x%08X", exc_frame->excvaddr);

    // Read special-purpose registers (Processor State Register and Shift Amount Register)
    uint32_t ps, sar;
    asm volatile("rsr.ps %0" : "=r"(ps));   // Read Processor State Register
    asm volatile("rsr.sar %0" : "=r"(sar)); // Read Shift Amount Register

    ESP_LOGE("PANIC", "PS        (Processor State)  : 0x%08X", ps);
    ESP_LOGE("PANIC", "SAR       (Shift Amount)     : 0x%08X", sar);
        
    Serial.println("KERNEL PANIC");

    // Optional: Trigger watchdog to reset
    esp_task_wdt_init(1, true);
    esp_task_wdt_add(NULL);
    
    // Wait for logs to be flushed before restarting
    vTaskDelay(pdMS_TO_TICKS(20000));  // Give 5 seconds for logs to be read

    #ifndef DEBUG
        esp_restart();
    #endif
}

int getRandomInt(int min, int max) {
    return min + (esp_random() % (max - min + 1));
}

float getRandomFloat(float min, float max) {
    return min + (max - min) * (esp_random() / (float)UINT32_MAX);
}

uint16_t getBrickColor(int durability) {
    switch (durability) {
        case 3: return ILI9341_GREEN;
        case 2: return ILI9341_ORANGE;
        case 1: return ILI9341_RED;
        default: return ILI9341_BLACK;
    }
}

void drawloadtext() {
    tft.setTextColor(ILI9341_WHITE); // Set text color to white
    tft.setTextSize(2); // Text size 2

    int charWidth = 6 * 2; // Each character is 12 pixels wide
    int charHeight = 8 * 2; // Each character is 16 pixels tall
    int textLength = 10; // "LOADING..." has 10 characters
    int textWidth = textLength * charWidth;
    int textHeight = charHeight;
    
    int x = (240 - textWidth) / 2;
    int y = (320 - textHeight) / 2;

    tft.setCursor(x, y);
    tft.print("LOADING...");
}

void loadLevel(int levelIndex) {
    drawloadtext();
    delay(200);
    // Load the level from PROGMEM
    memcpy_P(&CurrentLevel, &levels[levelIndex], sizeof(LevelInfo));

    // Calculate X-offset using parameters
    int numCols = CurrentLevel.brickCols;
    int numRows = CurrentLevel.brickRows;
    int height = CurrentLevel.brickHeight;
    int width = CurrentLevel.brickWidth;
    int spacing = CurrentLevel.brickSpacing;
    CurrentLevel.brickOffsetX = (SCREEN_WIDTH - (numCols * (width + spacing)))/2;
    CurrentLevel.brickOffsetY = HEADER_HEIGHT + TOP_BUFFER;
}

void drawHeader() {
    tft.fillRect(0, 0, tft.width(), HEADER_HEIGHT, ILI9341_BLUE); // Draw header background
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);

    // Draw Level
    tft.setCursor(5, 5);
    tft.print("Level: ");
    tft.print(currentLevel + 1);

    // Draw Points
    tft.setCursor(80, 5);
    tft.print("Points: ");
    tft.print(points);

    // Draw Lives as balls
    int livesX = 180; // Starting X position for lives
    int livesY = 4;   // Y position for lives
    int ballRadius = 3; // Ball size

    for (int i = 0; i < MAX_LIVES; i++) {
        if (i < lives) {
            if (i < STARTER_LIVES)
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_WHITE); // Filled for active life
            else 
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_GREEN); // Filled for active life
        } else if (i < STARTER_LIVES) {
            tft.drawCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_DARKGREY); // Outline for lost life
        } 
    }
}


void drawBrick(int row, int col, bool overridecol = false, uint16_t color = ILI9341_BLACK) {
    int durability = CurrentLevel.bricks[row][col];
    if (durability > 0) {
        int bx = CurrentLevel.brickOffsetX + col * (CurrentLevel.brickWidth + CurrentLevel.brickSpacing);
        int by = CurrentLevel.brickOffsetY + row * (CurrentLevel.brickHeight + CurrentLevel.brickSpacing);

        if (!overridecol)
            tft.fillRect(bx, by, CurrentLevel.brickWidth, CurrentLevel.brickHeight, getBrickColor(durability));
        else
            tft.fillRect(bx, by, CurrentLevel.brickWidth, CurrentLevel.brickHeight, color);
    }
}

void clearBricks() {
    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            drawBrick(r, c, true);
        }
    }
}

void drawAllBricks() {
    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            drawBrick(r, c);
        }
    }
}

void moveBricksDown(int amount) {
    clearBricks();
    CurrentLevel.brickOffsetY += BRICK_INCR_AMT;
    drawAllBricks();
}

void resetGame() {
    tft.fillScreen(ILI9341_BLACK);
    drawHeader();

    paddleX = (SCREEN_WIDTH - paddleWidth) / 2;
    x = paddleX + paddleWidth/2;
    y = paddleY - paddleHeight - radius-1;
    ballOnPaddle = true;
    launchAngle = getRandomInt(30, 150);

    // Increment speed
    speed = min(speed + 0.15, MAX_SPEED);

    // Normalize direction and apply new speed
    float norm = sqrt(dx * dx + dy * dy);
    dx = (dx / norm) * speed;
    dy = (dy / norm) * speed;
    paddle_speed = PADDLE_SPEED_DAMPEN(speed); 

    // Reset brick movement interval
    interval = DEFAULT_INTERVAL;

    // Reset collided brick
    collided_r = -1;
    collided_c = -1;

    drawAllBricks();

    // Draw lose boundary
    tft.drawLine(0, MIN_BRICK_HEIGHT, SCREEN_WIDTH, MIN_BRICK_HEIGHT, ILI9341_RED);
}

void nextLevel() {
    tft.fillScreen(ILI9341_BLACK);
    // Load next level from memory
    currentLevel = (currentLevel + 1);
    loadLevel(currentLevel % numLevels);
    Serial.println("NEWLEVEL LOADED FROM PROGMEM...");

    // Reset game params, draw bricks
    resetGame();
    Serial.println("GAME STATE RESET...");

    // Query display status
    uint8_t status = tft.readcommand8(ILI9341_RDMODE);
    Serial.print("DISPLAY CHECK... STATUS = 0x");
    Serial.println(status, HEX);

    if (status != DISPLAY_OK) {
        Serial.println("DISPLAY CHECK FAIL, ABORTING...");
        esp_system_abort("DISPLAY CHECK FAIL");
    }

    Serial.println("OK!");

    Serial.println("NEWLEVEL BEGIN...");
}

// Powerup drawing
void drawMissile(int x, int y) {
    tft.fillTriangle(x, y, x - 3, y + 8, x + 3, y + 8, ILI9341_RED); // Rocket tip
    tft.fillRect(x - 2, y + 8, 4, 10, ILI9341_WHITE); // Rocket body
    tft.fillTriangle(x - 3, y + 18, x + 3, y + 18, x, y + 22, ILI9341_ORANGE); // Rocket flames
}

void drawExtraBalls(int x, int y) {
    tft.fillCircle(x - 6, y, 3, ILI9341_CYAN);
    tft.fillCircle(x, y, 3, ILI9341_CYAN);
    tft.fillCircle(x + 6, y, 3, ILI9341_CYAN);
}

void drawLargeBall(int x, int y) {
    tft.fillCircle(x, y, 6, ILI9341_YELLOW);
}

void drawLaser(int x, int y) {
    tft.fillRoundRect(x - 10, y, 20, 6, 3, ILI9341_RED);
}

float roundToNearestHalf(float value) {
    return round(value * 2.0f) / 2.0f;
}

// Update paddle position (to be called in loop)
void movePaddleDraw(float direction) {
    // Save the old paddle position
    float oldPaddleX = paddleX;

    // Update the paddle position
    paddleX += roundToNearestHalf(direction); // Move left or right
    paddleX = max(0.0f, min((float)(SCREEN_WIDTH - paddleWidth), paddleX)); // Keep in bounds

    // Only redraw if the paddle has actually moved
    if (oldPaddleX != paddleX) {
        // Overdraw the old paddle with a black box
        if (paddleX > oldPaddleX)
            tft.fillRect(floor(oldPaddleX), paddleY, ceil(paddleX - oldPaddleX), paddleHeight, ILI9341_BLACK);
        else 
            tft.fillRect(floor(paddleX+paddleWidth), paddleY, ceil(oldPaddleX - paddleX), paddleHeight, ILI9341_BLACK);

        // Draw the new paddle at the updated position
        tft.fillRect(paddleX, paddleY, paddleWidth, paddleHeight, ILI9341_WHITE);
    }
}

void movePaddle(float direction) {
    for (float i = 0; i < abs(direction); i++) {
        if (direction > 0) {
            movePaddleDraw(1);
        } else {
            movePaddleDraw(-1);
        }
    }
}

void drawLaunchAngleIndicator() {
    int indicatorLength = 20; // Length of the indicator
    
    // Convert angle to radians
    double angleRad = launchAngle * M_PI / 180.0;
    
    // Ball position (assuming it's at the center of the paddle)
    int ballX = x;
    int ballY = y;

    // Calculate the new end point of the indicator
    int endX = ballX + indicatorLength * cos(angleRad);
    int endY = ballY - indicatorLength * sin(angleRad);

    // Erase previous line if it exists
    if (prevEndX != -1 && prevEndY != -1 && prevEndX != endX && prevEndY != endY) {
        tft.drawLine(ballX, ballY, prevEndX, prevEndY, ILI9341_BLACK);
    }

    // Draw new indicator line
    tft.drawLine(ballX, ballY, endX, endY, ILI9341_WHITE);

    // Store new end position
    prevEndX = endX;
    prevEndY = endY;
}

void increaseLaunchAngle() {
    if (launchAngle < 150) {
        launchAngle += 5;
        drawLaunchAngleIndicator();
    }
}

void decreaseLaunchAngle() {
    if (launchAngle > 30) {
        launchAngle -= 5;
        drawLaunchAngleIndicator();
    }
}

void launchBall() {
    dx = speed * cos(launchAngle * M_PI / 180.0);
    dy = -speed * sin(launchAngle * M_PI / 180.0);
    ballOnPaddle = false;
    lastInterval = millis();
}

void handleInput() {
    if (digitalRead(BOOT_PIN) == LOW) {
        launchBall();
    }
}

bool check_game_finished() {
    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            if (CurrentLevel.bricks[r][c] != 0)
                return false;
        }
    }
    return true;
}

const char* chip_model_to_string(esp_chip_model_t chip_model) {
    switch(chip_model) {
        case CHIP_ESP32:
            return "CPU_ESP32";
        case CHIP_ESP32S2:
            return "CPU_ESP32-S2";
        case CHIP_ESP32S3:
            return "CPU_ESP32-S3";
        case CHIP_ESP32C3:
            return "CPU_ESP32-C3";
        case CHIP_ESP32H2:
            return "CPU_ESP32-H2";
        default:
            return "Unknown Chip Model";
    }
}

const char* reset_reason_to_string(esp_reset_reason_t reset_reason) {
    switch (reset_reason) {
        case ESP_RST_UNKNOWN:
            return "UNKNOWN RESET REASON";
        case ESP_RST_POWERON:
            return "POWER-ON RESET";
        case ESP_RST_EXT:
            return "EXTERNAL PIN RESET";
        case ESP_RST_SW:
            return "SOFTWARE RESET";
        case ESP_RST_PANIC:
            return "PANIC RESET";
        case ESP_RST_INT_WDT:
            return "INTERRUPT WATCHDOG RESET";
        case ESP_RST_TASK_WDT:
            return "TASK WATCHDOG RESET";
        case ESP_RST_WDT:
            return "OTHER WATCHDOG RESET";
        case ESP_RST_DEEPSLEEP:
            return "DEEP SLEEP EXIT RESET";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT RESET";
        case ESP_RST_SDIO:
            return "SDIO RESET";
        default:
            return "UNKNOWN RESET REASON";
    }
}

// Paddle collision logic

// Function to get a random active brick
bool getRandomActiveBrick(int &row, int &col) {
    int activeBricks = 0;

    // Count the number of active bricks
    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            if (CurrentLevel.bricks[r][c] > 0) {
                activeBricks++;
            }
        }
    }

    if (activeBricks == 0) {
        return false; // No active bricks
    }

    // Select a random active brick
    int randomIndex = getRandomInt(0, activeBricks - 1);
    int count = 0;

    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            if (CurrentLevel.bricks[r][c] > 0) {
                if (count == randomIndex) {
                    row = r;
                    col = c;
                    return true;
                }
                count++;
            }
        }
    }

    return false; // Should never reach here
}

int getLowestActiveBrickY() {
    // Iterate through rows from the bottom upwards
    for (int r = CurrentLevel.brickRows - 1; r >= 0; r--) {
        // Check if any brick in this row is active
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            if (CurrentLevel.bricks[r][c] > 0) {
                // Calculate the y level for this row
                int by = CurrentLevel.brickOffsetY + r * (CurrentLevel.brickHeight + CurrentLevel.brickSpacing);
                return by; // Return the y level of the lowest active row
            }
        }
    }
    // If no active bricks are found, return -1 (or any invalid value)
    return -1;
}


void setup() {
    // Setup pins / serial
    Serial.begin(9600);
    pinMode(BOOT_PIN, INPUT_PULLUP); // Debug : Next Level
    pinMode(TFT_LED, OUTPUT);
    
    tft.begin(); // Display init

    // Query display status
    uint8_t status = tft.readcommand8(ILI9341_RDMODE);
    Serial.print("DISPLAY INIT... STATUS = 0x");
    Serial.println(status, HEX);

    if (status != DISPLAY_OK) {
        Serial.println("DISPLAY INITIALIZATION FAIL, ABORTING...");
        delay(3000);
        esp_system_abort("DISPLAY INIT FAIL");
    }

    screen_init = true;
    tft.fillScreen(ILI9341_BLACK);
    char msg[40];
    sprintf(msg, "DISPLAY INITIALIZATION OK (0x%X)...", status);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    delay(200);
    digitalWrite(TFT_LED, HIGH);

    sprintf(msg, "BACKLIGHT ON (D5)...");
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    esp_chip_info_t c_info;
    esp_chip_info(&c_info);
    sprintf(msg, "CPU MODEL %s (%d CORES)...", chip_model_to_string(c_info.model), c_info.cores);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    bool ble = (c_info.features & CHIP_FEATURE_BLE) != 0;
    bool blu = (c_info.features & CHIP_FEATURE_BT) != 0;
    bool w_support = (c_info.features & CHIP_FEATURE_IEEE802154) != 0;
    bool wifi_bgn = (c_info.features & CHIP_FEATURE_WIFI_BGN) != 0;
    bool flash = (c_info.features & CHIP_FEATURE_EMB_FLASH) != 0;
    bool psram = (c_info.features & CHIP_FEATURE_EMB_PSRAM) != 0;
    sprintf(msg, "BLU: %d | BLE : %d | IEEE802154 : %d", blu, ble, w_support);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext("FEATURE FLAGS:");
        drawdebugtext(msg);
    #endif
    sprintf(msg, "W_BGN: %d | FLASH : %d | PSRAM : %d", wifi_bgn, flash, psram);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    esp_reset_reason_t reset_reason = esp_reset_reason();
    sprintf(msg, "RESET REASON: %s", reset_reason_to_string(reset_reason));
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    if (!Serial) {
        Serial.println("SERIAL1 NOT INITIALIZED...");
        #ifdef DEBUG
            drawdebugtext("SERIAL1 NOT INITIALIZED...");
        #endif
        delay(3000);
        esp_system_abort("SERIAL INIT FAIL");
    }

    #ifdef DEBUG
        drawdebugtext("SERIAL1 PORT INITIALIZED...");
    #endif

    sprintf(msg, "FREE HEAP MEMORY: %dB (%dKB)", esp_get_free_heap_size(), esp_get_free_heap_size()/1000);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    sprintf(msg, "FREE STACK MEMORY: %dB (%dKB)", stack_remaining, stack_remaining/1000);
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    sprintf(msg, "ESP IDF %s", esp_get_idf_version());
    Serial.println(msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif  

    drawloadtext();
    loadLevel(currentLevel);

    
    #ifdef DEBUG
        drawdebugtext("LEVEL LOAD OK...");
    #endif

    #ifdef DEBUG
        drawdebugtext("STARTING LEVEL...");
        delay(1000);
    #endif
    resetGame();
    setup_complete = true;    
}

void loop() {
    // get current frame time to normalize to ~60hz
    unsigned long cur_f_time = millis();

    if (digitalRead(BOOT_PIN) == LOW) {
        delay(200); // debounce delay
        while (digitalRead(BOOT_PIN) == LOW); // Wait for button release (freeze)
        nextLevel(); // Move to next level
    }
    
    
    // Paddle logic
    bool hitPaddle = false;
    tft.fillRect(paddleX, paddleY, paddleWidth, paddleHeight, ILI9341_WHITE);

    if (y + radius >= paddleY && y + radius <= paddleY + paddleHeight && x+radius >= paddleX && x <= paddleX + paddleWidth + radius) {
        float hitPos = ((x + radius) - (paddleX + paddleWidth / 2)) / ((paddleWidth / 2) + radius);

        dx = speed * hitPos * BOUNCE_FACTOR; // Angle the bounce
        dy = -sqrt(speed * speed - dx * dx); // Adjust dy to preserve total speed

        hitPaddle = true;

        target_coord = getRandomInt(2*radius, paddleWidth-2*radius); // Where on the paddle to hit next?
    }

    // TODO: Remove
    if (!ballOnPaddle && dy > 0) {
        int padding = radius;
        if(x < paddleX + target_coord - padding) {
            movePaddleDraw(-paddle_speed);
        } else if (x > paddleX + target_coord + padding) {
            movePaddleDraw(paddle_speed);
        } else {
            if (dx > 0)
                movePaddleDraw(min(dx, paddle_speed));
            else
                movePaddleDraw(max(dx, -paddle_speed));
        }
    }
    
    // Ball logic

    if (ballOnPaddle) {
        x = paddleX + paddleWidth/2;
        y = paddleY - paddleHeight - radius-1;
        
        launchAngle = getRandomInt(30, 150);

        while (launchAngle > 80 && launchAngle < 100) {
            launchAngle = getRandomInt(30, 150);
        }

        drawLaunchAngleIndicator();

        
        tft.fillCircle(x, y, radius, ILI9341_WHITE);
        
        if (digitalRead(BOOT_PIN) == LOW) {
            launchBall();
        }
        
        // TODO: Remove
        delay(2500);
        launchBall();
    } else {
        unsigned long currentMillis = millis();

        if (currentMillis - lastInterval >= interval) {
            lastInterval = currentMillis; // Reset timer
    
            // Reduce interval by 250ms, but not below 5 seconds
            interval = max(minInterval, interval - DELTA_INTERVAL);
    
            // Perform actions that should happen every interval
            moveBricksDown(BRICK_INCR_AMT);

            int lowestYRow = getLowestActiveBrickY();
            if (lowestYRow >= MIN_BRICK_HEIGHT) { // Did lowest brick reach min height?
                if (lives > 0) { 
                    lives -= 1;
                    nextLevel();
                } else {
                    currentLevel = -1;
                    points = 0;
                    lives = 3;
                    speed = STARTER_SPEED;
    
                    nextLevel();
                }
            }   
        }


        bool game_finished = true;
        float old_x = x, old_y = y;

        x += dx;
        y += dy;

        if (x - radius <= 0 && dx < 0) dx = -dx;
        if (x + radius >= tft.width() && dx > 0) dx = -dx;
        if (y - radius <= 15 && dy < 0) dy = -dy;
        if (old_y - radius <= 15) redraw_header = true; // bouncing off top header may cause ball shadow to erase it
        if (y + radius >= tft.height() && dy > 0 && !hitPaddle) {
            if (lives > 0) {
                lives -= 1;
            } else {
                currentLevel = -1;
                points = 0;
                lives = 3;
                speed = STARTER_SPEED;

                nextLevel();
            }
            drawHeader();

            x = paddleX + paddleWidth/2;
            y = paddleY - paddleHeight - radius-1;

            ballOnPaddle = true;
        }

        if (y + radius >= CurrentLevel.brickOffsetY) {
            for (int r = 0; r < CurrentLevel.brickRows; r++) {
                for (int c = 0; c < CurrentLevel.brickCols; c++) {
                    if (CurrentLevel.bricks[r][c] > 0) {
                        int bx = CurrentLevel.brickOffsetX + c * (CurrentLevel.brickWidth + CurrentLevel.brickSpacing);
                        int by = CurrentLevel.brickOffsetY + r * (CurrentLevel.brickHeight + CurrentLevel.brickSpacing);

                        int ballLeft = x - radius;
                        int ballRight = x + radius;
                        int oldBallLeft = old_x - radius;
                        int oldBallRight = old_x + radius;
                        int ballTop = y - radius;
                        int ballBottom = y + radius;
                        int oldBallTop = old_y - radius;
                        int oldBallBottom = old_y + radius;
                        int brickLeft = bx;
                        int brickRight = bx + CurrentLevel.brickWidth;
                        int brickTop = by;
                        int brickBottom = by + CurrentLevel.brickHeight;

                        bool collisionX = (oldBallRight <= brickLeft && ballRight >= brickLeft) || 
                                        (oldBallLeft >= brickRight && ballLeft <= brickRight);

                        bool collisionY = (oldBallBottom <= brickTop && ballBottom >= brickTop) || 
                                        (oldBallTop >= brickBottom && ballTop <= brickBottom);

                        if ((ballRight > brickLeft && ballLeft < brickRight && 
                            ballBottom > brickTop && ballTop < brickBottom) ||
                            (collisionX && ballBottom > brickTop && ballTop < brickBottom) ||
                            (collisionY && ballRight > brickLeft && ballLeft < brickRight)) {
                            collided_c = c;
                            collided_r = r;  
                            CurrentLevel.bricks[r][c]--;
                            if (CurrentLevel.bricks[r][c] == 0) {
                                game_finished = check_game_finished();
                                tft.fillRect(bx, by, CurrentLevel.brickWidth, CurrentLevel.brickHeight, ILI9341_BLACK);
                                points += 10;
                                drawHeader();
                            } else {
                                game_finished = false;
                                drawBrick(r, c);
                            }
                        
                           int overlapLeft = ballRight - brickLeft;
                            int overlapRight = brickRight - ballLeft;
                            int overlapTop = ballBottom - brickTop;
                            int overlapBottom = brickBottom - ballTop;
                            int minOverlap = min(min(overlapLeft, overlapRight), min(overlapTop, overlapBottom));

                            if (minOverlap == overlapLeft && overlapLeft < overlapTop && overlapLeft < overlapBottom) {
                                dx = -dx; // Bounce horizontally
                            } else if (minOverlap == overlapRight && overlapRight < overlapTop && overlapRight < overlapBottom) {
                                dx = -dx; // Bounce horizontally
                            } else {
                                dy = -dy; // Bounce vertically
                            }

                        } 
                        else if (oldBallRight > brickLeft && oldBallLeft < brickRight && oldBallBottom > brickTop && oldBallTop < brickBottom) {
                            collided_c = c;
                            collided_r = r;   
                        }
                        game_finished = false;
                    }
                }
            }
        } else {
            game_finished = false;
            collided_c = -1;
            collided_r = -1;
        }

        // Draw lose boundary
        tft.drawLine(0, MIN_BRICK_HEIGHT, SCREEN_WIDTH, MIN_BRICK_HEIGHT, ILI9341_RED);

        // Draw ball in new location, erase in old location
        tft.fillCircle(old_x, old_y, radius, ILI9341_BLACK);
        tft.fillCircle(x, y, radius, ILI9341_WHITE);
        
        if (collided_r >= 0 && collided_c >= 0) {
            drawBrick(collided_r, collided_c);
        }
        if (redraw_header) {
            drawHeader();
            redraw_header = false;
        }


        if (game_finished) {
            nextLevel();
        }
    }

    // Enforce ~60hz
    unsigned long f_complete_time = millis() - cur_f_time;
    if (f_complete_time < 16)
        delay(16 - f_complete_time);
}