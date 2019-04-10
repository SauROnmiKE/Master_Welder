#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>

LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
DS3231 rtc(SDA, SCL);

// Rotary Encoder Related
const int clkPin = 8; // A
const int dataPin = 9; // B
const int swPin = 10; // Button
int getEncoderMovement();

const int weldBtn = 13;
const int weldOutput = 11;

long buttonStartTime = 0;
long buttonEndTime = 0;
long charTimeStart = 0;
long charTimeEnd = 0;
int menuSwitchTime = 2; // Seconds of constant button push to switch modes
int menuChoice = 1;
int oldMenuChoice = 1;
int pulseChoice = 1;

bool optionsMode = false;
bool weldBtnDown = false;
bool characterVisible = false;
bool enterMode = false;
bool modeChanged = false; // to prevent modes from changing when the encoder button is constantly pressed
bool oneTwoPulse = false;

//*******RTC-RELATED VARIABLES*******//
String currentClock, currentHours, currentMinutes, currentSeconds, currentDate, currentDay, currentMonth, currentYear;
String timeString, hoursString, minutesString, secondsString, hoursS, minutesS, secondsS, dateS;
float currentTemperature, temperature;
//***********************************//

// Pulse Related
long pulseLength = 100; // 1ms min
int pulses = 2; // 0 - 5 (0 = One-Two Pulse)
int firstPulse = 100; // First pulse ONLY for one-two pulse option
int secondPulse = 100; // Second pulse ONLY for one-two pulse option
int delayBetweenPulses = 50; // 50ms min - 500ms max


void intro();
void initializeClock();
void menu();
void updateTime();
void createCharacter();
void showCorrectDisplay(int menuChoice, bool displacement);
void showSetChoice();
void correctDisplay();

byte fullSquare[8] = {
  B01000,
  B00100,
  B00010,
  B11111,
  B11111,
  B00010,
  B00100,
  B01000
};

void setup() {
  pinMode(clkPin, INPUT);
  pinMode(dataPin, INPUT);
  pinMode(swPin, INPUT);
  pinMode(weldBtn, INPUT);
  pinMode(weldOutput, OUTPUT);
  digitalWrite(swPin, HIGH);
  lcd.begin(20, 4);
  intro();
  menu();
}

