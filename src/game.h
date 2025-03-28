#include <levels.h>

#define MAX_LIVES 6
#define STARTER_LIVES 3
#define BRICK_INCR_AMT 5
#define MIN_BRICK_HEIGHT 265
#define DEFAULT_INTERVAL 7000 // Default time interval for bricks moving down (ms)
#define MIN_INTERVAL 2500 // Minimum time interval for bricks moving down (ms)
#define DELTA_INTERVAL 500

#ifndef GAME_H
typedef struct {
    LevelInfo current_level;
    int current_level_index;
    int num_levels;
    int points;
    int lives;
    bool redraw_header;
    unsigned long last_interval;
    float interval;
    const float min_interval;
    bool game_started;
    bool game_finished;
} game_t;

game_t *get_game_info();
void start_game();
bool check_game_finished();
void next_level(bool use_load_screen=true);
void load_level(int levelIndex, bool use_load_scree=true);
void game_cycle();

#endif