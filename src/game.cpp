#include <Arduino.h>
#include <cmath>
#include "game.h"
#include "ili9341.h"
#include "paddle.h"
#include "ball.h"
#include "util.h"
#include "debug.h"


game_t game = {
    .current_level = {},  // Assuming LevelInfo has a default constructor or initialization
    .current_level_index = 0,
    .num_levels = sizeof(levels) / sizeof(LevelInfo),
    .points = 0,
    .lives = STARTER_LIVES,
    .redraw_header = false,
    .last_interval = 0,
    .interval = DEFAULT_INTERVAL,
    .min_interval = MIN_INTERVAL,
    .game_started = false,
    .game_finished = false
};

game_t *get_game_info() {
    return &game;
}

void load_level(int levelIndex) {
    drawloadtext();
    delay(200);
    // Load the level from PROGMEM
    memcpy_P(&game.current_level, &levels[levelIndex], sizeof(LevelInfo));

    // Calculate X-offset using parameters
    int numCols = game.current_level.brickCols;
    int numRows = game.current_level.brickRows;
    int height = game.current_level.brickHeight;
    int width = game.current_level.brickWidth;
    int spacing = game.current_level.brickSpacing;
    game.current_level.brickOffsetX = (SCREEN_WIDTH - (numCols * (width + spacing)))/2;
    game.current_level.brickOffsetY = HEADER_HEIGHT + TOP_BUFFER;
}

void resetGame() {
    black_screen();
    draw_header();

    paddle_t *p_info = get_paddle_info();
    p_info->paddle_x = (SCREEN_WIDTH - p_info->paddle_width)/2;

    ball_t *b_info = get_ball_info();
    b_info->x = (p_info->paddle_y + p_info->paddle_width)/2;
    b_info->y = p_info->paddle_y - p_info->paddle_height - b_info->radius - 1;
    b_info->ball_on_paddle = true;
    b_info->launch_angle = getRandomInt(30, 150);

    // Increment speed
    b_info->speed = min(b_info->speed + 0.15, MAX_SPEED);

    // Normalize direction and apply new speed
    float norm = sqrt(b_info->dx * b_info->dx + b_info->dy * b_info->dy);
    b_info->dx = (b_info->dx / norm) * b_info->speed;
    b_info->dy = (b_info->dy / norm) * b_info->speed;
    p_info->paddle_speed = PADDLE_SPEED_DAMPEN(b_info->speed); 

    // Reset brick movement interval
    game.interval = DEFAULT_INTERVAL;

    // Reset collided brick
    b_info->collided_r = -1;
    b_info->collided_c = -1;

    draw_all_bricks();

    draw_loss_boundary();
}


void next_level() {
    black_screen();

    // Load next level from memory
    game.current_level_index = (game.current_level_index + 1);
    load_level(game.current_level_index % game.num_levels);
    Serial.println("NEWLEVEL LOADED FROM PROGMEM...");

    // Reset game params, draw bricks
    resetGame();
    Serial.println("GAME STATE RESET...");

    query_display_status();

    Serial.println("OK!");

    Serial.println("NEWLEVEL BEGIN...");
}


bool check_game_finished() {
    for (int r = 0; r < game.current_level.brickRows; r++) {
        for (int c = 0; c < game.current_level.brickCols; c++) {
            if (game.current_level.bricks[r][c] > 0)
                return false;
        }
    }
    return true;
}

int getLowestActiveBrickY() {
    // Iterate through rows from the bottom upwards
    for (int r = game.current_level.brickRows - 1; r >= 0; r--) {
        // Check if any brick in this row is active
        for (int c = 0; c < game.current_level.brickCols; c++) {
            if (game.current_level.bricks[r][c] > 0) {
                // Calculate the y level for this row
                int by = game.current_level.brickOffsetY + r * (game.current_level.brickHeight + game.current_level.brickSpacing);
                return by; // Return the y level of the lowest active row
            }
        }
    }
    // If no active bricks are found, return -1 (or any invalid value)
    return -1;
}

void start_game() {
    ball_t *b_info = get_ball_info();
    game.current_level_index = -1;
    game.lives = 3;
    b_info->speed = STARTER_SPEED;

    next_level();
}

void game_loop() {
    ball_t *b_info = get_ball_info();
    paddle_t *p_info = get_paddle_info();

    game.game_finished = false;

    if (digitalRead(BOOT_PIN) == LOW) {
        delay(200); // debounce delay
        while (digitalRead(BOOT_PIN) == LOW); // Wait for button release (freeze)
        next_level(); // Move to next level
    }
    
    incr_paddle_auto();
    
    // Ball logic

    if (b_info->ball_on_paddle) {
        launch_ball_auto();
    } else {
        unsigned long currentMillis = millis();

        if (currentMillis - game.last_interval >= game.interval) {
            game.last_interval = currentMillis; // Reset timer
    
            // Reduce interval by 250ms, but not below 5 seconds
            game.interval = max(game.min_interval, game.interval - DELTA_INTERVAL);
    
            // Perform actions that should happen every interval
            move_bricks_down(BRICK_INCR_AMT);

            int lowestYRow = getLowestActiveBrickY();
            if (lowestYRow >= MIN_BRICK_HEIGHT) { // Did lowest brick reach min height?
                if (game.lives > 0) { 
                    game.lives -= 1;
                    next_level();
                } else {
                    game.current_level_index = -1;
                    game.points = 0;
                    game.lives = 3;
                    b_info->speed = STARTER_SPEED;
    
                    next_level();
                }
            }   
        }
        
        float old_x = b_info->x;
        float old_y = b_info->y;

        ball_collision();

        // Draw lose boundary
        draw_loss_boundary();

        // Draw ball in new location, erase in old location
        draw_ball(b_info->x, b_info->y, old_x, old_y, b_info->radius);
        
        if (b_info->collided_r >= 0 && b_info->collided_c >= 0) {
            draw_brick(b_info->collided_r, b_info->collided_c);
        }
        
        if (game.redraw_header) {
            draw_header();
            game.redraw_header = false;
        }

        if (game.game_finished) {
            next_level();
        }
    }
}