#include <cmath>
#include "ball.h"
#include "paddle.h"
#include "display.h"
#include "game.h"
#include "util.h"
#include "inputs.h"

ball_t ball = {
    .radius = 3,
    .x = 30, .y = 100,
    .dx = 2, .dy = -2,
    .speed = STARTER_SPEED,
    .ball_on_paddle = true,
    .hit_paddle = false,
    .launch_angle = 150,
    .prev_end_x = -1,
    .prev_end_y = -1,
    .collided_r = -1, .collided_c = -1
};

// Get ball struct
ball_t *get_ball_info() {
    return &ball;
}

void launch_ball() {
    game_t *game_info = get_game_info();

    ball.dx = ball.speed * cos(ball.launch_angle * M_PI / 180.0);
    ball.dy = -ball.speed * sin(ball.launch_angle * M_PI / 180.0);
    ball.ball_on_paddle = false;

    game_info -> last_interval = millis();
}

void launch_ball_auto() {
    paddle_t *p_info = get_paddle_info();
    game_t *game_info = get_game_info();

    ball.x = p_info->paddle_x + p_info->paddle_width/2;
    ball.y = p_info->paddle_y - p_info->paddle_height - ball.radius-1;
    
    ball.launch_angle = getRandomInt(30, 150);

    while (ball.launch_angle > 80 && ball.launch_angle < 100) {
        ball.launch_angle = getRandomInt(30, 150);
    }

    draw_launch_angle_indicator();
    draw_ball(ball.x, ball.y, ball.x, ball.y, ball.radius);
    
    for (int i = 0; i < 2500; i++) {
        if (debug_input_check() || get_a_pressed()) {
            game_info->game_started = true;
            start_game();   
            return;
        }
        if (get_start_pressed()) {
            pause_menu_logic();
            break;
        }
        delay(1);
    }

    launch_ball();
}

void center_ball_on_paddle() {
    paddle_t *p_info = get_paddle_info();

    ball.x = p_info->paddle_x + p_info->paddle_width/2;
    ball.y = p_info->paddle_y - p_info->paddle_height - ball.radius-1; 

    draw_ball(ball.x, ball.y, ball.x, ball.y, ball.radius); 
}

void ball_collision() {
    game_t *g_info = get_game_info();
    paddle_t *p_info = get_paddle_info();

    float old_x = ball.x, old_y = ball.y;

    ball.x += ball.dx;
    ball.y += ball.dy;

    if (ball.x - ball.radius <= 0 && ball.dx < 0) ball.dx = -ball.dx;
    if (ball.x + ball.radius >= SCREEN_WIDTH && ball.dx > 0) ball.dx = -ball.dx;
    if (ball.y - ball.radius <= 15 && ball.dy < 0) ball.dy = -ball.dy;
    if (old_y - ball.radius <= 15) g_info->redraw_header = true; // bouncing off top header may cause ball shadow to erase it
    if (ball.y + ball.radius >= SCREEN_HEIGHT && ball.dy > 0 && !ball.hit_paddle) {
        if (g_info->lives > 0) {
            g_info->lives -= 1;
        } else {
            end_game_and_restart(!g_info->game_started); 
        }
        draw_header();

        ball.x = p_info->paddle_x + p_info->paddle_width/2;
        ball.y = p_info->paddle_y - p_info->paddle_height - ball.radius-1;

        ball.ball_on_paddle = true;
    }
    int brickOffsetY = g_info->current_level.brickOffsetY;
    int brickRows = g_info->current_level.brickRows;
    int brickSpacing = g_info->current_level.brickSpacing;
    int brickHeight = g_info->current_level.brickHeight;

    
    for (int r = 0; r < g_info->current_level.brickRows; r++) {
        for (int c = 0; c < g_info->current_level.brickCols; c++) {
            if (g_info->current_level.bricks[r][c] > 0) {
                int bx = g_info->current_level.brickOffsetX + c * (g_info->current_level.brickWidth + g_info->current_level.brickSpacing);
                int by = g_info->current_level.brickOffsetY + r * (g_info->current_level.brickHeight + g_info->current_level.brickSpacing);

                int ballLeft = ball.x - ball.radius;
                int ballRight = ball.x + ball.radius;
                int oldBallLeft = old_x - ball.radius;
                int oldBallRight = old_x + ball.radius;
                int ballTop = ball.y - ball.radius;
                int ballBottom = ball.y + ball.radius;
                int oldBallTop = old_y - ball.radius;
                int oldBallBottom = old_y + ball.radius;
                int brickLeft = bx;
                int brickRight = bx + g_info->current_level.brickWidth;
                int brickTop = by;
                int brickBottom = by + g_info->current_level.brickHeight;

                bool collisionX = (oldBallRight <= brickLeft && ballRight >= brickLeft) || 
                                (oldBallLeft >= brickRight && ballLeft <= brickRight);

                bool collisionY = (oldBallBottom <= brickTop && ballBottom >= brickTop) || 
                                (oldBallTop >= brickBottom && ballTop <= brickBottom);

                if ((ballRight > brickLeft && ballLeft < brickRight && 
                    ballBottom > brickTop && ballTop < brickBottom) ||
                    (collisionX && ballBottom > brickTop && ballTop < brickBottom) ||
                    (collisionY && ballRight > brickLeft && ballLeft < brickRight)) {
                    ball.collided_c = c;
                    ball.collided_r = r;  
                    g_info->current_level.bricks[r][c]--;
                    if (g_info->current_level.bricks[r][c] <= 0) {
                        g_info->game_finished = check_game_finished();
                        draw_brick(r, c, true, ~ST77XX_BLACK);
                        g_info->points += 10;
                        draw_header();
                    } else {
                        g_info->game_finished = false;
                        draw_brick(r, c);
                    }
                
                    int overlapLeft = ballRight - brickLeft;
                    int overlapRight = brickRight - ballLeft;
                    int overlapTop = ballBottom - brickTop;
                    int overlapBottom = brickBottom - ballTop;
                    int minOverlap = min(min(overlapLeft, overlapRight), min(overlapTop, overlapBottom));

                    if (overlapLeft < overlapRight && overlapLeft < overlapTop && overlapLeft < overlapBottom) {
                        if (ball.dx > 0) ball.dx = -ball.dx; // Ball was moving right
                    } else if (overlapRight < overlapLeft && overlapRight < overlapTop && overlapRight < overlapBottom) {
                        if (ball.dx < 0) ball.dx = -ball.dx; // Ball was moving left
                    } else if (overlapTop < overlapBottom) {
                        if (ball.dy > 0) ball.dy = -ball.dy; // Ball was moving down
                    } else {
                        if (ball.dy < 0) ball.dy = -ball.dy; // Ball was moving up
                    }
                } 
                else if (oldBallRight > brickLeft && oldBallLeft < brickRight && oldBallBottom > brickTop && oldBallTop < brickBottom) {
                    ball.collided_c = c;
                    ball.collided_r = r;   
                }
            }
        }
    }
}