#include <Arduino.h>
#include <cmath>
#include "game.h"
#include "display.h"
#include "paddle.h"
#include "ball.h"
#include "inputs.h"
#include "util.h"
#include "debug.h"
#include "system.h"

// GLOBALS
game_t game = {
    .current_level = {},  // Assuming LevelInfo has a default constructor or initialization
    .current_level_index = 0,
    .num_levels = sizeof(levels) / sizeof(LevelInfo),
    .points = 0,
    .lives = STARTER_LIVES,
    .max_score = 0,
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

void load_level(int levelIndex, bool use_load_screen) {
    if(use_load_screen)
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

void resetGame(bool use_load_screen) {
    if (use_load_screen)
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


void next_level(bool use_load_screen) {
    if (use_load_screen)
        black_screen();

    // Load next level from memory
    game.current_level_index = (game.current_level_index + 1);
    load_level(game.current_level_index % game.num_levels, false);
    Serial.println("NEWLEVEL LOADED FROM PROGMEM...");

    // Reset game params, draw bricks
    resetGame(use_load_screen);
    Serial.println("GAME STATE RESET...");

    query_display_status();

    Serial.println("OK!");

    // Draw 'Loading...' if game not started
    if (!game.game_started)
        draw_start_text();

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
    game.points = 0;
    game.max_score = get_hiscore();
    b_info->speed = STARTER_SPEED;

    // Black screen before game
    black_screen();

    uint32_t cur_time = millis();
    

    while(millis() - cur_time < 5000 && digitalRead(BOOT_PIN) == HIGH) {
        if (get_a_pressed() || game.game_started) {
            black_screen();
            game.game_started = true;
            break;
        }

        draw_start_text();
    }

    next_level(false);
}

void end_game_and_restart(bool is_ai_game) {
    ball_t *b_info = get_ball_info();

    if (!is_ai_game) {
        draw_leaderboard(game.points, game.max_score);

        delay(3000);

        if (game.points > game.max_score) {
            set_hiscore(game.points);
            game.max_score = game.points;
        }
    }

    game.current_level_index = -1;
    game.points = 0;
    game.lives = 3;
    b_info->speed = STARTER_SPEED;


    next_level(true); 
}

void handle_pause_input(int selected_option) {
    switch (selected_option) {
        case 0: { // Brightness adjustment
            static int brightness_level = 3; // 0 = Low, 1 = Medium, 2 = High
            int brightness_values[] = { 25, 50, 150, 255 }; // Define brightness levels
            
            brightness_level = (brightness_level + 1) % 4; // Cycle through levels
            set_brightness(brightness_values[brightness_level]);
            break;
        }
        case 1: { // Toggle LED brightness
            static int led_level = 3; // 0 = off, 1 = dim, 2 = full
            led_level = (led_level + 1) % 4;
            led_brightness(led_level);
            break;
        }
        case 2: // Reset game
            end_game_and_restart(true);
            break;
        case 3: // Restart ESP32
            ESP.restart();
            break;
    }
}

void pause_menu_logic() {
    drawpausescreen(0);
    int selected_opt = 0;
    int cycles = 0;
    while (!get_start_pressed() && !get_b_pressed()) {
        cycles++;

        if (get_up_pressed()) {
            if (--selected_opt < 0) {
                selected_opt = 3;
            }
            drawpausescreen(selected_opt);
        } else if (get_down_pressed()) {
            selected_opt++;
            selected_opt %= 4;
            drawpausescreen(selected_opt);
        }  else if (get_a_pressed()) {
            handle_pause_input(selected_opt);

            if (selected_opt > 1)
                return;
        }

        if (cycles > BATTERY_CHECK_INTERVAL) {
            draw_batt_volts();
            cycles = 0;
        }

        delay(1);
    }
    black_screen();
    draw_all_bricks();
    draw_header();
    draw_loss_boundary();
}


void game_cycle() {
    ball_t *b_info = get_ball_info();
    paddle_t *p_info = get_paddle_info();

    game.game_finished = false;
    
    // Paddle logic / starting game
    if(!game.game_started) {
        if (get_a_pressed()) {
            game.game_started = true;
            start_game();
        } else {
            incr_paddle_auto();
        }
    } else if (!b_info->ball_on_paddle) {
        if (get_left_pressed()) {
            movePaddleDraw(-p_info->paddle_speed);
        }
        else if (get_right_pressed()) {
            movePaddleDraw(p_info->paddle_speed);
        }
        
        draw_paddle();
        handle_collision(); // Paddle-ball collision
    } else { 
        draw_paddle();
    }
    
    // Ball logic

    if (b_info->ball_on_paddle) {
        if(!game.game_started)
            launch_ball_auto();
        else {
            center_ball_on_paddle();
            if (get_a_pressed())
                launch_ball();
            if (get_left_pressed()) {
                increaseLaunchAngle();
            }
            if (get_right_pressed()) {
                decreaseLaunchAngle();
            }

            draw_launch_angle_indicator();
        }
    } else {
        unsigned long currentMillis = millis();

        if (currentMillis - game.last_interval >= game.interval) {
            game.last_interval = currentMillis; // Reset timer
    
            // Reduce interval by 250ms, but not below 3 seconds
            game.interval = max(game.min_interval, game.interval - DELTA_INTERVAL);
    
            // Perform actions that should happen every interval
            move_bricks_down(BRICK_INCR_AMT);

            int lowestYRow = getLowestActiveBrickY();
            if (lowestYRow + game.current_level.brickHeight >= MIN_BRICK_HEIGHT) { // Did lowest brick reach min height?
                if (game.lives > 0) { 
                    game.lives -= 1;
                    next_level(true);
                } else {
                    end_game_and_restart(!game.game_started);
                }
                return;
            }   
        }
        
        float old_x = b_info->x;
        float old_y = b_info->y;

        ball_collision();

        // Draw lose boundary
        draw_loss_boundary();

        // Draw ball in new location, erase in old location
        draw_ball(b_info->x, b_info->y, old_x, old_y, b_info->radius);

        draw_all_bricks();
        
        if (game.redraw_header) {
            draw_header();
            game.redraw_header = false;
        }

        if (game.game_finished) {
            next_level(true);
        } else if (!game.game_started) {
            draw_start_text();
        } 
    }
    if (get_start_pressed()) {
        pause_menu_logic();
    }
}