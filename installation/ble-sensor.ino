/* 
  ble-central.ino
  
  The installation "pacefaker" consists of two Arduino Nano 33 IoT boards of 
  which one is hooked up to a Sparkfun BioSensor for pulse measurement, while 
  the other one sits inside a silicon-casted human heart to power to mimic the
  measured heart beat. Commuication between the boards is realised with BLE and 
  built up upon sample code by Jeremy Ellis: 
  https://forum.arduino.cc/t/html-connect-to-ble-using-p5-ble-js-and-p5-js/629754

  This file contains the code for the separate sensor-unit and sets up the BLE peripheral.
  Handling the BioSensor is based on sample code by Elias Santistevan, Sparkfun Electronics:
  https://learn.sparkfun.com/tutorials/sparkfun-pulse-oximeter-and-heart-rate-monitor-hookup-guide#hardware-hookup

*/



#include <ArduinoBLE.h>
#include <SparkFun_Bio_Sensor_Hub_Library.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

int resetPin = 2;

/*
  Setup contains a 13x9 LED matrix by Adafruit to display the measured pulse. As
  font width was not suitable for displaying 3-digit values a custom font was needed.
*/
#include <Adafruit_IS31FL3741.h>
Adafruit_IS31FL3741_QT matrix;
TwoWire *i2c = &Wire; // e.g. change this to &Wire1 for QT Py RP2040

// pixel arrays, to be filled later ...
int c[13][9];
int output[13][9];

/*
  This multidimensional array contains the pixels for each digit:
  – first level describes the digit itself, so for displaying a 7 call n[7]
  – second level contains the columns of pixels, all digits but 1 are 4 pixels wide
  – third level contains the binary state of each pixel (column height 9 pixels) 
*/
int n[10][4][9] = { 
  { 
    {0,0,1,1,1,1,1,0,0}, {0,1,0,0,0,0,0,1,0}, {0,1,0,0,0,0,0,1,0}, {0,0,1,1,1,1,1,0,0} 
  }, 
  {
    {0,0,1,0,0,0,0,0,0},{0,1,1,1,1,1,1,1,0}
  },
  {
    {0,0,1,0,0,0,1,1,0},{0,1,0,0,0,1,0,1,0},{0,1,0,0,1,0,0,1,0},{0,0,1,1,0,0,0,1,0}
  },
  {
    {0,1,0,0,0,0,1,0,0},{0,1,0,0,1,0,0,1,0},{0,1,0,1,1,0,0,1,0},{0,1,1,0,0,1,1,0,0}
  },
  {
    {0,0,0,0,1,1,0,0,0},{0,0,0,1,0,1,0,0,0},{0,0,1,0,0,1,0,0,0},{0,1,1,1,1,1,1,1,0}
  },
  {
    {0,1,1,1,0,0,1,0,0},{0,1,0,1,0,0,0,1,0},{0,1,0,1,0,0,0,1,0},{0,1,0,1,1,1,1,0,0}
  },
  {
    {0,0,1,1,1,1,1,0,0},{0,1,0,0,1,0,0,1,0},{0,1,0,0,1,0,0,1,0},{0,0,0,0,0,1,1,0,0}
  },
  {
    {0,1,0,0,0,0,0,0,0},{0,1,0,0,0,1,1,1,0},{0,1,0,0,1,0,0,0,0},{0,1,1,1,0,0,0,0,0}
  },
  {
    {0,0,1,1,0,1,1,0,0},{0,1,0,0,1,0,0,1,0},{0,1,0,0,1,0,0,1,0},{0,0,1,1,0,1,1,0,0}
  },
  {
    {0,0,1,1,0,0,0,0,0},{0,1,0,0,1,0,0,1,0},{0,1,0,0,1,0,0,1,0},{0,0,1,1,1,1,1,0,0}
  }
};

/*
  Setup contains a 34 pixels long RGB LED stripe which gives visual feedback during
  setup, measurement and transmission of pulse values.
*/
#define PIN 9
#define NUMPIXELS 34
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 20


// set up BLE UUIDs for the service itself and a characteristic containing the pulse
const char *uuid = "23c05e57-4520-d4f6-7a60-e6773c736a52";
const char *devicePulseCharacteristicUuid = "6e4a51f1-859d-4323-b984-0e8968fc8ade";

int resPin = 4;
int mfioPin = 5;
int pulse = 0;
int pulseResult = 0;
int pulseTime;
int dly = 250;

