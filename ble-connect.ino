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
const char *uuid = "465A640E-7553-5DDC-86C9-B3E59919C36D";
int resPin = 4;
int mfioPin = 5;

BLEService heartService(uuid); // create service

// create status characteristics
BLEIntCharacteristic statusCharacteristic("e7ea0fb5-4f46-4241-8825-a3fa60acbb71", BLERead | BLENotify);
BLEIntCharacteristic heartrateCharacteristic("6e4a51f1-859d-4323-b984-0e8968fc8ade", BLERead | BLENotify);

// Takes address, reset pin, and MFIO pin.
SparkFun_Bio_Sensor_Hub bioHub(resPin, mfioPin); 
bioData body;
// ^^^^^^^^^
// What's this!? This is a type (like int, byte, long) unique to the SparkFun
// Pulse Oximeter and Heart Rate Monitor. Unlike those other types it holds
// specific information on your heartrate and blood oxygen levels. BioData is
// actually a specific kind of type, known as a "struct". 
// You can choose another variable name other than "body", like "blood", or
// "readings", but I chose "body". Using this "body" varible in the 
// following way gives us access to the following data: 
// body.heartrate  - Heartrate
// body.confidence - Confidence in the heartrate value
// body.oxygen     - Blood oxygen level
// body.status     - Has a finger been sensed?
// body.extStatus  - What else is the finger up to?
// body.rValue     - Blood oxygen correlation coefficient.  


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
  BLE.setLocalName("heartSensorLED");
  
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

void loop() {
  
  BLE.poll();  // poll for BLE events

  // get body data from sensor
  body = bioHub.readBpm();

  // send heartbeat to BLE central
  Serial.print("Hearbeat: ");
  Serial.println(body.heartRate);
  heartrateCharacteristic.writeValue(body.heartRate);
  statusCharacteristic.writeValue(body.extStatus);

  delay(250); 
  
}