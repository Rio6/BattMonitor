#include <Arduino.h>
#include <Adafruit_RGBLCDShield.h>
#include <string.h>

constexpr double VREF = 5;

Adafruit_RGBLCDShield lcd;

struct PinVoltage {
   double min = INFINITY;
   double max = 0;
   double now = 0;
   int pin;

   PinVoltage(int _pin): pin(_pin) {}
};

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

constexpr int8_t Page_col = 2;
enum Page {
   ALARM = -1,

   MAIN,    MAIN_MINMAX,
   CELL0,   CELL0_MINMAX,
   CELL1,   CELL1_MINMAX,
   CELL2,   CELL2_MINMAX,
   CELL3,   CELL3_MINMAX,
   MINMAX,  CELL4_MINMAX,
   MISC0,   MISC1,

   Page_len
};

double readVoltage(int pin) {
   return (analogRead(pin) + 0.5) / 1024 * VREF;
}

void updateVolts() {
   for(size_t i = 0; i < numPins; i++) {
      double volt = readVoltage(pinVolts[i].pin);
      pinVolts[i].now = volt;
      if(volt > pinVolts[i].max)
         pinVolts[i].max = volt;
      if(volt < pinVolts[i].min)
         pinVolts[i].min = volt;
   }
}

// Functions to switch pages. Using some (confusing) math so I don't need to write lots of if elses
Page downPage(Page page) {
   return Page((int(page) + Page_col) % int(Page_len));
}

Page upPage(Page page) {
   return Page((int(page) - Page_col + int(Page_len)) % int(Page_len));
}

Page rightPage(Page page) {
   int pageNum = int(page);
   return Page(pageNum / Page_col * Page_col + (pageNum + 1) % Page_col);
}

Page leftPage(Page page) {
   int pageNum = int(page);
   return Page(pageNum / Page_col * Page_col + (pageNum - 1) % Page_col);
}

void setup() {
   // NOTE: Call this before any analogRead calls or else VREF pin and internal voltage reference will short
   analogReference(EXTERNAL);

   lcd.begin(16, 2);
   Serial.begin(9600);

   // Stablize the input
   for(int i = 0; i < 10; i++) {
      updateVolts();
   }
}

void loop() {
   static Page page = MAIN;

   updateVolts();

   // Calculate these every loop so they can be used in alarm
   double minVolt = INFINITY;
   double maxVolt = 0;
   double totalVolts = 0;

   for(size_t i = 0; i < numPins; i++) {
      double volt = pinVolts[i].now;
      totalVolts += volt;
      if(volt < minVolt)
         minVolt = volt;
      if(volt > maxVolt)
         maxVolt = volt;
   }

   static double minTotal = INFINITY;
   static double maxTotal = 0;
   if(totalVolts < minTotal)
      minTotal = totalVolts;
   if(totalVolts > maxTotal)
      maxTotal = totalVolts;

   static double maxVary = 0;
   double vary = maxVolt - minVolt;
   if(vary > maxVary)
      maxVary = vary;

   // Buttons
   static long lastPrint = 0;

   // Lazy debouncing
   #define DEBOUNCE(btn) (buttons & (btn) && ~debounce & (btn))
   static uint8_t debounce = 0;

   uint8_t buttons = lcd.readButtons();

   if(DEBOUNCE(BUTTON_DOWN))
      page = downPage(page);

   if(DEBOUNCE(BUTTON_UP))
      page = upPage(page);

   if(DEBOUNCE(BUTTON_LEFT))
      page = leftPage(page);

   if(DEBOUNCE(BUTTON_RIGHT))
      page = rightPage(page);

   if(buttons ^ debounce)
      lastPrint = 0;

   debounce = buttons;

   // Print to LCD
   long now = millis();
   if(now - lastPrint > 1000) {
      lastPrint = now;

      char buff[17];

      switch(page) {
         case ALARM:
            break;

         case MAIN:
            lcd.setCursor(0, 0);
            lcd.print("TOTAL   ");
            lcd.print(dtostrf(totalVolts, 8, 4, buff));

            lcd.setCursor(0, 1);
            lcd.print("VAR     ");
            lcd.print(dtostrf(vary, 8, 4, buff));
            break;

         case MAIN_MINMAX:
            lcd.setCursor(0, 0);
            lcd.print("MAX     ");
            lcd.print(dtostrf(maxTotal, 8, 4, buff));

            lcd.setCursor(0, 1);
            lcd.print("MIN     ");
            lcd.print(dtostrf(minTotal, 8, 4, buff));
            break;

         case CELL0:
         case CELL1:
         case CELL2:
         case CELL3:
            {
               int cell = int(page) - int(CELL0);
               for(int i = 0; i < 2; i++) {
                  lcd.setCursor(0, i);
                  lcd.print("CELL"); lcd.print(cell + i);
                  lcd.print("   ");
                  lcd.print(dtostrf(pinVolts[cell + i].now, 8, 4, buff));
               }
               break;
            }

         case CELL0_MINMAX:
         case CELL1_MINMAX:
         case CELL2_MINMAX:
         case CELL3_MINMAX:
            {
               int cell = int(page) - int(CELL0);
               for(int i = 0; i < 2; i++) {
                  lcd.setCursor(0, i);
                  lcd.print("X"); lcd.print(cell + i);
                  lcd.print(" ");
                  lcd.print(dtostrf(pinVolts[cell + i].max, 4, 2, buff));

                  lcd.print("  N"); lcd.print(cell + i);
                  lcd.print(" ");
                  lcd.print(dtostrf(pinVolts[cell + i].min, 4, 2, buff));
               }
               break;
            }

         default:
            lcd.setCursor(0, 0);
            sprintf(buff, "PAGE:         %02d", page);
            lcd.print(buff);
            lcd.setCursor(0, 1);
            lcd.print("                ");
            break;
      }
   }
}
