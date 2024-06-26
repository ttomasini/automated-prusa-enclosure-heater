/*
##########################################################
AUTOMATED HEATING SYSTEM FOR ORIGINAL PRUSA ENCLOSURE
Originally written by user @lars and published on
printables.com in August 2023.
##########################################################

*******************************************************
** YOU NEED TO RENAME configuration-sample.h TO      **
** configuration.h (REMOVE THE -sample) TO USE THIS  **
** SOFTWARE. IT WILL NOT WORK WIHTOUT RENAMING!      **
*******************************************************
**    ALSO: GO AND CONFIGURE YOUR SETUP IN THERE!    **
*******************************************************
*/

// Include neccessary libraries
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Servo.h>
#include <Wire.h>

// Include configuartion file.
// Remember to rename (see above)!
#include <configuration.h>

// Declaration of variables
// Do not set values here, they are set in configuration.h

int target;         // initial target temperature
int maxsettemp;     // target temperature limit
int maxheatertemp;  // max heater temperature
int tolerance;      // set total tolerance for target temperature
int undertemp;      // set temperature limit for detecting faulty/missing sensors
char scale;         // set Celsius or Fahrenheit scale

int caseovertemplimit = 0;
int heaterovertemplimit = 0;

int upperlimit;
int lowerlimit;
int casetemp = 0;
int heatertemp = 0;
char temp_buf[4];
char serial_temp_buf[4];
char serial_mode_buf[5];
char serial_output[17];

// Definitions of names for operating modes. Just to make it more readable.
#define MODE_IDLE 0
#define MODE_HEATING 1
#define MODE_COOLING 2
#define MODE_FAN 3

// Defining operatingMode as MODE_FAN as a little workaround, as it will get
// increased by 1 while starting the first time, so it will actually begin in MODE_IDLE.
byte operatingMode = MODE_FAN;
bool changeMode = false;

// Declaration of states of actuators
bool heaterState = false;
bool fanState = false;
bool servoState = false;
bool closeServo = true;

// Flags for recurring events
bool updateTarget = false;

// Timing related variables
#define TEMPERATURE_DELAY 200  // get Temperature every X millisecond
uint32_t previousMillis_temp = 0;
uint32_t previousMillis_serial = 0;

// Parameters of display
#define i2c_Address 0x3c
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // QT-PY / XIAO
Adafruit_SH1106G display =
  Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Parameters of thermosensors
OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance
DallasTemperature sensors(&oneWire);

// OneWire device addresses of the thermometers as configured in configuration.h
DeviceAddress caseThermometer = { CASE_THERMOMETER_ADDRESS };
DeviceAddress heaterThermometer = { HEATER_THERMOMETER_ADDRESS };

// Variables for button states
bool buttonUpState = false;
bool prevButtonUpState = false;
bool buttonUpPressed = false;

bool buttonDownState = false;
bool prevButtonDownState = false;
bool buttonDownPressed = false;

bool buttonSelectState = false;
bool prevButtonSelectState = false;
bool buttonSelectPressed = true;

// Definition of servo-object and initialization of its position
Servo servo;

// Bitmap-icons for heating, cooling and fan, stored in PROGMEM

