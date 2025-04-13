#include <Arduino.h>
#include "powerups.h"
#include "display.h"


powerup_state_t powerup_state;

void add_powerup(int x, int y, powerup_id id) {
    int idx = powerup_state.lowest_idx;

    if (idx >= 10) {
        Serial.println("No space to add new powerup!");
        return;
    }

    // Insert at lowest_idx
    powerup_state.active_powerups[idx].active = true;
    powerup_state.active_powerups[idx].x = (float)x;
    powerup_state.active_powerups[idx].y = (float)y;
    powerup_state.active_powerups[idx].id = id;

    powerup_state.num_active++;

    // Find the next lowest available index
    powerup_state.lowest_idx = 10; // Reset to max
    for (int i = 0; i < 10; i++) {
        if (!powerup_state.active_powerups[i].active) {
            powerup_state.lowest_idx = i;
            break;
        }
    }
}

void draw_powerup(powerup_instance *p) {
    int x = p->x;
    int y = p->y;
    switch (p->id)
    {
    case LARGEBALL:
        drawLargeBall(x, y);
        break;

    case MULTIBALL:
        drawExtraBalls(x, y);
        break;

    case PLUSONE:
        drawPlusOne(x, y);
        break;

    case LASER:
        drawLaser(x, y);
        break;
    
    default:
        ESP_LOGE("GENERAL ERROR", "NONEXISTENT POWERUP");
        break;
    }
}

void update_powerups() {
    for (int i = 0; i < 10; i++) {
        powerup_instance* p = &powerup_state.active_powerups[i];
        if (!p->active) continue;

        // Erase previous position (black 10x10 shadow)
        draw_rect((int)p->x, (int)p->y, 10, 10, ~ST77XX_BLACK);

        // Update y position
        p->y += POWERUP_DROP_SPEED;

        // Check if it fell off the screen
        if (p->y >= SCREEN_HEIGHT) {
            p->active = false;
            powerup_state.num_active--;

            // Update lowest_idx if needed
            if (i < powerup_state.lowest_idx) {
                powerup_state.lowest_idx = i;
            }
            continue;
        } else {
            // draw the powerup
            draw_powerup(p);
        }

    }
}
