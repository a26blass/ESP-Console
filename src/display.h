#include "xtensa/xtensa_context.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ST7789.h"

// Display pins
#define TFT_CS   22
#define TFT_DC   21
#define TFT_LED   4
#define TFT_RST  33

// Screen constants
#define SCREEN_WIDTH 240 
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 15
#define TOP_BUFFER 15
#define DISPLAY_OK status & (1 << 10)
#define SPI_SPEED 24000000 

#ifndef DISPLAY_H
#define DISPLAY_H
// Structs
typedef struct ili_9341 {
    int dbg_line;
    bool screen_init;
} ili_9341_t;

// Function declarations
uint8_t get_display_status();
void set_dbg_line(int l);
bool get_screen_init();
void drawdebugtext(const char* text);
void draw_panic_log(const char *text);
void display_panic_message(const XtExcFrame *exc_frame);
void black_screen();
void draw_leaderboard(int score, int max_score);
void draw_header();
void draw_lowbatt_symbol();
void draw_ball(int x, int y, int old_x, int old_y, int radius);
void draw_brick(int row, int col, bool overridecol = false, uint16_t color = ~ST77XX_BLACK);
void move_bricks_down(int amount);
void draw_all_bricks();
void draw_paddle();
void increaseLaunchAngle();
void decreaseLaunchAngle();
void draw_launch_angle_indicator(uint16_t color = ~ST77XX_WHITE);
void draw_loss_boundary();
void drawloadtext();
void draw_start_text();
void movePaddleDraw(float direction);
void movePaddle(float direction);
void display_init();


#endif