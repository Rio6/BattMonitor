#include <Arduino.h>
#include <Adafruit_RGBLCDShield.h>
#include <string.h>
#include <math.h>

constexpr double VREF = 5;

Adafruit_RGBLCDShield lcd;

void printDouble(double value, int width, int maxDecimal = -1) {
   if(maxDecimal < 0) maxDecimal = width - 1;

   int wholeDigits = int(log10(abs(value))) + 1;
   if(value < 0) wholeDigits += 1;

   char buff[17];
   lcd.print(dtostrf(value, width, constrain(abs(width) - wholeDigits - 1, 0, maxDecimal), buff));
}

struct Voltage {
   double min;
   double max;
   double now;

   Voltage() {
      reset();
   }

   void reset() {
      min = INFINITY;
      max = 0;
      now = 0;
   }

   void update(double volt) {
      now = volt;
      if(now > max)
         max = now;
      if(now < min)
         min = now;
   }
};

struct PinVoltage : Voltage {
   int pin;

   PinVoltage(int _pin): pin(_pin) {}

   void update() {
      double volt = (analogRead(pin) + 0.5) / 1024 * VREF;
      Voltage::update(volt);
   }
};

struct Page {
   Page *up = nullptr, *down = nullptr, *left = nullptr, *right = nullptr;
   virtual void draw() = 0;
};

struct OnePage : Page {
   char line[17] = {0};
   const double *value;

   OnePage(const char *_line, const double *_value) : value(_value) {
      sprintf(line, "%-16.16s", _line);
   }

   void draw() {
      lcd.setCursor(0, 0);
      lcd.print(line);
      lcd.setCursor(0, 1);
      printDouble(*value, 16, 4);
   }
};

struct TwoPage : Page {
   char lines[2][9] = {0};
   const double *values[2];

   TwoPage(const char *line0, const double *value0, const char *line1, const double *value1) {
      sprintf(lines[0], "%-8.8s", line0);
      sprintf(lines[1], "%-8.8s", line1);
      values[0] = value0;
      values[1] = value1;
   }

   void draw() {
      for(size_t i = 0; i < 2; i++) {
         lcd.setCursor(0, i);
         lcd.print(lines[i]);
         printDouble(*values[i], 8, 4);
      }
   }
};

struct ThreePage : Page {
   char lines0[2][9] = {0};
   char lines1[9] = {0};
   const double *values[3];

   ThreePage(const char *line0, const double *value0, const char *line1, const double *value1, const char *line2, const double *value2) {
      sprintf(lines0[0], "%-3.3s", line0);
      sprintf(lines0[1], "%-3.3s", line1);
      sprintf(lines1, "%-8.8s", line2);
      values[0] = value0;
      values[1] = value1;
      values[2] = value2;
   }

   void draw() {
      lcd.setCursor(0, 0);
      for(size_t i = 0; i < 2; i++) {
         lcd.print(lines0[i]);
         printDouble(*values[i], 5);
      }

      lcd.setCursor(0, 1);
      lcd.print(lines1);
      printDouble(*values[2], 8, 4);
   }
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

Page *page;
Voltage totalVolt;
Voltage variation;

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
   Page *mainPage = new TwoPage("TOTAL", &totalVolt.now, "VAR", &variation.now);
   Page *mainMinMax = new TwoPage("MAX", &totalVolt.max, "MIN", &totalVolt.min);
   Page *varMinMax = new OnePage("         MAX VAR", &variation.max);

   mainPage->right = mainMinMax;
   mainMinMax->right = varMinMax;
   varMinMax->right = mainPage;

   varMinMax->left = mainMinMax;
   mainMinMax->left = mainPage;
   mainPage->left = varMinMax;

   Page *cellPage = mainPage;
   for(size_t i = 0; i < numPins; i += 2) {
      char line0[6] = {0};
      char line1[6] = {0};

      sprintf(line0, "CELL%d", i+1);
      sprintf(line1, "CELL%d", i+2);
      Page *page = new TwoPage(line0, &pinVolts[i].now, line1, &pinVolts[i+1].now);

      sprintf(line0, "MAX%d", i+1);
      sprintf(line1, "MAX%d", i+2);
      Page *max = new TwoPage(line0, &pinVolts[i].max, line1, &pinVolts[i+1].max);

      sprintf(line0, "MIN%d", i+1);
      sprintf(line1, "MIN%d", i+2);
      Page *min = new TwoPage(line0, &pinVolts[i].min, line1, &pinVolts[i+1].min);

      page->right = max;
      max->right = min;
      min->right = page;

      page->left = min;
      min->left = max;
      max->left = page;

      if(i == 0) {
         mainPage->down = mainMinMax->down = varMinMax->down = page;
         page->up = max->up = min->up = mainPage;
      } else {
         cellPage->down = page;
         cellPage->left->down = min;
         cellPage->right->down = max;

         page->up = cellPage;
         max->up = cellPage->right;
         min->up = cellPage->left;
      }

      cellPage = page;
   }

   page = mainPage;
}

void loop() {
   double total = 0;
   Voltage tally;

   for(size_t i = 0; i < numPins; i++) {
      pinVolts[i].update();
      total += pinVolts[i].now;
      tally.update(pinVolts[i].now);
   }

   totalVolt.update(total);
   variation.update(tally.max - tally.min);

   // Buttons
   static long lastPrint = 0;

   // Lazy debouncing, returns non-zero when button is first become pressed, zero afterwards
   #define DEBOUNCE(btn) (buttons & (btn) && ~debounce & (btn))
   static uint8_t debounce = 0;

   uint8_t buttons = lcd.readButtons();

   if(DEBOUNCE(BUTTON_DOWN) && page->down)
      page = page->down;
   
   if(DEBOUNCE(BUTTON_UP) && page->up)
      page = page->up;
   
   if(DEBOUNCE(BUTTON_LEFT) && page->left)
      page = page->left;
   
   if(DEBOUNCE(BUTTON_RIGHT) && page->right)
      page = page->right;
   
   if(buttons ^ debounce)
      lastPrint = 0;

   debounce = buttons;

   // Print to LCD
   long now = millis();
   if(now - lastPrint > 1000) {
      lastPrint = now;
      page->draw();
   }
}