int LED = 3; // LED pin for feedback

// create BLE service & characteristic
BLEService heartService(uuid);
BLEIntCharacteristic heartrateCharacteristic(devicePulseCharacteristicUuid, BLERead | BLENotify);

// Initiate BioSensor
SparkFun_Bio_Sensor_Hub bioHub(resPin, mfioPin); 
bioData body;

/*
  The LED stip visualises feedback in different ways, e.g. by fading in and out, 
  flashing or spinning. Custom function handles those repeating cases centrally.
*/
bool faded = false;
void feedbackLed(char type[], int r = 255, int g = 255, int b = 255, int repeat = 1) {
  if (type == "fadeIn") {
    if (!faded) {
      for (int d = 0; d < 256; d++) {
        float factor = d / 255.00;
        for (int i = 0; i < NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(factor * r, factor * g, factor * b));
        }
        pixels.show();
      }
      faded = true;
    }
  }  // fadeIn

  if (type == "fadeOut") {
    if (faded) {
      for (int d = 255; d >= 0; d--) {
        float factor = d / 255.00;
        for (int i = 0; i < NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(factor * r, factor * g, factor * b));
        }
        pixels.show();
      }
      faded = false;
    }
  } // fadeOut

  if (type == "flash") {
    pixels.clear();
    faded = false;
    for (int rep = 0; rep <= repeat; rep++) {
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(r, g, b));
      }
      pixels.show();
      delay(50);
      pixels.clear();
      pixels.show();
      delay(150);
    }
  } // flash

  if(type == "spin") {
    faded = false;
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.clear();

      /*
        spinning is realised by one "core pixel" running along the strip and
        it's neighboring pixels lighting up with 60% and 30% brightness. This
        gives a smoother visual than just one single pixel. As the strip is arranged
        around a circle, handling the edge cases is needed to achive a continuos spin.
      */
      pixels.setPixelColor(i, pixels.Color(r, g, b));  // core pixel

      // handle begin and end of stripe
      switch (i) {
        case 0:
          pixels.setPixelColor(NUMPIXELS - 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          pixels.setPixelColor(NUMPIXELS - 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          break;
        case 1:
          pixels.setPixelColor(NUMPIXELS - 1, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          pixels.setPixelColor(i - 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          break;
        case NUMPIXELS - 2:
          pixels.setPixelColor(i - 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          pixels.setPixelColor(i - 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(0, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          break;
        case NUMPIXELS - 1:
          pixels.setPixelColor(i - 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          pixels.setPixelColor(i - 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(0, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(1, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          break;
        default:
          pixels.setPixelColor(i - 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          pixels.setPixelColor(i - 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 1, pixels.Color(0.6 * r, 0.6 * g, 0.6 * b));
          pixels.setPixelColor(i + 2, pixels.Color(0.3 * r, 0.3 * g, 0.3 * b));
          break;
      }
      pixels.show();
      delay(20);
    }
    pixels.clear();
    pixels.show();
  } // spin

}

void blePeripheralConnectHandler(BLEDevice central)
{
  // central connected event handler
  Serial.print("Connected to central: ");
  Serial.println(central.address());

  // shortly fade feedback LEDs to green
  feedbackLed("fadeIn",0,255,0);
  feedbackLed("fadeOut",0,255,0);
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  // central disconnected event handler
  Serial.print("Disconnected from central: ");
  Serial.println(central.address());

  // shortly fade feeback LEDs to red, then reboot Arduino
  feedbackLed("fadeIn",255,0,0);
  feedbackLed("fadeOut",255,0,0);
  digitalWrite(resetPin, LOW);
}

void setup() {
  // setup reset pin to reboot in case of errors
  // needs to be the first command, otherwise Arduino goes in th reboot loop
  digitalWrite(resetPin, HIGH);
  delay(200);
  pinMode(resetPin, OUTPUT);

  Serial.begin(115200);

  Wire.begin();

  // Matrix setup
  if (! matrix.begin(IS3741_ADDR_DEFAULT, i2c)) {
    Serial.println("IS41 not found");
    while (1);
  }
  Serial.println("IS41 found!");
  i2c->setClock(800000);
  matrix.setLEDscaling(0xFF);
  matrix.setGlobalCurrent(0xFF);
  Serial.print("Global current set to: ");
  Serial.println(matrix.getGlobalCurrent());

  matrix.fill(0);
  matrix.enable(true);
  matrix.setRotation(0);
  matrix.setTextWrap(false);

  // Pulse sensor setup
  int result = bioHub.begin();
  if (result == 0) Serial.println("Sensor started!");
  else Serial.println("Could not communicate with the sensor!");
 
  Serial.println("Configuring Sensor...."); 
  int error = bioHub.configBpm(MODE_TWO); 
  if(error == 0){
    Serial.println("Sensor configured.");
  }
  else {
    Serial.println("Error configuring sensor.");
    Serial.print("Error: "); 
    Serial.println(error); 
  }
 
  Serial.println("Loading up the buffer with data....");
  delay(4000);          

  // Go into loop until BLE module is successfully started
  if (!BLE.begin()) {
    while (1);
  }

  // set the local name peripheral advertises
  BLE.setLocalName("Nano Sensor");
  
  // set the service
  BLE.setAdvertisedService(heartService);

  // add characteristics to the service
  heartService.addCharacteristic(heartrateCharacteristic);

  // add the service
  BLE.addService(heartService);

  // handle connection events
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // set default values
  heartrateCharacteristic.writeValue(0);

  // start advertising
  BLE.advertise();


  // Pixel strip
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif

  pixels.begin(); 
}

/*
  translate pulse value to pixel array ready for LED matrix
*/
void getPixels(int pulse) {
  int d1 = (pulse / 100) % 10; // hundreds
  int d2 = (pulse / 10) % 10; // tens
  int d3 = pulse % 10; // ones
  
  int col = 0; // column index
  int digitLength;

  // add hundreds to the column array
  if(d1 > 0) {
    if(d1 == 1) digitLength = 2;
    else digitLength = 4;
    for(int i = 0; i < digitLength; i++) {
      for(int x = 0; x < 9; x++) {
        c[col][x] = n[d1][i][x];
      }
      col++;
    }
  }

  // add one pixel wide gap
  for (int x = 0; x < 9; x++) {
    c[col][x] = 0;
  }
  col++;

  // add tens to column array
  if(d2 > 0 || d1 > 0) {
    if(d2 == 1) digitLength = 2;
    else digitLength = 4;
    for(int i = 0; i < digitLength; i++) {
      for(int x = 0; x < 9; x++) {
        c[col][x] = n[d2][i][x];
      }
      col++;
    }
  }

  // again one pixel gap
  for (int x = 0; x < 9; x++) {
    c[col][x] = 0;
  }
  col++;

  // and finally the ones
  if(d3 == 1) digitLength = 2;
  else digitLength = 4;
  for(int i = 0; i < digitLength; i++) {
    for(int x = 0; x < 9; x++) {
      c[col][x] = n[d3][i][x];
    }
    col++;
  }

  // calculate margin based on the amount of columns taken up by the number
  int margin = (13 - (col+1)) / 2;

  // transpose the temporary column array together with the calculated margins to final output array ordered by rows
  for(int i = 0; i < 13; i++) {
    for(int x = 0; x < 9; x++) {
      if(i < margin || i >= (margin + col))output[i][x] = 0;
      else output[i][x] = c[(i - margin)][x];
    }
  }
}

uint32_t white = matrix.Color(255,255,255);

int measuringTime = 0;
int unmatchedSamples = 20;
int bckupPulse;

void resetSensor() {
  digitalWrite(resPin, HIGH);
  delay(100);
  digitalWrite(resPin, LOW);
  Serial.println("Sensor reset");
  measuringTime = 0;
}

bool matrixOutput = false;
int connectionTimeout = 0;

void loop() {
  // poll for BLE events
  BLE.poll();

  /*
    check if connection is up&running and if not–with a certain tolerance–
    force Arduino to reboot. As the central is doing the same, both devices
    reboot once the connection is lost and autonomously solve their issues. 
  */
  if(BLE.connected()) {
    connectionTimeout = 0;
  }
  else {
    connectionTimeout++;
  }
  if(connectionTimeout > 20) {
    Serial.println("connection timeout");
    feedbackLed("fadeIn", 255, 0, 0);
    feedbackLed("fadeOut", 255, 0, 0);
    digitalWrite(resetPin, LOW);
  }

  // Read body data from sensor
  body = bioHub.readBpm();
  pulse = body.heartRate;

  // default delay between loops (sensor readings)
  dly = 250;

  /*
    BioSensor gives back a status code, which is used here to handle events correspondingly:
     0	Success
     1	Not Ready
    -1	Object Detected
    -2	Excessive Sensor Device Motion
    -3	No object detected
    -4	Pressing too hard
    -5	Object other than finger detected
    -6	Excessive finger motion

    Sometimes, one or several readings in a row are unsuccessful, eventhough this is just a 
    short interruption of the otherwise functioning system. To avoid those unmatched samples
    to interrupt the whole flow, a certain tolerance is programmed.
    Unmatched samples occur for example if the status is successful, yet the pulse is 0.
    In those cases, the previously measured pulse (stored in bckupPulse) is handle as if just 
    measured.
  */

  // make status identifier positive integer 
  int status = body.extStatus + 6;

  Serial.print("Status: ");
  Serial.print(status);
  Serial.print(" / ");
  if (((status == 6 || status == 5) && pulse > 0) || ((status == 6 || status == 5) && unmatchedSamples < 10)) {
    if (pulse > 0) {
      Serial.print("Success!");

      // store the current pulse in case of unmatched samples
      bckupPulse = pulse;

      // reset count
      unmatchedSamples = 0;
    }
    else {
      Serial.print("unmatched sample");

      // treat backed-up Pulse as if just measured
      pulse = bckupPulse;

      // increase count to interrupt after certain tolerance
      unmatchedSamples++;
    }

    // store current pulse & timestamp for visualising in the matrix
    pulseResult = pulse;
    pulseTime = millis();

    // fade feedback LEDs to bright white, meaning it's all right
    feedbackLed("fadeIn", 255, 255, 255);

    // reset measurment timeout
    measuringTime = 0;
  } else {
    unmatchedSamples = 20;
    pulse = 0;
    if (status == 5 || (status == 6 && pulse == 0)) {
      Serial.print("Object Detected");

      // blue spinning while measuring, motivate users to wait      
      feedbackLed("spin", 0, 0, 255);

      // safe timestamp when system started measuring
      if(measuringTime == 0) measuringTime = millis();

      // handle measuring timeout by rebooting Arduino
      if(millis() - measuringTime > 20000) {
        Serial.println("measuring timeout");
        digitalWrite(resetPin, LOW);
      }

      // set delay between loops to 0, so that spinning runs continously
      dly = 0;
    } else {
      // reset measurment timeout
      measuringTime = 0;

      if (status == 3) {
        Serial.print("No object detected");

        // attempt to reset sensor module
        if (faded) {
          digitalWrite(resPin, HIGH);
          delay(100);
          digitalWrite(resPin, LOW);
        }

        // fade out feedback LEDs
        feedbackLed("fadeOut", 255, 255, 255);

        // check if there is a measured pulse result to display on the matrix & display timeout is not exceeded
        if (pulseResult > 0 && (millis() - pulseTime) < 5000) {
          // trigger matrix only if not yet triggered
          if(!matrixOutput) {
            // reset
            matrix.fill(0);
            matrixOutput = true;

            Serial.println("- - - - - - - - - - - - -");
            getPixels(pulseResult);

            // iterate through pixel array and switch on/off each pixel
            // use serial print to visualise pixels on monitor for fun (and debugging)
            for(int row = 0; row < 9; row++) {
              for(int col = 0; col < 13; col++) {
                if(output[col][row] == 1) {
                  Serial.print("x ");
                  matrix.drawPixel(col, row, white);
                }
                else Serial.print("  ");
                if(col == 12) Serial.println(" ");
              }
            }

            Serial.println(" ");
            Serial.println("- - - - - - - - - - - - -");
          }
        } else {
          matrix.fill(0);
          matrixOutput = false;
        }
      }
      else {
        if (status != 7) {
          // handle all error statuses by blinking red
          Serial.print("Error");
          feedbackLed("flash", 255, 0, 0, 2);
          resetSensor();
        }
      }
      faded = false;
    }
    pixels.clear();
    pixels.show();
    pulse = 0;
  }

  int pulseDisplayDuration = millis() - pulseTime;
  if(pulseDisplayDuration > 5000) {
    matrix.fill(0);
    matrixOutput = false;
  }

  Serial.print(" / Pulse: ");
  Serial.println(pulse);

  // send heartbeat to BLE central
  heartrateCharacteristic.writeValue(pulse);

  delay(dly);

}
