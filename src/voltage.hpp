// These are some helper structs to keep track of min and max voltage
#ifndef VOLTAGE_HPP
#define VOLTAGE_HPP

#include "config.h"

#include <Arduino.h>
#include <math.h>

static double realTotal(double divided) {
      return divided * (TOTAL_VOLT_R2 + TOTAL_VOLT_R1) / (TOTAL_VOLT_R2);
}

// A voltage class keeps track of max, min, and current value
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

// A voltage class that reads voltage from specified pin
struct PinVoltage : Voltage {
   int pin;

   PinVoltage(int _pin): pin(_pin) {}

   void update() {
      double volt = (analogRead(pin) + 0.5) / 1024 * VREF;
      Voltage::update(volt);
   }
};

struct TotalPinVoltage : Voltage {
   int pin;

   TotalPinVoltage(int _pin): pin(_pin) {}

   void update() {
      double volt = realTotal((analogRead(pin) + 0.5) / 1024 * VREF);
      Voltage::update(volt);
   }
};

// Helper class for VoltageVariation to keep track of max and min cell
struct CellValue {
   size_t cell;
   double value;

   void set(size_t _cell, double _value) {
      cell = _cell;
      value = _value;
   }
};

// Voltage variation class that keeps track of the cell variation, and the
// cells of when that maximum variation is recorded
struct VoltageVariation {
   CellValue minCellNow, maxCellNow;
   CellValue minCellAtMaxVar, maxCellAtMaxVar;

   double now, max = 0;

   VoltageVariation() {
      start();
   }

   void reset() {
      max = 0;
   }

   void start() {
      minCellNow.set(0, INFINITY);
      maxCellNow.set(0, 0);
   }

   void update(size_t cell, double volt) {
      if(volt < minCellNow.value) {
         minCellNow.set(cell, volt);
      }
      if(volt > maxCellNow.value) {
         maxCellNow.set(cell, volt);
      }
   }

   void finish() {
      now = maxCellNow.value - minCellNow.value;
      if(now > max) {
         max = now;
         minCellAtMaxVar = minCellNow;
         maxCellAtMaxVar = maxCellNow;
      }
   }
};

#endif