void loop() {
start:
  createCharacter();
  // Weld-Mode
  if (!optionsMode) {
    if (digitalRead(swPin) == LOW) {
      while (modeChanged) { // To prevent the modes from constantly changing. The user must leave the button if he/she wants to change modes again
        if (digitalRead(swPin) == HIGH) {
          modeChanged = false;
          goto start;
        }
        updateTime();
      }
      buttonStartTime = rtc.getUnixTime(rtc.getTime());
      lcd.setCursor(19, 0);
      lcd.print("!");
      while (digitalRead(swPin) == LOW) { // Going to options-mode
        buttonEndTime = rtc.getUnixTime(rtc.getTime());
        updateTime();
        if ((buttonEndTime - buttonStartTime) >= menuSwitchTime) {
          optionsMode = true;
          modeChanged = true;
          menu();
          showCorrectDisplay(menuChoice, false);
          goto start;
        }
      }
      lcd.setCursor(19, 0);
      lcd.print(" ");
    }

    // TODO: CLOCK DOESN"T WORK WHEN WELDING IS IN PROCESS! (CHANGE IT BY USING TIMERS
    if (digitalRead(weldBtn) == HIGH) { // WELD BUTTON - Comment if button is not connected
      delay(100);
      while (digitalRead(weldBtn) == HIGH) {
        if (!weldBtnDown) {
          if (oneTwoPulse) {
            digitalWrite(weldOutput, HIGH);
            delay(firstPulse);
            digitalWrite(weldOutput, LOW);
            delay(delayBetweenPulses);
            digitalWrite(weldOutput, HIGH);
            delay(secondPulse);
            digitalWrite(weldOutput, LOW);
            updateTime();
          }
          else if (pulses > 1) {
            for (int i = 1; i <= pulses; i++) {
              digitalWrite(weldOutput, HIGH);
              delay(pulseLength);
              digitalWrite(weldOutput, LOW);
              delay(delayBetweenPulses);
              updateTime();
            }
          }
          else {
            digitalWrite(weldOutput, HIGH);
            delay(pulseLength);
            digitalWrite(weldOutput, LOW);
            updateTime();
          }
          weldBtnDown = true;
        }
        updateTime();
      }
    }
    weldBtnDown = false;

    updateTime();
  } else {
    // Options-Mode
    int encMove = 0;
    encMove = getEncoderMovement();
    menuChoice += encMove;
    if (menuChoice > 4) { // Check if menuChoice is out of bounds after the movement.
      menuChoice = 1;
    } else if (menuChoice < 1) {
      menuChoice = 4;
    }

    // DelayBetweenPulses is not used on one pulse, so we skip that setting when we only have a single pulse
    if ((pulses == 1) && (menuChoice == 3) && (oldMenuChoice == 2)) {
      menuChoice = 4;
    } else if ((pulses == 1) && (menuChoice == 3) && (oldMenuChoice == 4)) {
      menuChoice = 2;
    }

    if (menuChoice != oldMenuChoice) {
      menu(); // Put every line back in place
      oldMenuChoice = menuChoice;
    }

    showCorrectDisplay(menuChoice, false);
    updateTime();

    if (digitalRead(swPin) == LOW) { // Going to weld-mode or options mode
      while (modeChanged) { // To prevent the modes from constantly changing. The user must leave the button if he/she wants to change modes again
        if (digitalRead(swPin) == HIGH) {
          modeChanged = false;
          goto start;
        }
        updateTime();
      }
      buttonStartTime = rtc.getUnixTime(rtc.getTime());
      lcd.setCursor(19, 0);
      lcd.print("!");
      while (digitalRead(swPin) == LOW) {
        buttonEndTime = rtc.getUnixTime(rtc.getTime());
        updateTime();
        if ((buttonEndTime - buttonStartTime) >= menuSwitchTime) {
          optionsMode = false;
          modeChanged = true;
          menu();
          goto start;
        }
      }
      lcd.setCursor(19, 0); // Deletes "!" when entering Enter Mode
      lcd.print(" ");

      enterMode = true; // If the user presses the encoder's button and then leaves it before the X sec. mark, the user will be entered in enterMode and can now change the values
      modeChanged = true;
      if (enterMode) {
        int encPosition = 0;
        int encOldPosition = 0;
        menu();
        showSetChoice();

        lcd.setCursor(3, menuChoice - 1); // The user will see a lone letter until the correctDisplay is shown, this fixes that.
        lcd.print(" ");

        while (enterMode) {
          updateTime();

          // One-Two Pulse will be set differently, because the input method is different than the other settings
          if ((menuChoice == 1) && (oneTwoPulse)) {
            lcd.setCursor(3, 0); // For some reason the first ":" disappears
            lcd.print(":");

            while (true) {
              showSetChoice();
              showCorrectDisplay(menuChoice, false);

              encMove = getEncoderMovement();
              encPosition += encMove;

              if (digitalRead(swPin) == LOW) {
                delay(125);
                if (digitalRead(swPin) == LOW) {
                  pulseChoice++;
                }

                // Erasing the arrow when the user changes the second pulse
                if (pulseChoice == 2) {
                  lcd.setCursor(0, 0);
                  lcd.print(" ");
                } else if (pulseChoice == 3) {
                  pulseChoice = 1;
                  enterMode = false;
                  modeChanged = true;
                  menu();
                  goto start;
                }
              }

              if (encPosition > encOldPosition) {
                switch (pulseChoice) {
                  case 1: {
                      if (firstPulse == 9995) { // 9995ms is the maximum
                        firstPulse = 1;
                      } else {
                        if (firstPulse < 10) {
                          firstPulse++;
                        } else {
                          firstPulse += 5;
                        }
                      }
                      correctDisplay();
                      break;
                    }

                  case 2: {
                      if (secondPulse == 9995) { // 9995ms is the maximum
                        secondPulse = 1;
                      } else {
                        if (secondPulse < 10) {
                          secondPulse++;
                        } else {
                          secondPulse += 5;
                        }
                      }
                      correctDisplay();
                      break;
                    }
                }
              } else if (encPosition < encOldPosition) {
                switch (pulseChoice) {
                  case 1: {
                      if (firstPulse == 1) { // 9995ms is the maximum
                        firstPulse = 9995;
                      } else {
                        if (firstPulse <= 10) {
                          firstPulse--;
                        } else {
                          firstPulse -= 5;
                        }
                      }
                      correctDisplay();
                      break;
                    }

                  case 2: {
                      if (secondPulse == 1) { // 9995ms is the maximum
                        secondPulse = 9995;
                      } else {
                        if (secondPulse <= 10) {
                          secondPulse--;
                        } else {
                          secondPulse -= 5;
                        }
                      }
                      correctDisplay();
                      break;
                    }
                }
              }
              encOldPosition = encPosition;
              updateTime();
            }
          }

          showCorrectDisplay(menuChoice, true);

          encMove = getEncoderMovement();
          encPosition += encMove;
          if (encPosition > encOldPosition) {
            switch (menuChoice) {
              case 1: {
                  if (pulseLength == 999995) {
                    pulseLength = 1; // 999995 ms is the maximum limit for pulse length (won't be used anyway, what to do...)
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                    correctDisplay();
                  } else {
                    if (pulseLength < 10) {
                      pulseLength += 1;
                    } else {
                      pulseLength += 5;
                    }
                    showSetChoice();
                    correctDisplay();
                  }
                  break;
                }
              case 2: {
                  if (pulses == 5) {
                    pulses = 0;
                    oneTwoPulse = true;
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                  } else {
                    pulses++;
                    if (pulses > 0) {
                      oneTwoPulse = false;
                    }
                    showSetChoice();
                  }
                  break;
                }
              case 3: {
                  if (delayBetweenPulses == 500) {
                    delayBetweenPulses = 50; // Maximum limit for delay between pulses
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                  } else {
                    delayBetweenPulses += 50;
                    showSetChoice();
                  }
                  correctDisplay();
                  break;
                }
              case 4: {
                  // TODO: Time set
                }
            }
          } else if (encPosition < encOldPosition) {
            switch (menuChoice) {
              case 1: {
                  if (pulseLength == 1) {
                    pulseLength = 999995;
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                    correctDisplay();
                  } else {
                    if (pulseLength <= 10) {
                      pulseLength -= 1;
                    } else {
                      pulseLength -= 5;
                    }
                    showSetChoice();
                    correctDisplay();
                  }
                  break;
                }
              case 2: {
                  if (pulses == 0) {
                    pulses = 5;
                    oneTwoPulse = false;
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                  } else {
                    pulses--;
                    if (pulses == 0) {
                      oneTwoPulse = true;
                    }
                    showSetChoice();
                  }
                  break;
                }
              case 3: {
                  if (delayBetweenPulses == 50) {
                    delayBetweenPulses = 500;
                    lcd.setCursor(3, menuChoice - 1);
                    lcd.print(" ");
                    showSetChoice();
                  } else {
                    delayBetweenPulses -= 50;
                    showSetChoice();
                  }
                  correctDisplay();
                  break;
                }
              case 4: {
                  // TODO: Set time/date
                  break;
                }
            }
          }

          encOldPosition = encPosition;

          if (digitalRead(swPin) == LOW) { // Exiting enter-mode
            delay(100);
            if (digitalRead(swPin) == LOW) {
              enterMode = false;
              modeChanged = true;
              menu();
              goto start;
            }
          }
        }

      }
    }
  }


}

