#ifndef CONFIG_H
#define CONFIG_H

#define MAX_CELL_VOLTAGE 4.6
#define MIN_CELL_VOLTAGE 3.5
#define MAX_TOTAL_VOLTAGE 37
#define MIN_TOTAL_VOLTAGE 35
#define MAX_VOLTAGE_VARIATION 0.5

#define BACKLIGHT_DURATION 15 // in seconds
#define LOOP_DELAY 50 // in milliseconds
#define LONG_PRESS_DELAY 2000 // in milliseconds

#define CELL_PINS { \
   { A8  }, \
   { A9  }, \
   { A10 }, \
   { A11 }, \
   { A12 }, \
   { A13 }, \
   { A14 }, \
   { A15 }, \
}

#define ALERT_PIN_FLASH 13
#define ALERT_PIN_CONST 22

#endif
