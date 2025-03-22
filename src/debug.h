
#define DEBUG
#define BOOT_PIN 0 

#ifndef DEBUG_H

typedef struct {
    bool setup_complete;
    bool screen_init;
} debug_t;

void query_display_status();
void debug_init();

#endif