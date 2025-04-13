#include <Arduino.h>
#include "display.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ST7789.h"
#include "paddle.h"
#include "ball.h"
#include "game.h"
#include "debug.h"
#include "esp_log.h"
#include "system.h"



// Globals
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas1 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);
ili_9341_t dbginfo;

// --- DEBUG ---
uint8_t get_display_status() {
    // Query display status
    uint8_t status = tft.readcommand8(0x09);
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
    tft.fillScreen(~ST77XX_BLUE);
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

// --- BATTERY ---
void draw_lowbatt_symbol() {
    black_screen();
    
    // Battery outline dimensions
    int batteryWidth = 60;
    int batteryHeight = 30;
    int batteryX = (SCREEN_WIDTH - batteryWidth) / 2;
    int batteryY = (SCREEN_HEIGHT - batteryHeight) / 2;

    // Draw outer rectangle (white outline)
    tft.fillRect(batteryX, batteryY, batteryWidth, batteryHeight, ~ST77XX_WHITE);

    // Draw inner rectangle (black fill)
    int innerBatteryWidth = batteryWidth - 10;
    int innerBatteryHeight = batteryHeight - 10;
    int innerBatteryX = batteryX + 5;
    int innerBatteryY = batteryY + 5;
    tft.fillRect(innerBatteryX, innerBatteryY, innerBatteryWidth, innerBatteryHeight, ~ST77XX_BLACK);

    // Draw tiny white rectangle on the right (the + terminal)
    int terminalWidth = 5;
    int terminalHeight = batteryHeight / 2;
    int terminalX = batteryX + batteryWidth;
    int terminalY = batteryY + (batteryHeight - terminalHeight) / 2;
    tft.fillRect(terminalX, terminalY, terminalWidth, terminalHeight, ~ST77XX_WHITE);
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
            tft.fillRect(floor(oldPaddleX), p_info->paddle_y, ceil(p_info->paddle_x - oldPaddleX), p_info->paddle_height, ~ST77XX_BLACK);
        else 
            tft.fillRect(floor(p_info->paddle_x+p_info->paddle_width), p_info->paddle_y, ceil(oldPaddleX - p_info->paddle_x), p_info->paddle_height, ~ST77XX_BLACK);

        // Draw the new paddle at the updated position
        tft.fillRect(p_info->paddle_x, p_info->paddle_y, p_info->paddle_width, p_info->paddle_height, ~ST77XX_WHITE);
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
    tft.fillRect(p_info->paddle_x, p_info->paddle_y, p_info->paddle_width, p_info->paddle_height, ~ST77XX_WHITE);
}

// --- UI ---
void draw_rect(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}


void black_screen() {
    tft.fillScreen(~ST77XX_BLACK);
}

void draw_leaderboard(int score, int max_score) {
    black_screen();

    tft.setTextColor(~ST77XX_WHITE); // Set text color to white
    tft.setTextSize(2); // Text size 2

    int charWidth = 6 * 2; // Each character is 12 pixels wide
    int charHeight = 8 * 2; // Each character is 16 pixels tall
    int textLength = 10; // "LOADING..." has 10 characters
    int textWidth = textLength * charWidth;
    int textHeight = charHeight;
    
    int x = (240 - textWidth) / 2;
    int y = (320 - textHeight) / 2;

    tft.setCursor(x, y);
    tft.print("GAME OVER!");

    tft.setTextSize(1);
    y += charHeight * 3;
    charWidth = 6;

    char score_str[25];

    sprintf(score_str, "SCORE: %d", score);

    textLength = strlen(score_str);
    textWidth = textLength * charWidth;
    x =  (240 - textWidth) / 2; 
    tft.setCursor(x, y);

    tft.printf(score_str);
    y += 8;

    sprintf(score_str, "HIGH SCORE: %d", max_score);

    textLength = strlen(score_str);
    textWidth = textLength * charWidth;
    x =  (240 - textWidth) / 2; 
    tft.setCursor(x, y);

    tft.printf(score_str);
}


void draw_batt_volts() {
    tft.setTextSize(1);
    char batt_buf[10];
    snprintf(batt_buf, sizeof(batt_buf), "%.2fV", battery_volts);
    int batt_text_width = strlen(batt_buf) * 6; // 6 pixels per char at size 1

    // Clear background behind old voltage
    int batt_x = 240 - batt_text_width - 2;
    int batt_y = 2;
    tft.fillRect(batt_x, batt_y, batt_text_width + 4, 8, ~ST77XX_BLACK); // 6*chars wide, 8 pixels tall

    // Draw updated voltage
    tft.setCursor(batt_x, batt_y);
    tft.setTextColor(~ST77XX_WHITE);
    tft.print(batt_buf);
}


void drawpausescreen(int selected_option) {
    black_screen();
    tft.setTextSize(2);

    // Centered "PAUSED..." text
    const char* paused_text = "PAUSED...";
    int charWidth = 6 * 2;
    int charHeight = 8 * 2;
    int textWidth = strlen(paused_text) * charWidth;
    int paused_x = (240 - textWidth) / 2;
    int paused_y = 60;

    tft.setTextColor(~ST77XX_WHITE);
    tft.setCursor(paused_x, paused_y);
    tft.printf(paused_text);

    draw_batt_volts();

    // Restore text size for menu
    tft.setTextSize(2);

    const char* options[] = { "Brightness", "LED", "Reset Game", "Restart Console" };
    int box_width = 180;
    int box_height = 40;
    int box_x = (240 - box_width) / 2;
    int first_box_y = 120;
    int spacing = 10;

    for (int i = 0; i < 4; i++) {
        int box_y = first_box_y + i * (box_height + spacing);
        if (i == selected_option) {
            tft.fillRect(box_x, box_y, box_width, box_height, ~ST77XX_BLUE);
            tft.setTextColor(~ST77XX_WHITE);
        } else {
            tft.fillRect(box_x, box_y, box_width, box_height, ~ST77XX_BLACK);
            tft.drawRect(box_x, box_y, box_width, box_height, ~ST77XX_WHITE);
            tft.setTextColor(~ST77XX_WHITE);
        }

        int text_x = box_x + (box_width - strlen(options[i]) * charWidth) / 2;
        int text_y = box_y + (box_height - charHeight) / 2;
        tft.setCursor(text_x, text_y);
        tft.print(options[i]);
    }
}



void drawloadtext() {
    tft.setTextColor(~ST77XX_WHITE); // Set text color to white
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
    tft.setTextColor(~ST77XX_WHITE); // Set text color to white
    tft.setTextSize(2); // Text size 2

    int charWidth = 6 * 2; // Each character is 12 pixels wide
    int charHeight = 8 * 2; // Each character is 16 pixels tall
    int textLength = 7; // "LOADING..." has 10 characters
    int textWidth = textLength * charWidth;
    int textHeight = charHeight;
    
    int x = (240 - textWidth) / 2;
    int y = (320 - textHeight) / 2;

    tft.setCursor(x, y);
    tft.print("PRESS A");
}

void draw_header() {
    game_t *g_info = get_game_info();

    tft.fillRect(0, 0, tft.width(), HEADER_HEIGHT, ~ST77XX_BLUE); // Draw header background
    tft.setTextColor(~ST77XX_WHITE);
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
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ~ST77XX_WHITE); // Filled for active life
            else 
                tft.fillCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ~ST77XX_GREEN); // Filled for active life
        } else if (i < STARTER_LIVES) {
            tft.drawCircle(livesX + (i * 10), livesY + ballRadius, ballRadius, ~0x5A5A); // Outline for lost life
        } 
    }
}

