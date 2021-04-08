#ifndef PAGE_HPP
#define PAGE_HPP

#include <Adafruit_RGBLCDShield.h>
#include <string.h>

extern Adafruit_RGBLCDShield lcd;

inline void printDouble(double value, int width, int maxDecimal = -1) {
   if(maxDecimal < 0) maxDecimal = width - 1;

   int wholeDigits = int(log10(abs(value))) + 1;
   if(value < 0) wholeDigits += 1;

   char buff[17];
   lcd.print(dtostrf(value, width, constrain(abs(width) - wholeDigits - 1, 0, maxDecimal), buff));
}

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
   VoltageVariation *var;
   bool showMax;

   VariationPage(VoltageVariation *_var, bool _showMax) : var(_var), showMax(_showMax) {}

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

#endif
