/*
  BLE Heartbeat Sensor with Browser Interface
  Build up on sample code by 
  – Jeremy Ellis (https://forum.arduino.cc/t/html-connect-to-ble-using-p5-ble-js-and-p5-js/629754)
  – Elias Santistevan, Sparkfun Electronics (https://learn.sparkfun.com/tutorials/sparkfun-pulse-oximeter-and-heart-rate-monitor-hookup-guide#hardware-hookup) 
*/



#include <ArduinoBLE.h>
#include <SparkFun_Bio_Sensor_Hub_Library.h>
#include <Wire.h>

// Global constants and variables
const int ledPin = LED_BUILTIN; // set ledPin to on-board LED
const char *uuid = "23c05e57-4520-d4f6-7a60-e6773c736a52";
int resPin = 4;
int mfioPin = 5;
int pulse = 0;

int LED = 3; // LED pin for feedback

BLEService heartService(uuid); // create service

// create status characteristics
BLEIntCharacteristic statusCharacteristic("e7ea0fb5-4f46-4241-8825-a3fa60acbb71", BLERead | BLENotify);
BLEIntCharacteristic heartrateCharacteristic("6e4a51f1-859d-4323-b984-0e8968fc8ade", BLERead | BLENotify);

// Takes address, reset pin, and MFIO pin.
SparkFun_Bio_Sensor_Hub bioHub(resPin, mfioPin); 
bioData body;

void setup() {
  
  pinMode(ledPin, OUTPUT); 
  Serial.begin(115200);

  Wire.begin();
  int result = bioHub.begin();
  if (result == 0) //Zero errors!
    Serial.println("Sensor started!");
  else
    Serial.println("Could not communicate with the sensor!");
 
  Serial.println("Configuring Sensor...."); 
  int error = bioHub.configBpm(MODE_TWO); // Configuring just the BPM settings. 
  if(error == 0){ // Zero errors
    Serial.println("Sensor configured.");
  }
  else {
    Serial.println("Error configuring sensor.");
    Serial.print("Error: "); 
    Serial.println(error); 
  }

  // Data lags a bit behind the sensor, if you're finger is on the sensor when
  // it's being configured this delay will give some time for the data to catch up. 
  Serial.println("Loading up the buffer with data....");
  delay(4000);          

  // begin initialization
  if (!BLE.begin()) {

    digitalWrite(ledPin, HIGH);
    while (1);  // kills it here
  }

  // set the local name peripheral advertises
  BLE.setLocalName("Nano Sensor");
  
  // set the service
  BLE.setAdvertisedService(heartService);

  // add both characteristics to the service
  heartService.addCharacteristic(statusCharacteristic);
  heartService.addCharacteristic(heartrateCharacteristic);

  // add the service
  BLE.addService(heartService);

  // set default values
  statusCharacteristic.writeValue(0);
  heartrateCharacteristic.writeValue(0);

  // start advertising
  BLE.advertise();

}

void flashLED(int num = 4) {
  for(int i = 0; i < num; i++) {
    analogWrite(LED, 255);
    delay(80);
    analogWrite(LED, 0);
    delay(80);
  }
  delay(200);
} 

void loop() {
  
  BLE.poll();  // poll for BLE events

  // get body data from sensor
  body = bioHub.readBpm();

  pulse = 0;

    switch(body.extStatus + 6) {
    case 6: 
      Serial.print("Success!");
      pulse = body.heartRate;

      // LED on
      analogWrite(LED, 255);
      break;
    case 7: 
      Serial.print("Not Ready");
      break;
    case 5:	
      Serial.print("Object Detected");

      //fade LED
      for(int i = 0; i < 255; i++) {
        analogWrite(LED, i);
        delay(2);
      }
      for(int i = 255; i > 0; i--) {
        analogWrite(LED, i);
        delay(2);
      }

      break;
    case 4:	
      Serial.print("Excessive Sensor Device Motion");
      flashLED(5);
      break;
    case 3:	
      Serial.print("No object detected");
      analogWrite(LED, 0);
      break;
    case 2:	
      Serial.print("Pressing too hard");
      flashLED();
      break;
    case 1:	
      Serial.print("Object other than finger detected");
      flashLED(6);
      break;
    case 0:	
      Serial.print("Excessive finger motion");
      flashLED();
      break;
  }

  Serial.print(" / Pulse: ");
  Serial.println(body.heartRate);

  // send heartbeat to BLE central
  heartrateCharacteristic.writeValue(body.heartRate);
  statusCharacteristic.writeValue(body.extStatus);

  delay(250); 
  
}
