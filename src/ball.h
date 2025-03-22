#define MAX_SPEED 7.0
#define STARTER_SPEED 3.5


#ifndef BALL_H
// Structs
typedef struct {
    const int radius;
    float x, y;
    float dx, dy;
    float speed;
    bool ball_on_paddle;
    float launch_angle;
    int prev_end_x;
    int prev_end_y;
    int collided_r, collided_c;
} ball_t;

ball_t *get_ball_info();
void ball_collision();
void launch_ball_auto();

#endif