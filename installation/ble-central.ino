/*
  BLE_Central_Device.ino

  This program uses the ArduinoBLE library to set-up an Arduino Nano 33 BLE Sense 
  as a central device and looks for a specified service and characteristic in a 
  peripheral device. If the specified service and characteristic is found in a 
  peripheral device, the last detected value of the on-board gesture sensor of 
  the Nano 33 BLE Sense, the APDS9960, is written in the specified characteristic. 

  The circuit:
  - Arduino Nano 33 BLE Sense. 

  This example code is in the public domain.
*/

#include <ArduinoBLE.h>
#include <Servo.h>

const char* deviceServiceUuid = "23c05e57-4520-d4f6-7a60-e6773c736a52";
const char* devicePulseCharacteristicUuid = "6e4a51f1-859d-4323-b984-0e8968fc8ade";
const char* deviceStatusCharacteristicUuid ="e7ea0fb5-4f46-4241-8825-a3fa60acbb71";

BLECharacteristic pulseCharacteristic;
BLECharacteristic statusCharacteristic;
 
bool connected = false;
uint32_t status;
uint32_t pulse;

// servo variables
Servo servo_a;
Servo servo_b;
int amin = 55;
int amax = 95;
int bmin = 45;
int bmax = 5;

int dia = 300;
int dly = 300;

void setup() {
  Serial.begin(9600);
  //while (!Serial);

  // servo setup
  servo_a.attach(11);
  servo_a.write(amin);
  servo_b.attach(9);
  servo_b.write(bmin);
  
  if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  
  BLE.setLocalName("Nano Central"); 
  BLE.advertise();

  Serial.println("Servo Unit");
  Serial.println(" ");
  connectToPeripheral();
}

void loop() {
  if(connected) {

    pulseCharacteristic.readValue(pulse);
    
    /*
    // read status characteristic for debugging
    // status from sensor is an integer between -6 and 1
    statusCharacteristic.readValue(status);
    status += 6;

    switch(status) {
    case 6: 
      Serial.print("Success!");
      break;
    case 7: 
      Serial.print("Not Ready");
      break;
    case 5:	
      Serial.print("Object Detected");
      break;
    case 4:	
      Serial.print("Excessive Sensor Device Motion");
      break;
    case 3:	
      Serial.print("No object detected");
      break;
    case 2:	
      Serial.print("Pressing too hard");
      break;
    case 1:	
      Serial.print("Object other than finger detected");
      break;
    case 0:	
      Serial.print("Excessive finger motion");
      break;
    }

    Serial.print(" / Pulse: ");
    Serial.println(pulse);
    */

    // servo control
    if(status == 6 && pulse > 0) {
      if(servo_a.read() == amin) {
        servo_a.write(amax);
        servo_b.write(bmin);
        dly = dia;
      } else {
        servo_a.write(amin);
        servo_b.write(bmax);
        dly = (60000 / pulse) - dia;
      }
    }
    if(dly < 0||pulse == 0) dly = dia;
    Serial.println(dly);
    delay(dly);
  } else {
    delay(100);
  }
}

void connectToPeripheral(){
  BLEDevice peripheral;
  
  Serial.println("- Discovering peripheral device...");

  do
  {
    BLE.scanForUuid(deviceServiceUuid);
    peripheral = BLE.available();
    Serial.println(".");
    delay(100);
  } while (!peripheral);
  
  if (peripheral) {
    Serial.println("* Peripheral device found!");
    Serial.print("* Device MAC address: ");
    Serial.println(peripheral.address());
    Serial.print("* Device name: ");
    Serial.println(peripheral.localName());
    Serial.print("* Advertised service UUID: ");
    Serial.println(peripheral.advertisedServiceUuid());
    Serial.println(" ");
    BLE.stopScan();
    controlPeripheral(peripheral);
  }
}

void controlPeripheral(BLEDevice peripheral) {
  Serial.println("- Connecting to peripheral device...");

  if (peripheral.connect()) {
    Serial.println("* Connected to peripheral device!");
    Serial.println(" ");
  } else {
    Serial.println("* Connection to peripheral device failed!");
    Serial.println(" ");
    return;
  }

  Serial.println("- Discovering peripheral device attributes...");
  if (peripheral.discoverAttributes()) {
    Serial.println("* Peripheral device attributes discovered!");
    Serial.println(" ");
  } else {
    Serial.println("* Peripheral device attributes discovery failed!");
    Serial.println(" ");
    peripheral.disconnect();
    return;
  }

  pulseCharacteristic = peripheral.characteristic(devicePulseCharacteristicUuid);
  statusCharacteristic = peripheral.characteristic(deviceStatusCharacteristicUuid);
    
  if (!pulseCharacteristic) {
    Serial.println("* Peripheral device does not have heartbeat characteristic!");
    peripheral.disconnect();
    return;
  } else {
    connected = true;
    statusCharacteristic.readValue(status);
    pulseCharacteristic.readValue(pulse);
  }
  
  Serial.println("- Peripheral device disconnected!");
}
