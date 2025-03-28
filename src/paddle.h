
// Paddle Constants
#define PADDLE_SPEED_DAMPEN paddle_speed_fn
#define PADDLE_SPEED_CONST_MUL 1.6
#define BOUNCE_FACTOR 0.9

#ifndef PADDLE_H
typedef struct {
    const int paddle_width;
    const int paddle_height;
    float paddle_x;
    const int paddle_y;
    float paddle_speed;
    bool left;
    int left_bound;
    int right_bound;
    int target_coord;
} paddle_t;

float paddle_speed_fn(float speed);
paddle_t *get_paddle_info();
void incr_paddle_auto();

#endif