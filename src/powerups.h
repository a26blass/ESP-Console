
#ifndef POWERUPS_H
#define POWERUPS_H

#define POWERUP_DROP_SPEED 1.0f

enum powerup_id {
    LARGEBALL,
    MULTIBALL,
    PLUSONE,
    LASER
};

typedef struct {
    bool active;
    float x;
    float y;
    powerup_id id;
} powerup_instance;


typedef struct {
    powerup_instance active_powerups[10];
    int lowest_idx;
    int num_active;
} powerup_state_t;



#endif