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

// Variables to track pin number and their max/min voltages. Pins configured in config.h
PinVoltage pinVolts[] = CELL_PINS;
constexpr size_t numPins = sizeof(pinVolts) / sizeof(pinVolts[0]);

// Variables to track total voltage and voltage variation
Voltage totalVolt;
VoltageVariation variation;

// alerts array, initialized in setup()
constexpr size_t numAlerts = numPins + 2; // all cells + total voltage + voltage variation
Alert *alerts[numAlerts];
size_t currentAlert = 0;

// Keep track of main page and current page
Page *mainPage;
Page *page;

void setup() {
   // NOTE: Call this before any analogRead calls or else VREF pin and internal voltage reference will short
   //analogReference(EXTERNAL);

   Bridge.begin();
   lcd.begin(16, 2);

   // Setup output pins
   pinMode(OUTPUT_PIN, OUTPUT);
   pinMode(ALERT_PIN_FLASH, OUTPUT);
   pinMode(ALERT_PIN_CONST, OUTPUT);

   // Setup input pins
   for(size_t i = 0; i < numPins; i++) {
      pinMode(pinVolts[i].pin, INPUT);
   }

   // Stablize the input
   for(int h = 0; h < 10; h++) {
      for(size_t i = 0; i < numPins; i++) {
         pinVolts[i].update();
      }
   }

   // Setup menu pages

   // Variation pages
   Page *varPage = new VariationPage(&variation, false);
   Page *maxVarPage = new VariationPage(&variation, true);

   varPage->right = varPage->left = maxVarPage;
   maxVarPage->right = maxVarPage->left = varPage;

   // Main page
   mainPage = new NormalPage("TOTAL", &totalVolt.now, "VAR", &variation.now);
   Page *mainMinMax = new NormalPage("TOTAL X", &totalVolt.max, "TOTAL N", &totalVolt.min);

   // Put cell variation pages on top of the main page
   mainPage->right = mainPage->left = mainMinMax;
   mainMinMax->right = mainMinMax->left = mainPage;

   varPage->down = mainPage;
   maxVarPage->down = mainMinMax;

   mainPage->up = varPage;
   mainMinMax->up = maxVarPage;

   // Cell voltage pages
   Page *cellPage = mainPage;
   for(size_t i = 0; i < numPins; i++) {
      // Cell voltage page
      Page *page = new VoltagePage(i, &pinVolts[i]);

      if(i == 0) {
         // First one, put it below the main page
         mainPage->down = mainMinMax->down = page;
         page->up = mainPage;
      } else {
         // Put it below the last page
         cellPage->down = page;
         page->up = cellPage;
      }

      cellPage = page;

      // Cell voltage alert that opens this page when viewed
      alerts[i] = new VoltageAlert(&pinVolts[i], page, MIN_CELL_VOLTAGE, MAX_CELL_VOLTAGE);
   }

   // Link the last page to the cell variation page so it loops around
   cellPage->down = varPage;
   varPage->up = maxVarPage->up = cellPage;

   // Cell variation alert, opens total variation page when viewed
   alerts[numPins] = new VariationAlert(&variation, maxVarPage);

   // Total voltage alert, opens total voltage page when viewed
   alerts[numPins+1] = new VoltageAlert(&totalVolt, mainMinMax, MIN_TOTAL_VOLTAGE, MAX_TOTAL_VOLTAGE);

   // Set current page to main page
   page = mainPage;
}

void loop() {
   evse_loop();

   // Find all the voltages
   double total = 0;
   variation.start();

   for(size_t i = 0; i < numPins; i++) {
      pinVolts[i].update();
      double volt = pinVolts[i].now;
      total += volt;
      variation.update(i, volt);
   }

   totalVolt.update(total);
   variation.finish();

   // Update alerts and check if any of them is triggered
   bool hasAlert = false;
   for(size_t i = 0; i < numAlerts; i++) {
      alerts[i]->update();
      hasAlert |= alerts[i]->triggered;
   }

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
        alerts[currentAlert]->reset();
        selectBtnPressed = -1;
     }

   } else if(debounce & BUTTON_SELECT && selectBtnPressed >= 0) {
      // Button released before long press is triggered
      if(hasAlert) {
         // View the next alert
         for(size_t i = (currentAlert+1) % numAlerts; i != currentAlert; i = (i+1) % numAlerts) {
            if(alerts[i]->triggered) {
               page = alerts[i]->alertPage;
               currentAlert = i;
               break;
            }
         }
      } else {
         // Return to main page
         page = mainPage;
      }
   }

   // Navigation buttons
   else if(DEBOUNCE(BUTTON_DOWN) && page->down)
      page = page->down;

   else if(DEBOUNCE(BUTTON_UP) && page->up)
      page = page->up;

   else if(DEBOUNCE(BUTTON_LEFT) && page->left)
      page = page->left;
   
   else if(DEBOUNCE(BUTTON_RIGHT) && page->right)
      page = page->right;

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
   if(hasAlert) {
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

   digitalWrite(ALERT_PIN_CONST, hasAlert ? HIGH : LOW);

   // Outputs control

   // Saves the current state so MQTT command isn't sent every loop
   static OutputState out_state = NONE;

   if(totalVolt.now > OUTPUT_THRESHOLD_ON) {
      // Turn on output
      if(out_state != ON) {
         digitalWrite(OUTPUT_PIN, HIGH);
         evse_enable(true);
         out_state = ON;
      }
   } else if(totalVolt.now < OUTPUT_THRESHOLD_OFF) {
      // Turn off output
      if(out_state != OFF) {
         digitalWrite(OUTPUT_PIN, LOW);
         evse_enable(false);
         out_state = OFF;
      }
   }

   // Print to LCD
   if(now - lastPrint > REFRESH_RATE) {
      lastPrint = now;
      page->draw();

      if(lightDuration > now)
         lcd.setBacklight(1);
      else if(!hasAlert)
         lcd.setBacklight(0);
   }

   delay(LOOP_DELAY);
}
