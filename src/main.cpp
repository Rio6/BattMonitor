#include "voltage.hpp"
#include "page.hpp"
#include "alert.hpp"
#include "config.h"

#include <Arduino.h>
#include <Adafruit_RGBLCDShield.h>
#include <string.h>

Adafruit_RGBLCDShield lcd;

PinVoltage pinVolts[] = CELL_PINS;
constexpr size_t numPins = sizeof(pinVolts) / sizeof(pinVolts[0]);

Page *page;
Voltage totalVolt;
VoltageVariation variation;

constexpr size_t numAlerts = numPins + 1; // all cells + voltage variation
Alert *alerts[numAlerts];
size_t currentAlert = 0;

int lightCounter = BACKLIGHT_DURATION;

void setup() {
   // NOTE: Call this before any analogRead calls or else VREF pin and internal voltage reference will short
   analogReference(EXTERNAL);

   lcd.begin(16, 2);
   Serial.begin(9600);

   pinMode(ALERT_PIN_FLASH, OUTPUT);
   pinMode(ALERT_PIN_CONST, OUTPUT);

   // Stablize the input
   for(int i = 0; i < 10; i++) {
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
   Page *mainPage = new NormalPage("TOTAL", &totalVolt.now, "VAR", &variation.now);
   Page *mainMinMax = new NormalPage("MAX", &totalVolt.max, "MIN", &totalVolt.min);

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
         mainPage->down = mainMinMax->down = page;
         page->up = mainPage;
      } else {
         cellPage->down = page;
         page->up = cellPage;
      }

      cellPage = page;

      // Cell voltage alert
      alerts[i] = new VoltageAlert(&pinVolts[i], page);
   }

   cellPage->down = varPage;
   varPage->up = maxVarPage->up = cellPage;

   page = mainPage;

   // Cell variation alert
   alerts[numPins] = new VariationAlert(&variation, maxVarPage);
}

void loop() {
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

   // Update alerts
   bool hasAlert = false;
   for(size_t i = 0; i < numAlerts; i++) {
      alerts[i]->update();
      hasAlert |= alerts[i]->triggered;
   }

   long now = millis();

   // Buttons
   // Lazy debouncing, returns non-zero when button is pressed and released
   #define DEBOUNCE(btn) (~buttons & (btn) && debounce & (btn))
   static uint8_t debounce = 0;
   uint8_t buttons = lcd.readButtons();

   static long selectBtnPressed = -1;
   if(buttons & BUTTON_SELECT) {
      if(~debounce & BUTTON_SELECT) {
         selectBtnPressed = now;
      } else if(selectBtnPressed >= 0 && now - selectBtnPressed > LONG_PRESS_DELAY) {
         alerts[currentAlert]->reset();
         selectBtnPressed = -1;
      }
   }

   else if(debounce & BUTTON_SELECT) {
      if(selectBtnPressed >= 0) {
         for(size_t i = (currentAlert+1) % numAlerts; i != currentAlert; i = (i+1) % numAlerts) {
            if(alerts[i]->triggered && alerts[i]->alertPage != page) {
               currentAlert = i;
               page = alerts[i]->alertPage;
               break;
            }
         }
      }
   }

   else if(DEBOUNCE(BUTTON_DOWN) && page->down)
      page = page->down;
   
   else if(DEBOUNCE(BUTTON_UP) && page->up)
      page = page->up;
   
   else if(DEBOUNCE(BUTTON_LEFT) && page->left)
      page = page->left;
   
   else if(DEBOUNCE(BUTTON_RIGHT) && page->right)
      page = page->right;

   if(buttons)
      lightCounter = BACKLIGHT_DURATION;
   
   static long lastPrint = 0;
   if(buttons != debounce)
      lastPrint = 0;

   debounce = buttons;

   // Alert flashing
   if(hasAlert) {
      if(now % 2000 > 1000) {
         digitalWrite(ALERT_PIN_FLASH, HIGH);
         if(lightCounter == 0)
            lcd.setBacklight(1);
      } else {
         digitalWrite(ALERT_PIN_FLASH, LOW);
         if(lightCounter == 0)
            lcd.setBacklight(0);
      }
   }

   digitalWrite(ALERT_PIN_CONST, hasAlert ? HIGH : LOW);

   // Print to LCD
   if(now - lastPrint > 1000) {
      lastPrint = now;
      page->draw();

      if(lightCounter > 0) {
         lcd.setBacklight(1);
         lightCounter--;
      } else if(!hasAlert) {
         lcd.setBacklight(0);
      }
   }

   delay(LOOP_DELAY);
}
