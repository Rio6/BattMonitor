// Different kinds of pages to show on LCD, they're all nodes for linked list
// whith 4 nodes: up, down, left, right which correspinds to the next page to
// go to when the navigation buttons are pressed.
#ifndef PAGE_HPP
#define PAGE_HPP

#include <Adafruit_RGBLCDShield.h>
#include <string.h>

extern Adafruit_RGBLCDShield lcd;

// Helper function to print a double to screen
inline void printDouble(double value, int width, int maxDecimal = -1) {
   if(maxDecimal < 0) maxDecimal = width - 1;

   int wholeDigits = int(log10(abs(value))) + 1;
   if(value < 0) wholeDigits += 1;

   char buff[17];
   lcd.print(dtostrf(value, width, constrain(abs(width) - wholeDigits - 1, 0, maxDecimal), buff));
}

// Base class of a page
struct Page {
   Page *up = nullptr, *down = nullptr, *left = nullptr, *right = nullptr;
   virtual void draw() = 0;
};

// Normal page contains 2 lines, each line can display a number
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

// Voltage page shows the cell number, current, min, and max values of a voltage object
struct VoltagePage : Page {
   size_t cellNum;
   Voltage *volt;

   VoltagePage(size_t _cellNum, Voltage *_volt) : cellNum(_cellNum), volt(_volt) {}

   void draw() {
      lcd.setCursor(0, 0);
      lcd.print("CELL");
      lcd.print(cellNum+1);
      lcd.print("   ");
      printDouble(volt->now, 8, 4);

      lcd.setCursor(0, 1);
      lcd.print("N/X");
      printDouble(volt->min, 6, 3);
      lcd.print(" ");
      printDouble(volt->max, 6, 3);
   }
};

// Variation page shows the current and max cell voltage variation
struct VariationPage : Page {
   VoltageVariation *var;
   bool showMax;

   VariationPage(VoltageVariation *_var, bool _showMax) : var(_var), showMax(_showMax) {}

   void draw() {
      const CellValue &max = showMax ? var->maxCellAtMaxVar : var->maxCellNow;
      const CellValue &min = showMax ? var->minCellAtMaxVar : var->minCellNow;

      lcd.setCursor(0, 0);
      lcd.print(showMax ? "MCV     " : "VAR     ");

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
