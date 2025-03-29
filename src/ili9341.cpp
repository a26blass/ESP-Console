#include <Arduino.h>
#include "ili9341.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "paddle.h"
#include "ball.h"
#include "game.h"
#include "debug.h"


// Globals
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas1 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);
ili_9341_t dbginfo;

// --- DEBUG ---
uint8_t get_display_status() {
    // Query display status
    uint8_t status = tft.readcommand8(ILI9341_RDMODE);
    return status;
}

void set_dbg_line (int l) {
    dbginfo.dbg_line = l;
}

bool get_screen_init() {
    return dbginfo.screen_init;
}

void drawdebugtext(const char* text) {
    tft.setTextSize(1);
    tft.setCursor(10, dbginfo.dbg_line);
    dbginfo.dbg_line += 10;
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
    tft.fillScreen(ILI9341_BLUE);
    dbginfo.dbg_line = 10;

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


// --- PADDLE ---
// Update paddle position (to be called in loop)
void movePaddleDraw(float direction) {
    paddle_t *p_info = get_paddle_info();

    // Save the old paddle position
    float oldPaddleX = p_info->paddle_x;

    // Update the paddle position
    p_info->paddle_x += direction; // Move left or right
    p_info->paddle_x = max(0.0f, min((float)(SCREEN_WIDTH - p_info->paddle_width), p_info->paddle_x)); // Keep in bounds

    // Only redraw if the paddle has actually moved
    if (oldPaddleX != p_info->paddle_x) {
        // Overdraw the old paddle with a black box
        if (p_info->paddle_x > oldPaddleX)
            tft.fillRect(floor(oldPaddleX), p_info->paddle_y, ceil(p_info->paddle_x - oldPaddleX), p_info->paddle_height, ILI9341_BLACK);
        else 
            tft.fillRect(floor(p_info->paddle_x+p_info->paddle_width), p_info->paddle_y, ceil(oldPaddleX - p_info->paddle_x), p_info->paddle_height, ILI9341_BLACK);

        // Draw the new paddle at the updated position
        tft.fillRect(p_info->paddle_x, p_info->paddle_y, p_info->paddle_width, p_info->paddle_height, ILI9341_WHITE);
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

void draw_paddle() {
    paddle_t *p_info = get_paddle_info();
    tft.fillRect(p_info->paddle_x, p_info->paddle_y, p_info->paddle_width, p_info->paddle_height, ILI9341_WHITE);
}

// --- UI ---
void black_screen() {
    tft.fillScreen(ILI9341_BLACK);
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

void draw_start_text() {
    tft.setTextColor(ILI9341_WHITE); // Set text color to white
    tft.setTextSize(2); // Text size 2

    int charWidth = 6 * 2; // Each character is 12 pixels wide
    int charHeight = 8 * 2; // Each character is 16 pixels tall
    int textLength = 11; // "LOADING..." has 10 characters
    int textWidth = textLength * charWidth;
    int textHeight = charHeight;
    
    int x = (240 - textWidth) / 2;
    int y = (320 - textHeight) / 2;

    tft.setCursor(x, y);
    tft.print("PRESS START");
}

void draw_header() {
    game_t *g_info = get_game_info();

    tft.fillRect(0, 0, tft.width(), HEADER_HEIGHT, ILI9341_BLUE); // Draw header background
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);

    // Draw Level
    tft.setCursor(5, 5);
    tft.print("Level: ");
    tft.print(g_info->current_level_index + 1);

    // Draw Points
    tft.setCursor(80, 5);
    tft.print("Points: ");
    tft.print(g_info->points);

    // Draw Lives as balls
    int livesX = 180; // Starting X position for lives
    int livesY = 4;   // Y position for lives
    int ballRadius = 3; // Ball size

    for (int i = 0; i < MAX_LIVES; i++) {
        if (i < g_info->lives) {
            if (i < STARTER_LIVES)
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_WHITE); // Filled for active life
            else 
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_GREEN); // Filled for active life
        } else if (i < STARTER_LIVES) {
            tft.drawCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ILI9341_DARKGREY); // Outline for lost life
        } 
    }
}