const unsigned char PROGMEM logo24_heat[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x60,
  0x0c, 0x18, 0x30, 0x06, 0x0c, 0x18, 0x03, 0x06, 0x0c, 0x03, 0x06, 0x0c,
  0x03, 0x06, 0x0c, 0x03, 0x06, 0x0c, 0x06, 0x0c, 0x18, 0x0c, 0x18, 0x30,
  0x18, 0x30, 0x60, 0x18, 0x30, 0x60, 0x18, 0x30, 0x60, 0x18, 0x30, 0x60,
  0x0c, 0x18, 0x30, 0x06, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x1f, 0xff, 0xfc, 0x1f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char PROGMEM logo24_cool[] = {
  0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0xdb, 0x00, 0x04, 0xff, 0x20,
  0x0c, 0x7e, 0x30, 0x26, 0x3c, 0x64, 0x7e, 0x18, 0x7e, 0x3f, 0x18, 0xfc,
  0x0f, 0x99, 0xf0, 0x7d, 0xdb, 0x7e, 0x30, 0xff, 0x0c, 0x00, 0x7e, 0x00,
  0x00, 0x7e, 0x00, 0x30, 0xff, 0x0c, 0x7d, 0xdb, 0x7e, 0x0f, 0x19, 0xf0,
  0x3f, 0x18, 0xfc, 0x7e, 0x18, 0x7e, 0x26, 0x3c, 0x64, 0x0c, 0x7e, 0x30,
  0x04, 0xff, 0x20, 0x00, 0xdb, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00
};
const unsigned char PROGMEM logo24_fan[] = {
  0x00, 0x3e, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x7f, 0x80, 0x00, 0x7f, 0x80,
  0x00, 0x7f, 0x80, 0x00, 0x7f, 0x00, 0x00, 0x7c, 0x00, 0x78, 0x7c, 0x00,
  0xfc, 0x78, 0x00, 0xfc, 0x7f, 0xfe, 0xff, 0x67, 0xff, 0xff, 0xc3, 0xff,
  0xff, 0xc3, 0xff, 0xff, 0xe6, 0xff, 0x7f, 0xfe, 0x3f, 0x00, 0x1e, 0x3f,
  0x00, 0x3e, 0x1e, 0x00, 0x3e, 0x00, 0x00, 0xfe, 0x00, 0x01, 0xfe, 0x00,
  0x01, 0xfe, 0x00, 0x01, 0xfe, 0x00, 0x01, 0xfe, 0x00, 0x00, 0xfc, 0x00
};

const unsigned char PROGMEM logo24_idle[]{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xc0,
  0x00, 0x00, 0x40, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00,
  0x01, 0xf7, 0xd5, 0x00, 0x10, 0x00, 0x00, 0x20, 0x00, 0x00, 0x40, 0x00,
  0x00, 0x80, 0x00, 0x7d, 0xf5, 0x40, 0x04, 0x00, 0x00, 0x09, 0xf7, 0xbc,
  0x11, 0x14, 0x20, 0x21, 0x14, 0x20, 0x7d, 0x17, 0x38, 0x01, 0x14, 0x20,
  0x01, 0xf4, 0x20, 0x00, 0x00, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x00, 0x00
};

// Functions for drawing the individual symbols on the display.

void drawHeatingSymbol() {
  display.fillRect(97, 12, 24, 24, SH110X_BLACK);
  display.drawBitmap(97, 12, logo24_heat, 24, 24, SH110X_WHITE);
}
void drawCoolingSymbol() {
  display.fillRect(97, 12, 24, 24, SH110X_BLACK);
  display.drawBitmap(97, 12, logo24_cool, 24, 24, SH110X_WHITE);
}
void drawFanSymbol() {
  display.fillRect(97, 12, 24, 24, SH110X_BLACK);
  display.drawBitmap(97, 12, logo24_fan, 24, 24, SH110X_WHITE);
}
void drawIdleSymbol() {
  display.fillRect(97, 12, 24, 24, SH110X_BLACK);
  display.drawBitmap(97, 12, logo24_idle, 24, 24, SH110X_WHITE);
}

// Setup OLED display
// This functions prints all the neccessary text on the display once.
void display_prepare() {
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print(F("Target:         Mode:"));
  display.setCursor(0, 16);
  display.print(F("Case  :"));
  display.setCursor(0, 32);
  display.print(F("Heater:"));
  display.setCursor(0, 48);
  display.print(F("Heater: OFF"));
  display.setCursor(0, 56);
  display.print(F("Fan   : OFF"));
  display.setCursor(90, 48);
  display.print(F("Vent:"));

  //Print initial target temperature
  display.setTextSize(2);
  display.setCursor(42, 0);
  sprintf(temp_buf, "%3d", target);
  display.print(temp_buf);
  display.print(scale);

  display.display();
}

void setHeaterState(bool setOn) {
  if (heaterState != setOn) {
    display.setTextSize(1);
    display.fillRect(48, 48, 30, 8, SH110X_BLACK);
    display.setCursor(48, 48);

    if(setOn) {
      digitalWrite(HEATER_PIN, HIGH);
      display.print(F("ON"));
    } else {
      digitalWrite(HEATER_PIN, LOW);
      display.print(F("OFF"));
    }

    display.display();
    heaterState = setOn;
  }
}

void setFanState(bool setOn) {
  if (fanState != setOn) {
    display.setTextSize(1);
    display.fillRect(48, 56, 30, 8, SH110X_BLACK);
    display.setCursor(48, 56);

    if(setOn) {
      digitalWrite(FAN_PIN, HIGH);
      display.print(F("ON"));
    } else {
      digitalWrite(FAN_PIN, LOW);
      display.print(F("OFF"));
    }
    
    display.display();
    fanState = setOn;
  }
}

void servoTurn(int targetAngle) {
  int currentAngle = servo.read();

  while (currentAngle != targetAngle) {
    if (currentAngle < targetAngle) {
      servo.write(++currentAngle);
    } else {
      servo.write(--currentAngle);
    }

    delay(18);
  }
}

void servoAttach() {
  servo.attach(SERVO_PIN);
  delay(SERVO_MOVING_MS);
}

void servoDetach() {
  delay(SERVO_MOVING_MS);  // Allow Servo time to reach position
  servo.detach();          // Detach servo so that it wont try to move all the time
}

void setServoState(bool isOpen) {
  if (servoState != isOpen) {
    display.setCursor(90, 56);
    display.setTextSize(1);
    display.fillRect(90, 56, 48, 8, SH110X_BLACK);

    if(isOpen) {
      display.print(F("OPEN"));
    } else {
      display.print(F("CLOSED"));
    }

    display.display();
    servoState = isOpen;
  }
}

// Set position of servo to open flap
void servoOpen() {
  servoAttach();
  servoTurn(SERVO_POS_OPEN);
  setServoState(true);
  servoDetach();
}

// Set position of servo to close flap
void servoClose() {
  servoAttach();
  servoTurn(SERVO_POS_CLOSED);
  setServoState(false);
  servoDetach();
}

// Checks if there are any temperature related problems and turns off the
// heating system accordingly.
//
// There are two different cases here: Overtemp and sensor-error.
//   * Overtemp should be pretty self-explanatory.
//   * Sensor error means that the sensor reports values, that are obviously
//     false. Normally they report -127°C (-196°F) when they are not wired
//     correctly or are defective, so we check for sensor-readings below
//     -20°C (-4°F), as I dont expect anyone actually trying to use this
//     system below that temperature. If you do, adjust the threshold
//     accordingly.
void checkForTempProblems() {
  if (casetemp >= caseovertemplimit || heatertemp >= heaterovertemplimit) {
    setHeaterState(false);
    servoOpen();
    while (true) {
      // Flash the error message and get stuck here.
      // This will cause the software to essentially stop
      // so that the heater can only be turned on again if
      // the user power-cycles the heating system.
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(3);
      display.println(F("ERROR!"));
      display.setTextSize(1);
      display.println(F("Overtemp. detected."));
      display.println(F("Shutting down heater."));
      display.println(F("Please powercycle!"));
      display.display();
      delay(1000);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.fillRect(0, 0, 128, 64, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
      display.setTextSize(3);
      display.println(F("ERROR!"));
      display.setTextSize(1);
      display.println(F("Overtemp. detected."));
      display.println(F("Shutting down heater."));
      display.println(F("Please powercycle!"));
      display.display();
      delay(1000);
    }
  } else if (casetemp <= undertemp || heatertemp <= undertemp) {
    setHeaterState(false);
    servoOpen();
    while (true) {
      // Flash the error message and get stuck here.
      // This will cause the software to essentially stop
      // so that the heater can only be turned on again if
      // the user power-cycles the heating system.
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(3);
      display.println(F("ERROR!"));
      display.setTextSize(1);
      display.println(F("Sensor error! Are"));
      display.println(F("they wired correctly?"));
      display.println(F("Shutting down heater."));
      display.println(F("Please powercycle!"));
      display.display();
      delay(1000);

      display.clearDisplay();
      display.fillRect(0, 0, 128, 64, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
      display.setTextSize(3);
      display.println(F("ERROR!"));
      display.setTextSize(1);
      display.println(F("Sensor error! Are"));
      display.println(F("they wired correctly?"));
      display.println(F("Shutting down heater."));
      display.println(F("Please powercycle!"));
      display.display();
      delay(1000);
    }
  }
}

// Checks if buttons were pressed and set flags accordingly
void checkButtons() {
  buttonUpState = !digitalRead(BUTTON_UP);
  if (buttonUpState && !prevButtonUpState) {
    buttonUpPressed = true;
    prevButtonUpState = true;
  } else if (!buttonUpState && prevButtonUpState) {
    prevButtonUpState = false;
  }

  buttonDownState = !digitalRead(BUTTON_DOWN);
  if (buttonDownState && !prevButtonDownState) {
    buttonDownPressed = true;
    prevButtonDownState = true;
  } else if (!buttonDownState && prevButtonDownState) {
    prevButtonDownState = false;
  }

  buttonSelectState = !digitalRead(BUTTON_SELECT);
  if (buttonSelectState && !prevButtonSelectState) {
    buttonSelectPressed = true;
    prevButtonSelectState = true;
  } else if (!buttonSelectState && prevButtonSelectState) {
    prevButtonSelectState = false;
  }
}

// Gets temperature readings from the sensors and draws them on the display.
void measureTemperatures() {
  sensors.requestTemperatures();

  if (TEMPERATURE_SCALE_C) {
    casetemp = sensors.getTempC(caseThermometer);
    heatertemp = sensors.getTempC(heaterThermometer);
  } else {
    casetemp = sensors.getTempF(caseThermometer);
    heatertemp = sensors.getTempF(heaterThermometer);
  }

  // Show current temp on display
  display.fillRect(42, 16, 46, 32, SH110X_BLACK);
  display.setCursor(42, 16);
  display.setTextSize(2);
  sprintf(temp_buf, "%3d", casetemp);
  display.print(temp_buf);
  display.print(scale);
  display.setCursor(42, 32);
  sprintf(temp_buf, "%3d", heatertemp);
  display.print(temp_buf);
  display.print(scale);
  display.setTextSize(1);
  display.display();
}

// Adjustment of target temperature
// If up/down button was pressed, change target temp accordingly
void updateTargetTemperature() {
  if (buttonUpPressed) {
    target = target + 5;
    if (target > maxsettemp) {  // Limit target temperature to maxsettemp
      target = maxsettemp;
    }
    updateTarget = true;
    buttonUpPressed = false;
  }
  if (buttonDownPressed) {
    target = target - 5;
    if (target < 0) {  // Limit target to positive temperatures
      target = 0;
    }
    updateTarget = true;
    buttonDownPressed = false;
  }

  // Only write changes to display when something actually changed.
  if (updateTarget) {
    // As target changed, set new limits
    upperlimit = target + tolerance / 2;
    lowerlimit = target - tolerance / 2;

    // Remove old target temp from display
    display.fillRect(42, 0, 42, 16, SH110X_BLACK);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(42, 0);
    sprintf(temp_buf, "%3d", target);
    display.print(temp_buf);
    display.print(scale);
    display.display();
    updateTarget = false;  // Reset flag
  }
}

void writeSerialData() {
  //first clear the existing output string
  serial_output[0] = '\0';
  // then write the target temperature
  sprintf(serial_temp_buf, "%3d", target);
  strcat(serial_output, serial_temp_buf);
  strcat(serial_output, ",");
  // then write the case temperature
  sprintf(serial_temp_buf, "%3d", casetemp);
  strcat(serial_output, serial_temp_buf);
  strcat(serial_output, ",");
  //then write the heater temperature
  sprintf(serial_temp_buf, "%3d", heatertemp);
  strcat(serial_output, serial_temp_buf);
  strcat(serial_output, ",");

  // then write the mode setting
  serial_mode_buf[0] = '\0';
  if (operatingMode == MODE_IDLE) {
    strcat(serial_mode_buf, "IDLE");
  } else if (operatingMode == MODE_HEATING) {
    strcat(serial_mode_buf, "HEAT");
  } else if (operatingMode == MODE_COOLING) {
    strcat(serial_mode_buf, "COOL");
  } else if (operatingMode == MODE_FAN) {
    strcat(serial_mode_buf, " FAN");
  }
  strcat(serial_output, serial_mode_buf);
  Serial.println(serial_output);
}

void setup() {
  // Initialize temperature sensors and set resolution to 9 bit
  sensors.begin();
  sensors.setResolution(caseThermometer, 9);
  sensors.setResolution(heaterThermometer, 9);
  sensors.setWaitForConversion(false);

  // Initialize Celsius or Fahrenheit values
  if (TEMPERATURE_SCALE_C) {
    caseovertemplimit = CASE_TEMP_OVERTEMP_LIMIT_C;
    heaterovertemplimit = HEATER_TEMP_OVERTEMP_LIMIT_C;
    target = INITIAL_TARGET_C;
    maxsettemp = MAX_SET_TEMP_SETTING_C;
    maxheatertemp = MAX_HEATER_TEMP_SETTING_C;
    undertemp = SENSOR_ERROR_UNDERTEMP_C;
    tolerance = TOLERANCE_C;
    scale = 'C';
  } else {
    caseovertemplimit = CASE_TEMP_OVERTEMP_LIMIT_F;
    heaterovertemplimit = HEATER_TEMP_OVERTEMP_LIMIT_F;
    target = INITIAL_TARGET_F;
    maxsettemp = MAX_SET_TEMP_SETTING_F;
    maxheatertemp = MAX_HEATER_TEMP_SETTING_F;
    undertemp = SENSOR_ERROR_UNDERTEMP_F;
    tolerance = TOLERANCE_F;
    scale = 'F';
  }

  // Initialize display and print basic information
  display.begin();
  display_prepare();

  // Define pins as input/output
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  // Initialize servo with zero position and briefly open to greet user
  servo.write(SERVO_POS_CLOSED);
  servoOpen();

  // Initialize serial for logging purposes.
  // Since attaching a serial monitor resets the Arduino,
  // we can use setup() to insert the column headers and the
  // monitor program will capture it.
  if (SERIAL_LOGGING) {
    Serial.begin(115200);
    Serial.println(F("Target Temp,Case Temp,Heater Temp,Mode"));
  }
}

void loop() {
  // Check if any of the buttons were pressed
  checkButtons();

  // Read current temperatures from sensors, if set time has passed since
  // last check As this operation takes quite some time, we'll only do it
  // every few seconds. The temperature in the enclosure doesnt't change
  // that fast anyways.
  if (millis() - previousMillis_temp >= TEMPERATURE_DELAY) {
    previousMillis_temp = millis();

    measureTemperatures();   // get new temperatures from sensors
    checkForTempProblems();  // check if temperatures are in safe range

    // Never perform fanOff when heater is running
    if(!heaterState) {
      // Shut off fan if heater temp is low enough and if not in mode cooling or fan
      if (heatertemp < caseovertemplimit && !(operatingMode == MODE_COOLING || operatingMode == MODE_FAN)) {
        setFanState(false);
      }
    }

    // Ensure fan is on when heating is active
    if(heatertemp > caseovertemplimit) {
      setFanState(true);
    }

    // This displays the current millis() on the display. It does not serve any
    // function besides telling me that the system is still alive. I needed this
    // for debugging, because I ran into some weird RAM-overflow situations
    // which resultet in crashes of the controller.
    // I kept the code in here, so that I could easily define PRINT_MILLIS to
    // get this functionality. You probably do not want to define it, as it does
    // not serve any purpose for normal use.
#ifdef PRINT_MILLIS
    display.setCursor(90, 40);
    display.fillRect(90, 40, 37, 8, SH110X_BLACK);
    display.print(millis());
#endif
    display.display();
  }

  // Write a new line of serial data if the timer has elapsed.
  // This allows a different logging rate from the data reporting
  // rate so that logging is not tied to the system's "response rate."
  if (SERIAL_LOGGING && millis() - previousMillis_serial >= SERIAL_RATE_MS) {
    previousMillis_serial = millis();
    writeSerialData();
  }

  // If Select-button is pressed, change modes from 1 to 4
  if (buttonSelectPressed) {
    operatingMode++;
    if (operatingMode > 3) {
      operatingMode = 0;
    }
    buttonSelectPressed = false;
    changeMode = true;
  }

  /*******************************************************
  ################ IDLE MODE - DO NOTHING ################
  *******************************************************/
  if (operatingMode == MODE_IDLE && changeMode) {
    changeMode = false;
    drawIdleSymbol();
    servoClose();
    setHeaterState(false);
  }

  // /*******************************************************
  // ##### HEATING MODE - HEATER ON, FAN ON, VENT CLOSED ####
  // *******************************************************/
  if (operatingMode == MODE_HEATING) {
    if (changeMode) {
      changeMode = false;
      drawHeatingSymbol();
      servoClose();
    }

    // Adjustment of target temperature
    updateTargetTemperature();

    // Check if case temperature is lower or higher than target
    // and turn heater on or off accordingly.
    // The multiple calls of servoClose are a clumsy workaround for
    // keeping the servo quiet (by not keeping it attached the whole time)
    // but also resetting it to the closed position after it sometimes
    // randomly moves when the relays turn on, as they seem to interfere with
    // it.
    if (casetemp <= lowerlimit && heatertemp < (maxheatertemp-tolerance)) {
      if (!heaterState) {
        closeServo = true;
      }

      setFanState(true);
      setHeaterState(true);

      if (closeServo) {
        servoClose();
        closeServo = false;
      }
    } else if (casetemp >= upperlimit || heatertemp > maxheatertemp) {
      if (heaterState) {
        closeServo = true;
      }

      setHeaterState(false);

      if (closeServo) {
        servoClose();
        closeServo = false;
      }
    }
  }

  // /*******************************************************
  // ##### COOLING MODE - HEATER OFF, FAN ON, VENT OPEN #####
  // *******************************************************/
  if (operatingMode == MODE_COOLING && changeMode) {
    changeMode = false;
    drawCoolingSymbol();
    servoOpen();
    setFanState(true);
    setHeaterState(false);
  }

  // /*******************************************************
  // ###### FAN MODE - HEATER OFF, FAN ON, VENT CLOSED ######
  // *******************************************************/
  if (operatingMode == MODE_FAN && changeMode) {
    changeMode = false;
    drawFanSymbol();
    servoClose();
    setFanState(true);
    setHeaterState(false);
  }
}
