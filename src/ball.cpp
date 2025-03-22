#include <cmath>
#include "ball.h"
#include "paddle.h"
#include "ili9341.h"
#include "game.h"
#include "util.h"

ball_t ball = {
    .radius = 3,
    .x = 30, .y = 100,
    .dx = 2, .dy = -2,
    .speed = STARTER_SPEED,
    .ball_on_paddle = true,
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

    ball.x = p_info->paddle_x + p_info->paddle_width/2;
    ball.y = p_info->paddle_y - p_info->paddle_height - ball.radius-1;
    
    ball.launch_angle = getRandomInt(30, 150);

    while (ball.launch_angle > 80 && ball.launch_angle < 100) {
        ball.launch_angle = getRandomInt(30, 150);
    }

    draw_launch_angle_indicator();

    draw_ball(ball.x, ball.y, ball.x, ball.y, ball.radius);
    
    
    delay(2500);
    launch_ball();
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
    if (ball.y + ball.radius >= SCREEN_HEIGHT && ball.dy > 0) {
        if (g_info->lives > 0) {
            g_info->lives -= 1;
        } else {
            g_info->current_level_index = -1;
            g_info->points = 0;
            g_info->lives = 3;
            ball.speed = STARTER_SPEED;

            next_level();
        }
        draw_header();

        ball.x = p_info->paddle_x + p_info->paddle_width/2;
        ball.y = p_info->paddle_y - p_info->paddle_height - ball.radius-1;

        ball.ball_on_paddle = true;
    }

    if (ball.y + ball.radius >= g_info->current_level.brickOffsetY) {
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
                            draw_brick(r, c, true, ILI9341_BLACK);
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

                        if (minOverlap == overlapLeft && overlapLeft < overlapTop && overlapLeft < overlapBottom) {
                            ball.dx = -ball.dx; // Bounce horizontally
                        } else if (minOverlap == overlapRight && overlapRight < overlapTop && overlapRight < overlapBottom) {
                            ball.dx = -ball.dx; // Bounce horizontally
                        } else {
                            ball.dy = -ball.dy; // Bounce vertically
                        }

                    } 
                    else if (oldBallRight > brickLeft && oldBallLeft < brickRight && oldBallBottom > brickTop && oldBallTop < brickBottom) {
                        ball.collided_c = c;
                        ball.collided_r = r;   
                    }
                }
            }
        }
    } else {
        g_info->game_finished = false;
        ball.collided_c = -1;
        ball.collided_r = -1;
    }
}