
#define DEBUG
#define BOOT_PIN 0 
#define LED_PIN 2
#define BOOT_DELAY_MS 1500
#define LED_FLASH_INTERVAL 35

#ifndef DEBUG_H

typedef struct {
    bool setup_complete;
    bool screen_init;
} debug_t;

void start_blinking();
void stop_blinking();
void dbg_write_line(const char *line);
void query_display_status();
void debug_delay_ms(int ms = BOOT_DELAY_MS);
void debug_init();

#endif