void draw_launch_angle_indicator(uint16_t color) {
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
    if (b_info->prev_end_x != -1 && b_info->prev_end_y != -1 && (b_info->prev_end_x != endX || b_info->prev_end_y != endY)) {
        tft.drawLine(ballX, ballY, b_info->prev_end_x, b_info->prev_end_y, ~ST77XX_BLACK);
    }

    // Draw new indicator line
    tft.drawLine(ballX, ballY, endX, endY, ~ST77XX_WHITE);

    // Store new end position
    b_info->prev_end_x = endX;
    b_info->prev_end_y = endY;
}

void increaseLaunchAngle() {
    //draw_launch_angle_indicator(~ST77XX_BLACK);
    ball_t *b_info = get_ball_info();

    if (b_info->launch_angle < 150) {
        b_info->launch_angle += 5;
        draw_launch_angle_indicator();
    }
}

void decreaseLaunchAngle() {
    //draw_launch_angle_indicator(~ST77XX_BLACK);
    ball_t *b_info = get_ball_info();
    if (b_info->launch_angle > 30) {
        b_info->launch_angle -= 5;
        draw_launch_angle_indicator();
    }
}

// --- BALL ---
void draw_ball(int x, int y, int old_x, int old_y, int radius) {

    if (x != old_x || y != old_y)
        tft.fillCircle(old_x, old_y, radius, ~ST77XX_BLACK);
    tft.fillCircle(x, y, radius, ~ST77XX_WHITE);
}

