#include "voltage.hpp"
#include "page.hpp"
#include "alert.hpp"
#include "config.h"

#include <Arduino.h>
#include <Adafruit_RGBLCDShield.h>
#include <string.h>

Adafruit_RGBLCDShield lcd;

constexpr size_t numPins = 8;
PinVoltage pinVolts[numPins] = {
   { A8  },
   { A9  },
   { A10 },
   { A11 },
   { A12 },
   { A13 },
   { A14 },
   { A15 },
};

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

   // Stablize the input
   for(int i = 0; i < 10; i++) {
      for(size_t i = 0; i < numPins; i++) {
         pinVolts[i].update();
      }
   }

   // Setup menu pages
   Page *varPage = new VariationPage(&variation, false);
   Page *varMinMax = new VariationPage(&variation, true);

   varPage->right = varPage->left = varMinMax;
   varMinMax->right = varMinMax->left = varPage;

   Page *mainPage = new NormalPage("TOTAL", &totalVolt.now, "VAR", &variation.now);
   Page *mainMinMax = new NormalPage("MAX", &totalVolt.max, "MIN", &totalVolt.min);

   mainPage->right = mainPage->left = mainMinMax;
   mainMinMax->right = mainMinMax->left = mainPage;

   varPage->down = mainPage;
   varMinMax->down = mainMinMax;

   mainPage->up = varPage;
   mainMinMax->up = varMinMax;

   // Cell voltage pages and alerts
   Page *cellPage = mainPage;
   for(size_t i = 0; i < numPins; i += 2) {
      char line0[6] = {0};
      char line1[6] = {0};

      sprintf(line0, "CELL%d", i+1);
      sprintf(line1, "CELL%d", i+2);
      Page *page = new NormalPage(line0, &pinVolts[i].now, line1, &pinVolts[i+1].now);

      sprintf(line0, "MAX%d", i+1);
      sprintf(line1, "MAX%d", i+2);
      Page *max = new NormalPage(line0, &pinVolts[i].max, line1, &pinVolts[i+1].max);

      sprintf(line0, "MIN%d", i+1);
      sprintf(line1, "MIN%d", i+2);
      Page *min = new NormalPage(line0, &pinVolts[i].min, line1, &pinVolts[i+1].min);

      page->right = max;
      max->right = min;
      min->right = page;

      page->left = min;
      min->left = max;
      max->left = page;

      if(i == 0) {
         mainPage->down = mainMinMax->down = page;
         page->up = max->up = min->up = mainPage;
      } else {
         cellPage->down = page;
         cellPage->left->down = min;
         cellPage->right->down = max;

         page->up = cellPage;
         max->up = cellPage->right;
         min->up = cellPage->left;
      }

      alerts[i] = new VoltageAlert(&pinVolts[i], page);
      alerts[i+1] = new VoltageAlert(&pinVolts[i+1], page);

      cellPage = page;
   }

   cellPage->down = cellPage->left->down = cellPage->right->down = varPage;
   varPage->up = varMinMax->up = cellPage;

   page = mainPage;

   // Cell variation alert
   alerts[numPins] = new VariationAlert(&variation, varMinMax);
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
   for(size_t i = 0; i < numAlerts; i++) {
      alerts[i]->update();
   }

   // Buttons
   static long lastPrint = 0;

   // Lazy debouncing, returns non-zero when button is first become pressed, zero afterwards
   #define DEBOUNCE(btn) (buttons & (btn) && ~debounce & (btn))
   static uint8_t debounce = 0;

   uint8_t buttons = lcd.readButtons();

   if(DEBOUNCE(BUTTON_SELECT)) {
      for(size_t i = (currentAlert+1) % numAlerts; i != currentAlert; i = (i+1) % numAlerts) {
         if(alerts[i]->triggered && alerts[i]->alertPage != page) {
            currentAlert = i;
            page = alerts[i]->alertPage;
            break;
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
   
   if(buttons ^ debounce)
      lastPrint = 0;

   debounce = buttons;

   if(buttons)
      lightCounter = BACKLIGHT_DURATION;

   // Print to LCD
   long now = millis();
   if(now - lastPrint > 1000) {
      lastPrint = now;
      page->draw();

      if(lightCounter > 0) {
         lcd.setBacklight(1);
         lightCounter--;
      } else {
         lcd.setBacklight(0);
      }
   }
}
