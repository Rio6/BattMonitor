#include "voltage.hpp"
#include "page.hpp"
#include "alert.hpp"
#include "evse.hpp"
#include "config.h"

#include <Arduino.h>
#include <Adafruit_RGBLCDShield.h>
#include <Bridge.h>
#include <string.h>

// enum used to track current state so it doesn't spam messages over MQTT
enum OutputState {
   NONE,
   ON,
   OFF,
};

// LCD shield controller
Adafruit_RGBLCDShield lcd;

// Variables to track total voltage
TotalPinVoltage *totalVolt = new TotalPinVoltage(TOTAL_VOLT_PIN);

// alerts array, initialized in setup()
Alert *alert;

// Keep track of main page and current page
Page *page;

void setup() {
   // NOTE: Call this before any analogRead calls or else VREF pin and internal voltage reference will short
   analogReference(EXTERNAL);

   Bridge.begin();
   lcd.begin(16, 2);

   // Setup output pins
   pinMode(OUTPUT_PIN, OUTPUT);
   pinMode(ALERT_PIN_FLASH, OUTPUT);
   pinMode(ALERT_PIN_CONST, OUTPUT);
   pinMode(totalVolt->pin, INPUT);

   // Stablize the input
   for(int h = 0; h < 10; h++) {
      totalVolt->update();
   }

   // Total voltage alert
   page = new TotalVoltagePage(totalVolt);
   alert = new VoltageAlert(totalVolt, page, MIN_TOTAL_VOLTAGE, MAX_TOTAL_VOLTAGE);
}

void loop() {
   evse_loop();

   // Find the voltage
   totalVolt->update();

   alert->update();

   long now = millis();

   // Buttons
   // Lazy debouncing, returns non-zero when button is pressed and released
   #define DEBOUNCE(btn) (buttons & (btn) && ~debounce & (btn))
   static uint8_t debounce = 0;

   // Variable to keep track of long press on select button
   static long selectBtnPressed = -1;

   // Read the buttons on LCD shield
   uint8_t buttons = lcd.readButtons();

   // Handle select button
   if(buttons & BUTTON_SELECT) {
     if(~debounce & BUTTON_SELECT) {
        // First pressed, strt counting long press counter
        selectBtnPressed = now;

     } else if(selectBtnPressed >= 0 && now - selectBtnPressed > LONG_PRESS_DELAY) {
        // Long press, reset current alert and stop counting
        alert->reset();
        selectBtnPressed = -1;
     }

   } else if(debounce & BUTTON_SELECT && selectBtnPressed >= 0) {
      // Button released before long press is triggered
      if(alert->triggered) {
         // View the next alert
         /*
         for(size_t i = (currentAlert+1) % numAlerts; i != currentAlert; i = (i+1) % numAlerts) {
            if(alerts->triggered) {
               page = alerts->alertPage;
               break;
            }
         }
         */
      }
   }

   // Controlls blacklight duration and refresh rate
   static long lightDuration = BACKLIGHT_DURATION;
   static long lastPrint = 0;
   
   if(buttons != debounce) // Refresh page when any button is pressed/released
      lastPrint = 0;

   if(buttons) // Keep backlight on when any of the button is being pressed
      lightDuration = now + BACKLIGHT_DURATION;

   // Update debounce state
   debounce = buttons;

   // Alert flashing
   if(alert->triggered) {
      if(now % FLASH_RATE > FLASH_RATE / 2) {
         digitalWrite(ALERT_PIN_FLASH, HIGH);
         if(lightDuration < now) // backlight is not on
            lcd.setBacklight(1);
      } else {
         digitalWrite(ALERT_PIN_FLASH, LOW);
         if(lightDuration < now) // backlight is not on
            lcd.setBacklight(0);
      }
   }

   digitalWrite(ALERT_PIN_CONST, alert->triggered ? HIGH : LOW);

   // Outputs control

   // Saves the current state so MQTT command isn't sent every loop
   static OutputState outState = NONE;
   static long lastOut = 0;

   if(now - lastOut > RESEND_INTERVAL) {
      outState = NONE;
      lastOut = now;
   }

   if(totalVolt->now > OUTPUT_THRESHOLD_ON) {
      // Turn on output
      if(outState != ON) {
         digitalWrite(OUTPUT_PIN, HIGH);
         evse_enable(true);
         outState = ON;
      }
   } else if(totalVolt->now < OUTPUT_THRESHOLD_OFF) {
      // Turn off output
      if(outState != OFF) {
         digitalWrite(OUTPUT_PIN, LOW);
         evse_enable(false);
         outState = OFF;
      }
   }

   // Print to LCD
   if(now - lastPrint > REFRESH_RATE) {
      lastPrint = now;
      page->draw();

      if(lightDuration > now)
         lcd.setBacklight(1);
      else if(!alert->triggered)
         lcd.setBacklight(0);
   }

   delay(LOOP_DELAY);
}