void intro() {
  lcd.setCursor(2, 1);
  lcd.print("Mike Spot Welder");
  lcd.setCursor(9, 2);
  lcd.print("v2");
  delay(3500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting clock");
  delay(75);
  lcd.print(".");
  delay(75);
  lcd.print(".");
  delay(75);
  lcd.print(".");

  initializeClock();

  lcd.setCursor(0, 1);
  lcd.print("Done!");
  delay(1000);

  lcd.clear();
  lcd.setCursor(6, 0);
  lcd.print("(c) 2018");
  lcd.setCursor(5, 1);
  lcd.print("SauROnmiKE");
  lcd.setCursor(9, 2);
  lcd.print("&");
  lcd.setCursor(5, 3);
  lcd.print("airgeorge");
  delay(2000);
}

// Initializes clock and gets the current values
void initializeClock() {
  rtc.begin();

  currentTemperature = rtc.getTemp();
  currentDate = rtc.getDateStr();
  currentClock = rtc.getTimeStr();
  timeString = rtc.getTimeStr();
  currentHours = timeString.substring(0, 2);
  currentMinutes = timeString.substring(3, 5);
  currentSeconds = timeString.substring(6, 8);
}

void menu() {
  lcd.clear();

  if (pulses == 0) {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("P1: ");
    lcd.print(firstPulse);
    lcd.setCursor(10, 0);
    lcd.print("P2: ");
    lcd.print(secondPulse);
    lcd.setCursor(1, 1);
    lcd.print("One-Two Pulse");
    lcd.setCursor(1, 2);
    lcd.print("Delay: ");
    lcd.print(delayBetweenPulses);
    lcd.print("ms");
  } else {
    lcd.setCursor(1, 0);
    lcd.print("Pulse: ");
    lcd.print(pulseLength);
    lcd.print("ms");
    lcd.setCursor(1, 1);
    lcd.print(pulses);
    if (pulses > 1) {
      lcd.print(" Pulses");
      lcd.setCursor(1, 2);
      lcd.print("Delay: ");
      lcd.print(delayBetweenPulses);
      lcd.print("ms");
    } else {
      lcd.print(" Pulse");
      lcd.setCursor(1, 2);
      lcd.print("               "); // Clear line
    }
  }

  lcd.setCursor(1, 3);
  lcd.print(rtc.getDateStr());
  lcd.print(" ");
  lcd.print(currentHours);
  lcd.print(":");
  lcd.print(currentMinutes);
  lcd.print(":");
  lcd.print(currentSeconds);
}

void updateTime() {
  if (currentClock != rtc.getTimeStr()) {
    timeString = rtc.getTimeStr();
    hoursS = timeString.substring(0, 2);
    minutesS = timeString.substring(3, 5);
    secondsS = timeString.substring(6, 8);

    lcd.setCursor(18, 3);
    lcd.print(secondsS);

    if (currentMinutes != minutesS) {
      lcd.setCursor(15, 3);
      lcd.print(minutesS);
      currentMinutes = minutesS;
    }

    if (currentHours != hoursS) {
      lcd.setCursor(12, 3);
      lcd.print(hoursS);
      currentHours = hoursS;
    }

    dateS = rtc.getDateStr();
    delay(10);
    if (currentDate != dateS) {
      currentDate = dateS;
      lcd.setCursor(1, 3);
      lcd.print(dateS);
    }
  }
}

void createCharacter() {
  lcd.createChar(0, fullSquare);
}

void showCorrectDisplay(int menuChoice, bool displacement) {
  switch (menuChoice) {
    case 1: { // Only the correctDisplay will move.
        if (enterMode) { // Only show the arrow on these places if enterMode is on. Otherwise the arrow won't show at all.
          if (oneTwoPulse) {
            switch (pulseChoice) {
              case 1: {
                  if (!characterVisible) {
                    charTimeEnd = rtc.getUnixTime(rtc.getTime());
                    if ((charTimeEnd - charTimeStart) >= 1) {
                      charTimeStart = rtc.getUnixTime(rtc.getTime());
                      lcd.setCursor(0, 0);
                      lcd.write(byte(0));
                      characterVisible = true;
                    }
                  } else {
                    charTimeEnd = rtc.getUnixTime(rtc.getTime());
                    if ((charTimeEnd - charTimeStart) >= 1) {
                      charTimeStart = rtc.getUnixTime(rtc.getTime());
                      lcd.setCursor(0, 0);
                      lcd.print(" ");
                      characterVisible = false;
                    }
                  }
                  break;
                }
              case 2: {
                  if (!characterVisible) {
                    charTimeEnd = rtc.getUnixTime(rtc.getTime());
                    if ((charTimeEnd - charTimeStart) >= 1) {
                      charTimeStart = rtc.getUnixTime(rtc.getTime());
                      lcd.setCursor(9, 0);
                      lcd.write(byte(0));
                      characterVisible = true;
                    }
                  } else {
                    charTimeEnd = rtc.getUnixTime(rtc.getTime());
                    if ((charTimeEnd - charTimeStart) >= 1) {
                      charTimeStart = rtc.getUnixTime(rtc.getTime());
                      lcd.setCursor(9, 0);
                      lcd.print(" ");
                      characterVisible = false;
                    }
                  }
                  break;
                }
            }
          }
        } else {
          if (!characterVisible) {
            charTimeEnd = rtc.getUnixTime(rtc.getTime());
            if ((charTimeEnd - charTimeStart) >= 1) {
              charTimeStart = rtc.getUnixTime(rtc.getTime());
              if (displacement) {
                lcd.setCursor(3, 0);
              } else {
                lcd.setCursor(0, 0);
              }
              lcd.write(byte(0));
              characterVisible = true;
            }
          }
          else {
            charTimeEnd = rtc.getUnixTime(rtc.getTime());
            if ((charTimeEnd - charTimeStart) >= 1) {
              charTimeStart = rtc.getUnixTime(rtc.getTime());
              if (displacement) {
                lcd.setCursor(3, 0);
              } else {
                lcd.setCursor(0, 0);
              }
              lcd.print(" ");
              characterVisible = false;
            }
          }
        }
        break;
      }

    case 2: {
        lcd.setCursor(0, 1);
        if (!characterVisible) {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(3, 1);
            } else {
              lcd.setCursor(0, 1);
            }
            lcd.write(byte(0));
            characterVisible = true;
          }
        }
        else {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(3, 1);
            } else {
              lcd.setCursor(0, 1);
            }
            lcd.print(" ");
            characterVisible = false;
          }
        }
        break;
      }

    case 3: {
        lcd.setCursor(0, 2);
        if (!characterVisible) {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(3, 2);
            } else {
              lcd.setCursor(0, 2);
            }
            lcd.write(byte(0));
            characterVisible = true;
          }
        }
        else {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(3, 2);
            } else {
              lcd.setCursor(0, 2);
            }
            lcd.print(" ");
            characterVisible = false;
          }
        }
        break;
      }
    case 4: {
        lcd.setCursor(0, 3);
        if (!characterVisible) {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(0, 3);
            } else {
              lcd.setCursor(0, 3);
            }
            lcd.write(byte(0));
            characterVisible = true;
          }
        }
        else {
          charTimeEnd = rtc.getUnixTime(rtc.getTime());
          if ((charTimeEnd - charTimeStart) >= 1) {
            charTimeStart = rtc.getUnixTime(rtc.getTime());
            if (displacement) {
              lcd.setCursor(0, 3);
            } else {
              lcd.setCursor(0, 3);
            }
            lcd.print(" ");
            characterVisible = false;
          }
        }
        break;
      }
  }
}

