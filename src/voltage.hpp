#ifndef VOLTAGE_HPP
#define VOLTAGE_HPP

#include <Arduino.h>
#include <math.h>

constexpr double VREF = 5;

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