void draw_launch_angle_indicator() {
    ball_t *b_info = get_ball_info();

    int indicatorLength = 20; // Length of the indicator
    
    // Convert angle to radians
    double angleRad = b_info->launch_angle * M_PI / 180.0;
    
    // Ball position (assuming it's at the center of the paddle)
    int ballX = b_info->x;
    int ballY = b_info->y;

    // Calculate the new end point of the indicator
    int endX = ballX + indicatorLength * cos(angleRad);
    int endY = ballY - indicatorLength * sin(angleRad);

    // Erase previous line if it exists
    if (b_info->prev_end_x != -1 && b_info->prev_end_y != -1 && b_info->prev_end_x != endX && b_info->prev_end_y != endY) {
        tft.drawLine(ballX, ballY, b_info->prev_end_x, b_info->prev_end_y, ILI9341_BLACK);
    }

    // Draw new indicator line
    tft.drawLine(ballX, ballY, endX, endY, ILI9341_WHITE);

    // Store new end position
    b_info->prev_end_x = endX;
    b_info->prev_end_y = endY;
}

void increaseLaunchAngle() {
    ball_t *b_info = get_ball_info();

    if (b_info->launch_angle < 150) {
        b_info->launch_angle += 5;
        draw_launch_angle_indicator();
    }
}

void decreaseLaunchAngle() {
    ball_t *b_info = get_ball_info();
    if (b_info->launch_angle > 30) {
        b_info->launch_angle -= 5;
        draw_launch_angle_indicator();
    }
}

// --- BALL ---
void draw_ball(int x, int y, int old_x, int old_y, int radius) {

    if (x != old_x || y != old_y)
        tft.fillCircle(old_x, old_y, radius, ILI9341_BLACK);
    tft.fillCircle(x, y, radius, ILI9341_WHITE);
}

// --- BRICKS ---
uint16_t getBrickColor(int durability) {
    switch (durability) {
        case 3: return ILI9341_GREEN;
        case 2: return ILI9341_ORANGE;
        case 1: return ILI9341_RED;
        default: return ILI9341_BLACK;
    }
}

void draw_brick(int row, int col, bool overridecol, uint16_t color) {
    game_t *g_info = get_game_info();

    int durability = g_info->current_level.bricks[row][col];
    int bx = g_info->current_level.brickOffsetX + col * (g_info->current_level.brickWidth + g_info->current_level.brickSpacing);
    int by = g_info->current_level.brickOffsetY + row * (g_info->current_level.brickHeight + g_info->current_level.brickSpacing);

    if (!overridecol)
        tft.fillRect(bx, by, g_info->current_level.brickWidth, g_info->current_level.brickHeight, getBrickColor(durability));
    else
        tft.fillRect(bx, by, g_info->current_level.brickWidth, g_info->current_level.brickHeight, color);
    
}

void clearBricks() {
    game_t *g_info = get_game_info();
    for (int r = 0; r < g_info->current_level.brickRows; r++) {
        for (int c = 0; c < g_info->current_level.brickCols; c++) {
            draw_brick(r, c, true);
        }
    }
}

void draw_all_bricks() {
    game_t *g_info = get_game_info();
    for (int r = 0; r < g_info->current_level.brickRows; r++) {
        for (int c = 0; c < g_info->current_level.brickCols; c++) {
            draw_brick(r, c);
        }
    }
}

void draw_loss_boundary() {
    // Draw lose boundary
    tft.drawLine(0, MIN_BRICK_HEIGHT, SCREEN_WIDTH, MIN_BRICK_HEIGHT, ILI9341_RED);
}

void move_bricks_down(int amount) {
    game_t *g_info = get_game_info();

    clearBricks();
    g_info->current_level.brickOffsetY += BRICK_INCR_AMT;
    draw_all_bricks();
}

// -- POWERUPS ---
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

// --- INIT ---
void display_init() {
    dbginfo.dbg_line = 10;
    
    char msg[40];

    // Setup Debug LED
    pinMode(LED_PIN, OUTPUT);

    uint8_t status = get_display_status();
    if (status != DISPLAY_OK) {
        ESP_LOGE("BOOT FAIL", "DISPLAY INITIALIZATION FAIL, ABORTING...");
        esp_system_abort("DISPLAY INIT FAIL");
    }

    sprintf(msg, "DISPLAY INITIALIZATION OK (0x%X)...", status);
    ESP_LOGI("BOOT", msg);
    #ifdef DEBUG
        drawdebugtext(msg);
    #endif

    pinMode(TFT_LED, OUTPUT);
    tft.begin(); // Display init
    tft.fillScreen(ILI9341_BLACK);

    delay(200);
    digitalWrite(TFT_LED, HIGH);
}