int getEncoderMovement() {
  static int aLastState = HIGH; // a is related to clkPin
  static int bLastState = HIGH; // b is related to dataPin
  int movement = 0;

  int aNewState = digitalRead(clkPin);
  int bNewState = digitalRead(dataPin);
  if ((aNewState != aLastState) || (bNewState != bLastState)) {
    if (aLastState == HIGH && aNewState == LOW) {
      movement = (bLastState * 2 - 1); // movement will be either 1 or -1, according to the value of bLastState
    }
  }
  aLastState = aNewState;
  bLastState = bNewState;
  return movement;
}

void showSetChoice () {
  switch (menuChoice) {
    case 1: {
        if (oneTwoPulse) {
          lcd.setCursor(5, 0);
          lcd.print(firstPulse);
          lcd.setCursor(14, 0);
          lcd.print(secondPulse);
        } else {
          lcd.setCursor(0, 0);
          lcd.print("SET");
          lcd.setCursor(4, 0);
          lcd.print(" Pulse: ");
          lcd.print(pulseLength);
          lcd.print("ms");
        }
        break;
      }

    case 2: {
        lcd.setCursor(12, 1); // Erasing extra characters from One-Two Pulse (if any)
        lcd.print("      ");
        lcd.setCursor(0, 1);
        lcd.print("SET");
        lcd.setCursor(4, 1);
        lcd.print(" ");

        if (pulses != 0) {
          lcd.print(pulses);

          if (pulses > 1) {
            lcd.print(" Pulses");
          } else if (pulses == 1) {
            lcd.print(" Pulse");
          }
        } else {
          lcd.print("One-Two Pulse");
        }
        break;
      }

    case 3: {
        lcd.setCursor(0, 2);
        lcd.print("SET");
        lcd.setCursor(4, 2);
        lcd.print(" Delay: ");
        lcd.print(delayBetweenPulses);
        lcd.print("ms");
      }
  }
}

