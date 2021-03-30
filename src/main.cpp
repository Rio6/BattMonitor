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

struct CellValue {
   size_t cell;
   double value;

   void set(size_t _cell, double _value) {
      cell = _cell;
      value = _value;
   }
};

struct Variation {
   CellValue minNow, maxNow;
   CellValue minMax, maxMax;

   double now, max = 0;

   Variation() {
      start();
   }

   void start() {
      minNow.set(0, INFINITY);
      maxNow.set(0, 0);
   }

   void update(size_t cell, double volt) {
      if(volt < minNow.value) {
         minNow.set(cell, volt);
      }
      if(volt > maxNow.value) {
         maxNow.set(cell, volt);
      }
   }

   void finish() {
      now = maxNow.value - minNow.value;
      if(now > max) {
         max = now;
         minMax = minNow;
         maxMax = maxNow;
      }
   }
};

struct Page {
   Page *up = nullptr, *down = nullptr, *left = nullptr, *right = nullptr;
   virtual void draw() = 0;
};

struct NormalPage : Page {
   char lines[2][9] = {0};
   const double *values[2];

   NormalPage(const char *line0, const double *value0, const char *line1, const double *value1) {
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

struct VariationPage : Page {
   Variation *var;
   bool showMax;

   VariationPage(Variation *_var, bool _showMax) : var(_var), showMax(_showMax) {}

   void draw() {
      const CellValue &max = showMax ? var->maxMax : var->maxNow;
      const CellValue &min = showMax ? var->minMax : var->minNow;

      lcd.setCursor(0, 0);

      if(showMax) {
         lcd.print("MCV     ");
      } else {
         lcd.print("VAR     ");
      }

      printDouble(max.value - min.value, 8, 4);

      lcd.setCursor(0, 1);

      char buff[3];
      sprintf(buff, "C%d  ", min.cell+1);
      lcd.print(buff);
      printDouble(min.value, 4);

      sprintf(buff, " C%d ", max.cell+1);
      lcd.print(buff);
      printDouble(max.value, 4);
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
Variation variation;

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

      cellPage = page;
   }

   page = mainPage;
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
