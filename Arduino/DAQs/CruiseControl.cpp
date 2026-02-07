#include <Arduino.h>
#include "CruiseControl.h"
#include "SM_16DIGIN.h"
#include "Globals.h"
#include "debug.h"

void cruise() {
  // DB_PRINTLN("Enter Cruise");

  // int cruise_on = 0;

  // Update pins
  int cruise_on = (digitalPins >> DIG_CRUISE_ON - 1) & 1;
  DB_PRINT("cruise_on: ");
  DB_PRINTLN(cruise_on);
  int cruise_set = (digitalPins >> DIG_CRUISE_SET - 1) & 1;
  DB_PRINT("cruise_set: ");
  DB_PRINTLN(cruise_set);
  int cruise_resume = (digitalPins >> DIG_CRUISE_RESUME - 1) & 1;
  DB_PRINT("cruise_resume: ");
  DB_PRINTLN(cruise_resume);
  int cruise_cancel = (digitalPins >> DIG_BRAKE_LIGHT - 1) & 1;
  DB_PRINT("cruise_cancel: ");
  DB_PRINTLN(cruise_cancel);// cruiseActive = 0; cruise off

// cruiseActive = 1; cruise on, but not driving
// cruiseActive = 2; cruise on and driving

  // Update cruise state based on inputs
  if (!cruise_on) {
    // initialize all settings
    cruiseActive = 0;
    // cruiseSpeedActive = 0;
    cruiseSetValue = 0;
    cruiseAccel = 0;
    cruiseDecel = 0;
    return;
  } else if (cruise_on && (cruiseActive != 2)) {
    cruiseActive = 1;
  }
  // Cruise on

    if (cruise_resume && (cruiseActive == 2)) {
      DB_PRINTLN(" resume pin is pressed while cruise is driving");
      cruiseAccel = 1;
      cruiseSetValue = speed;
      // DB_PRINT("cruiseSetValue: ");
      // DB_PRINTLN(cruiseSetValue);
    }

    if (cruise_resume && (cruiseActive == 1) && (cruiseSetValue != 0)) {
      //  set is/has been pressed, cruise is on, but not driving
      // cruiseSpeedActive = 1;
      cruiseActive = 2;   // 2 means cruise is driving
      // DB_PRINT("cruiseActive: ");
      // DB_PRINTLN(cruiseActive);
    }
      
    if (cruise_set && (cruiseActive == 2)) {
      // set is pressed, cruise is on, and driving
      cruiseDecel = 1;
      cruiseSetValue = speed;
      // DB_PRINT("cruiseSetValue: ");
      // DB_PRINTLN(cruiseSetValue);
    }

    if (cruise_set && (cruiseActive == 1)) {
      // set is pressed, cruise is on, but not driving
      speed = 55;
      cruiseActive = 2;
      cruiseSetValue = speed;
      // DB_PRINT("cruiseActive: ");
      // DB_PRINTLN(cruiseActive);
      // DB_PRINT("cruiseSetValue: ");
      // DB_PRINTLN(cruiseSetValue);
    }

    if (cruise_cancel && cruiseActive == 2) {
      // cancel is pressed, cruise is on and driving
      cruiseActive = 1; // Back to cruise on
    }

    DB_PRINT("cruiseActive: ");
    DB_PRINTLN(cruiseActive);
    DB_PRINT("cruiseSetValue: ");
    DB_PRINTLN(cruiseSetValue);

  }
