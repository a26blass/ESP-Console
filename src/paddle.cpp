#include <cmath>
#include <Arduino.h>
#include "paddle.h"
#include "ili9341.h"
#include "ball.h"
#include "util.h"

// Initialize paddle instance
paddle_t paddle = {
    .paddle_width = 50,
    .paddle_height = 5,
    .paddle_x = (SCREEN_WIDTH - 50) / 2,
    .paddle_y = SCREEN_HEIGHT - 10,
    .paddle_speed = 0.0,
    .left = true,
    .left_bound = 5,
    .right_bound = 5,
    .target_coord = 0
};

// Macro functions
float paddle_speed_fn(float speed) {
    return sqrt(speed) * PADDLE_SPEED_CONST_MUL;
}

// Access paddle struct
paddle_t *get_paddle_info() {
    return &paddle;
}

void handle_collision() {
    ball_t *b_info = get_ball_info();

    if (b_info->y + b_info->radius >= paddle.paddle_y && b_info->y + b_info->radius <= paddle.paddle_y + paddle.paddle_height && b_info->x+b_info->radius >= paddle.paddle_x && b_info->x <= paddle.paddle_x + paddle.paddle_width + b_info->radius) {
        float hitPos = ((b_info->x + b_info->radius) - (paddle.paddle_x + paddle.paddle_width / 2)) / ((paddle.paddle_width / 2) + b_info->radius);

        b_info->dx = b_info->speed * hitPos * BOUNCE_FACTOR; // Angle the bounce
        b_info->dy = -sqrt(b_info->speed * b_info->speed - b_info->dx * b_info->dx); // Adjust dy to preserve total speed

        paddle.target_coord = getRandomInt(3*b_info->radius, paddle.paddle_width-3*b_info->radius); // Where on the paddle to hit next?
        b_info->hit_paddle = true;
    } else {
        b_info->hit_paddle = false;
    }
}

// Function that moves the paddle to try and hit the ball
void incr_paddle_auto() {
    ball_t *b_info = get_ball_info();

    // Paddle logic
    draw_paddle();

    handle_collision();

    // TODO: Remove
    if (!b_info->ball_on_paddle && (b_info->dy > 0 || abs(b_info -> dx) > paddle.paddle_speed || b_info->y >= SCREEN_HEIGHT/2)) {
        int padding = b_info->radius;
        if(b_info->x < paddle.paddle_x + paddle.target_coord - padding) {
            movePaddleDraw(-paddle.paddle_speed);
        } else if (b_info->x > paddle.paddle_x + paddle.target_coord + padding) {
            movePaddleDraw(paddle.paddle_speed);
        } else {
            if (b_info->dx > 0)
                movePaddleDraw(min(b_info->dx, paddle.paddle_speed));
            else
                movePaddleDraw(max(b_info->dx, -paddle.paddle_speed));
        }
    }
}