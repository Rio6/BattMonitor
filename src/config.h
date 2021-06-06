#ifndef CONFIG_H
#define CONFIG_H

#define ALERT_PIN_FLASH 13 // A pin that flashes when alert is triggered
#define ALERT_PIN_CONST 22 // A pin that is on constantly when alert is triggered

#define MAX_CELL_VOLTAGE 4.05      // Trigger alert when any individual cell has more than this voltage
#define MIN_CELL_VOLTAGE 3.7       // Trigger alert when any individual cell has less than this voltage
#define MAX_TOTAL_VOLTAGE 32.4     // Trigger alert when sum of all cells has more than this voltage
#define MIN_TOTAL_VOLTAGE 29.6     // Trigger alert when sum of all cells has less than this voltage
#define MAX_VOLTAGE_VARIATION 0.02 // Trigger alert when the difference betwen two cell is more than this voltage

#define OUTPUT_PIN 7              // A pin that outputs HIGH when the charger should be enabled
#define OUTPUT_THRESHOLD_ON 31.6  // Voltage for when the charger should be enabled
#define OUTPUT_THRESHOLD_OFF 30.6 // Voltage for when the charger should be disabled

#define LOOP_DELAY 50         // Time to delay in each loop, in milliseconds
#define LONG_PRESS_DELAY 1500 // Time for a longpress to be registered, in milliseconds
#define BACKLIGHT_DURATION 15000 // Time for the blacklight to stay on after button press, in milliseconds
#define REFRESH_RATE 1000     // Time between each LCD refresh, in milliseconds
#define FLASH_RATE 2000       // Time between each flash when alert is triggered, in milliseconds

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

#define VREF 5

#define MQTT_ID "yun"
#define MQTT_USER "controller"
#define MQTT_PASS "rellortnoc"

#endif