// --- BRICKS ---
uint16_t getBrickColor(int durability) {
    switch (durability) {
        case 3: return ~ST77XX_GREEN;
        case 2: return ~ST77XX_ORANGE;
        case 1: return ~ST77XX_RED;
        default: return ~ST77XX_BLACK;
    }
}

void draw_brick(int row, int col, bool overridecol, uint16_t color) {
    game_t *g_info = get_game_info();

    int durability = g_info->current_level.bricks[row][col];
    int bx = g_info->current_level.brickOffsetX + col * (g_info->current_level.brickWidth + g_info->current_level.brickSpacing);
    int by = g_info->current_level.brickOffsetY + row * (g_info->current_level.brickHeight + g_info->current_level.brickSpacing);

    if (!overridecol && durability > 0)
        tft.fillRect(bx, by, g_info->current_level.brickWidth, g_info->current_level.brickHeight, getBrickColor(durability));
    else if (overridecol)
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
    tft.drawLine(0, MIN_BRICK_HEIGHT, SCREEN_WIDTH, MIN_BRICK_HEIGHT, ~ST77XX_RED);
}

void move_bricks_down(int amount) {
    game_t *g_info = get_game_info();

    clearBricks();
    g_info->current_level.brickOffsetY += BRICK_INCR_AMT;
    draw_all_bricks();
}

// -- POWERUPS ---
void drawMissile(int x, int y) {
    tft.fillTriangle(x, y, x - 3, y + 8, x + 3, y + 8, ~ST77XX_RED); // Rocket tip
    tft.fillRect(x - 2, y + 8, 4, 10, ~ST77XX_WHITE); // Rocket body
    tft.fillTriangle(x - 3, y + 18, x + 3, y + 18, x, y + 22, ~ST77XX_ORANGE); // Rocket flames
}

void drawExtraBalls(int x, int y) {
    tft.fillCircle(x - 4, y, 2, ~ST77XX_CYAN);
    tft.fillCircle(x,     y, 2, ~ST77XX_CYAN);
    tft.fillCircle(x + 4, y, 2, ~ST77XX_CYAN);
}

void drawLargeBall(int x, int y) {
    tft.fillCircle(x, y, 4, ~ST77XX_YELLOW); // 8px diameter fits in 10x10
}

void drawLaser(int x, int y) {
    tft.fillRoundRect(x - 4, y - 2, 8, 4, 2, ~ST77XX_RED); // fits inside 10x10 centered
}

void drawPlusOne(int x, int y) {
    tft.setTextSize(1);
    tft.setTextColor(~ST77XX_GREEN);
    tft.setCursor(x - 3, y - 4); // Center 6x8 character in 10x10
    tft.print("+");
}


// --- BRIGHTNESS ---
void set_brightness(uint32_t duty) {
    ledcWrite(PWM_CHANNEL, duty);
}

// --- INIT ---
void display_init() {
    dbginfo.dbg_line = 10;
    
    char msg[40];

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

    tft.init(240, 320); // Display init
    tft.setRotation(2);
    tft.fillScreen(~ST77XX_BLACK);

    Serial.println(tft.width());
    Serial.println(tft.height());

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(TFT_LED, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 255);

    dbginfo.screen_init = true;
}