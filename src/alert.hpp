// Types and behaviours about alerts
#ifndef ALERT_HPP
#define ALERT_HPP

#include "page.hpp"
#include "voltage.hpp"
#include "config.h"

// General alert class, contains an associated page to view and a trigger state
struct Alert {
   Page *alertPage;
   bool triggered = false;

   Alert(Page *_alertPage) : alertPage(_alertPage) {}

   virtual void update() = 0;
   virtual void reset() = 0;
};

// Voltage alert, triggers when its voltage os above or below certain level
struct VoltageAlert : Alert {
   Voltage *volt;

   double minVolt;
   double maxVolt;

   VoltageAlert(Voltage *_volt, Page *_alertPage, double _minVolt, double _maxVolt)
      : Alert(_alertPage), volt(_volt), minVolt(_minVolt), maxVolt(_maxVolt) {}

   void update() {
      if(triggered || !volt) return;
      if(volt->max > maxVolt || volt->min < minVolt) {
         triggered = true;
      }
   }

   void reset() {
      if(volt) {
         volt->reset();
      }
      triggered = false;
   }
};

// Variation alert, triggers when its variation is above certain level
struct VariationAlert : Alert {
   double max = MAX_VOLTAGE_VARIATION;

   VoltageVariation *variation;

   VariationAlert(VoltageVariation *_var, Page *_alertPage) : Alert(_alertPage), variation(_var) {}

   void update() {
      if(triggered || !variation) return;
      if(variation->max > max) {
         triggered = true;
      }
   }

   void reset() {
      if(variation) {
         variation->reset();
      }
      triggered = false;
   }
};

#endif
