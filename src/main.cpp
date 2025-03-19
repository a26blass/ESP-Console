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
extern "C" {
    #include <malloc.h>
}

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

// Game constants
#define MAX_SPEED 3.5
#define MAX_LIVES 6
#define STARTER_LIVES 3

// DEBUG
#define DEBUG

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Ball info
const int radius = 3;
float x = 30, y = 100;
float dx = 2, dy = -2;
int collided_r, collided_c = -1;

// Game info
LevelInfo CurrentLevel;
int currentLevel = 0;
int numLevels = sizeof(levels) / sizeof(LevelInfo);
int points = 0;
int lives = STARTER_LIVES;
bool redraw_header = false;

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

int freeMemory() {
    struct mallinfo mi = mallinfo();
    return mi.fordblks;
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

    sprintf(line, "PANIC: KERNEL PANIC, ABORTING...");
    drawdebugtext(line);

    sprintf(line, "PC (Program Counter): 0x%08X", exc_frame->pc);
    drawdebugtext(line);

    sprintf(line, "LR (Link Register): 0x%08X", exc_frame->a0);
    drawdebugtext(line);

    sprintf(line, "SP (Stack Pointer): 0x%08X", exc_frame->a1);
    drawdebugtext(line);

    sprintf(line, "EXCCAUSE (Exception Cause): %d", exc_frame->exccause);
    drawdebugtext(line);

    sprintf(line, "EXCVADDR (Fault Address): 0x%08X", exc_frame->excvaddr);
    drawdebugtext(line);
}

// Panic Handler
extern "C" void __wrap_esp_panic_handler(void *frame) {
    if (setup_complete) {
        tft.fillScreen(ILI9341_BLUE);
        dbgline = 10;
    }

    // Cast frame to a CPU context structure
    XtExcFrame *exc_frame = (XtExcFrame *)frame;

    // Print a panic message
    ESP_LOGE("PANIC", "KERNEL PANIC, ABORTING...");

    // Extract and print CPU register values
    ESP_LOGE("PANIC", "PC (Program Counter): 0x%08X", exc_frame->pc);
    ESP_LOGE("PANIC", "LR (Link Register): 0x%08X", exc_frame->a0);
    ESP_LOGE("PANIC", "SP (Stack Pointer): 0x%08X", exc_frame->a1);
    ESP_LOGE("PANIC", "EXCCAUSE (Exception Cause): %d", exc_frame->exccause);
    ESP_LOGE("PANIC", "EXCVADDR (Faulting Address): 0x%08X", exc_frame->excvaddr);

    // Print a warning message
    if (screen_init)
        display_panic_message(exc_frame);
        
    Serial.println("KERNEL PANIC");

    // Optional: Trigger watchdog to reset
    esp_task_wdt_init(1, true);
    esp_task_wdt_add(NULL);
    
    // Wait for logs to be flushed before restarting
    vTaskDelay(pdMS_TO_TICKS(5000));  // Give 5 seconds for logs to be read

    #ifndef DEBUG
        esp_restart();
    #endif
}

int getRandomInt(int min, int max) {
    return min + (esp_random() % (max - min + 1));
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


void drawBrick(int row, int col) {
    int durability = CurrentLevel.bricks[row][col];
    if (durability > 0) {
        int bx = CurrentLevel.brickOffsetX + col * (CurrentLevel.brickWidth + CurrentLevel.brickSpacing);
        int by = CurrentLevel.brickOffsetY + row * (CurrentLevel.brickHeight + CurrentLevel.brickSpacing);
        tft.fillRect(bx, by, CurrentLevel.brickWidth, CurrentLevel.brickHeight, getBrickColor(durability));
    }
}

void resetGame() {
    tft.fillScreen(ILI9341_BLACK);
    drawHeader();
    x = getRandomInt(20, 220);
    y = 300;
    for (int r = 0; r < CurrentLevel.brickRows; r++) {
        for (int c = 0; c < CurrentLevel.brickCols; c++) {
            drawBrick(r, c);
        }
    }
}

void nextLevel() {
    tft.fillScreen(ILI9341_BLACK);
    // Load next level from memory
    currentLevel = (currentLevel + 1) % numLevels;
    loadLevel(currentLevel);
    Serial.println("NEWLEVEL LOADED FROM PROGMEM...");

    // Reset collided brick
    collided_r = -1;
    collided_c = -1;

    // Reset game params, draw bricks
    resetGame();
    Serial.println("GAME STATE RESET...");

    // Increment dx/dy, bounded by MAX_SPEED
    dx = min(abs(dx)+0.15, MAX_SPEED);
    dy = -dx;

    // Query display status
    uint8_t status = tft.readcommand8(ILI9341_RDMODE);
    Serial.print("DISPLAY CHECK... STATUS = 0x");
    Serial.println(status, HEX);

    if (status != 0xCA) {
        Serial.println("DISPLAY CHECK FAIL, ABORTING...");
        esp_system_abort("DISPLAY CHECK FAIL");
    }

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

    if (status != 0xCA) {
        Serial.println("DISPLAY INITIALIZATION FAIL, ABORTING...");
        delay(3000);
        esp_system_abort("DISPLAY INIT FAIL");
    }

    screen_init = true;
    tft.fillScreen(ILI9341_BLACK);
    digitalWrite(TFT_LED, HIGH);

    char msg[40];
    sprintf(msg, "DISPLAY INITIALIZATION OK (0x%X)...", status);
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
        delay(2000);
    #endif
    resetGame();
    setup_complete = true;
}

void loop() {
    if (digitalRead(BOOT_PIN) == LOW) {
        delay(200); // debounce delay
        while (digitalRead(BOOT_PIN) == LOW); // Wait for button release (freeze)
        nextLevel(); // Move to next level
    }

    float old_x = x, old_y = y;
    bool game_finished = true;
    x += dx;
    y += dy;

    if (x - radius <= 0 && dx < 0) dx = -dx;
    if (x + radius >= tft.width() && dx > 0) dx = -dx;
    if (y - radius <= 15 && dy < 0) dy = -dy;
    if (old_y - radius <= 15) redraw_header = true; // bouncing off top header may cause ball shadow to erase it
    if (y + radius >= tft.height() && dy > 0) dy = -dy;

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

                        if (minOverlap == overlapLeft || minOverlap == overlapRight) {
                            dx = -dx;
                        } else {
                            dy = -dy;
                        }
                        break;
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

    delay(10);
}