// Corrects what the LCD displays, e.g. when a variable changes from 100ms to 95ms, the LCD displays 95mss. This function corrects that.
void correctDisplay () {
  // TOOD: Correct display for one-two pulse
  if (oneTwoPulse) {
    if ((firstPulse == 1) || (firstPulse == 9)) {
      lcd.setCursor(6 , 0);
      lcd.print("   ");
    } else if (firstPulse == 95) {
      lcd.setCursor(7, 0);
      lcd.print(" ");
    } else if (firstPulse == 995) {
      lcd.setCursor(8, 0);
      lcd.print(" ");
    }

    if ((secondPulse == 1) || (secondPulse == 9)) {
      lcd.setCursor(15 , 0);
      lcd.print("   ");
    } else if (secondPulse == 95) {
      lcd.setCursor(16, 0);
      lcd.print(" ");
    } else if (secondPulse == 995) {
      lcd.setCursor(17, 0);
      lcd.print(" ");
    }
    return;
  }

  if ((pulseLength == 1) || (pulseLength == 9)) { // Aesthetic changes
    lcd.setCursor(15, 0);
    lcd.print("     ");
  } else if (pulseLength == 95) {
    lcd.setCursor(16, 0);
    lcd.print(" ");
  } else if (pulseLength == 995) {
    lcd.setCursor(17, 0);
    lcd.print(" ");
  } else if (pulseLength == 9995) {
    lcd.setCursor(18, 0);
    lcd.print(" ");
  } else if (pulseLength == 99995) {
    lcd.setCursor(19, 0);
    lcd.print(" ");
  }

  if (pulses == 1) {
    lcd.setCursor(12, 1);
    lcd.print(" ");
  }

  if (delayBetweenPulses == 50) {
    lcd.setCursor(16 , 2);
    lcd.print(" ");
  }
}

