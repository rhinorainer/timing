/* 
  ble-central.ino
  
  The installation "pacefaker" consists of two Arduino Nano 33 IoT boards of 
  which one is hooked up to a Sparkfun BioSensor for pulse measurement, while 
  the other one sits inside a silicon-casted human heart to power to mimic the
  measured heart beat. Commuication between the boards is realised with BLE and 
  built up upon sample code by Jeremy Ellis: 
  https://forum.arduino.cc/t/html-connect-to-ble-using-p5-ble-js-and-p5-js/629754

  This file contains the code for the actuator-unit inside the heart and sets up the BLE central.

*/

#include <ArduinoBLE.h>
#include <Servo.h>

// set up BLE UUIDs for the service itself and a characteristic containing the pulse
const char* deviceServiceUuid = "23c05e57-4520-d4f6-7a60-e6773c736a52";
const char* devicePulseCharacteristicUuid = "6e4a51f1-859d-4323-b984-0e8968fc8ade";

BLECharacteristic pulseCharacteristic;
 
bool connected = false;
uint32_t pulse;

/*
  The mechanism is operated by four servo motors of which each has a resting 
  position (e.g. amin for servo_a) and an open position (amax). Each value was
  calibrated after physically installing the motors in their positions and is 
  dependent on the mechanism's flap design.
*/
Servo servo_a;
Servo servo_b;
Servo servo_c;
Servo servo_d;
int amin = 120; 
int amax = 85;
int bmin = 85; 
int bmax = 130;
int cmin = 150; 
int cmax = 110; 
int dmin = 45; 
int dmax = 75;

/* 
  The heart cycle is herein reduced to two phases: diastole and systole.
  The duration of the diastole phase is nearly constant, regardless of the pulse
  itself and lasts approx. 300ms. The duration of the systole phase in return is then
  filling the remaining time till completion of the cycle.

*/
int dia = 300; 
int dly = 300;
int opn = 100; // duration of flap opening

// further variable definitions, for explanation see below ...
int pinR = 3;
int pinG = 2;
int connectionTime;

int resetPin = 21;

int servoStatus = false;
int detachCount = 0;
int pulse_duration = 0;
int unmatchedReads = 0;

BLEDevice peripheral;

void setup() {
  // setup reset pin to reboot in case of errors
  // needs to be the first command, otherwise Arduino goes in th reboot loop
  digitalWrite(resetPin, HIGH);
  delay(200);
  pinMode(resetPin, OUTPUT);
  
  Serial.begin(9600);

  // servo setup
  /*
    As soon as servos are attached to a pin they do some buzzing sounds and tend to
    move slightly because of unsteady current. To enhance the experience (silence)
    and safe battery life, they are only attached when needed and imediately detached
    afterwards. Custom functions attachServos() and detachServos() handle this repetative
    task.
  */
  attachServos();
  servo_a.write(amin);
  servo_b.write(bmin);
  servo_c.write(cmin);
  servo_d.write(dmin);
  detachServos();
  
  // Go into loop until BLE module is successfully started
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

void detachServos() {
  servo_a.detach();
  servo_b.detach();
  servo_c.detach();
  servo_d.detach();
  servoStatus = false;
  Serial.println("Servo detached");
}
void attachServos() {
  servo_a.attach(9);
  servo_b.attach(10);
  servo_c.attach(11);
  servo_d.attach(12);
  servoStatus = true;
  Serial.println("Servo attached");
}

void loop() {
  if (connected) {
    /* 
      it's all about timing ... the varying pulse manifests in operating
      the mechanism some milliseconds faster or slower, so precision is
      crucial. As execution of the code takes some ms, the duration is 
      measured and later on detracted from the delay between two cycles.
    */
    int start_time = millis();

    /*
      LED is set to green when the setup is successfully completed. 
      To keep the experience clean, the LED turns off 2 seconds after
      connection has successfully been established.
    */
    if(millis() - connectionTime > 2000) {
      analogWrite(pinR, 0);
      analogWrite(pinG, 0);
    }
    

    // servo control
    if (pulse > 0) {

      // servo activation
      if(!servoStatus) attachServos();

      /*
        detachCount is counting how many loops have been performed since
        pulse went to 0 in order to detach servos only after a certain
        tolerance. Resetting here, as pulse is greater than 0.
      */
      detachCount = 0;
      
      /*
        This is where the magic happens:
        – first set of flaps (a & b) open, while second set (c & d) close
        – keep this position for defined time
        – close first set, all flaps are closed now
        – keep this position until the end of diastole phase
      */
      // diastole
      servo_a.write(amax);
      servo_b.write(bmax);
      servo_c.write(cmin);
      servo_d.write(dmin);
      delay(opn);
      servo_a.write(amin);
      servo_b.write(bmin);
      delay(dia - opn);

      /*
        Second phase:
        – open second set of flaps
        – keep this position for defined time
        – close set, all flaps are closed again
        – keep this position until the next loop. Variable dly contains the delay
          which–depending on the pulse–should be kept between two cycles. 
      */
      // systole
      servo_c.write(cmax);
      servo_d.write(dmax);
      delay(opn);
      servo_c.write(cmin);
      servo_d.write(dmin);
      pulse_duration = (60000) / pulse;
      dly = pulse_duration - dia - opn;
      
    } else {
      detachCount++;
      pulse_duration = 0;
    }

    // read updated value from BLE peripheral
    int readIni = millis();
    pulseCharacteristic.readValue(pulse);
    int readTime = millis() - readIni;

    /*
      Sometimes the installation runs into a condition, where BLE characteristics
      are not being read properly, in those cases the readVaule method takes 0ms 
      for execution. If that happens, set LED to red and–after a certain tolerance–
      reboot the whole system.
    */
    if(readTime == 0) {
      // connection error?
      analogWrite(pinR, 255);
      unmatchedReads++;
      if(unmatchedReads > 100) {
        digitalWrite(resetPin, LOW);
      }
    } else unmatchedReads = 0;

    // loop is executed, calculate time needed and adapt the delay till the next loop accordingly
    int loop_duration = millis() - start_time;
    dly -= loop_duration;
    if(dly < 0 ||pulse == 0) dly = 0;
    delay(dly);
  } else {
    delay(100);
    analogWrite(pinR, 255);
    detachCount++;
  }

  // put servos to standby
  if(servoStatus && detachCount > 5) detachServos();
}

/*
  Build up connection with sensor module (peripheral)
  An RGB LED inside the translucent case gives visual feeback on the
  connection status and turns off only when a stable connection has 
  been established.
*/
void connectToPeripheral(){
  Serial.println("- Discovering peripheral device...");
  int i = 0; // index for counting the connection attempts
  do
  {
    i++;
    BLE.scanForUuid(deviceServiceUuid);
    peripheral = BLE.available();

    /* 
      reboot Arduino if more than 50 attempts were unsuccessful. Prooved 
      to handle well the fact, that both central and peripheral need to
      boot / start seeking connection at about the same time.
    */
    if(i > 50) digitalWrite(resetPin, LOW);

    // while searching LED blinks orange, changes state every 5 iterations
    if(i % 5 == 0) {
      analogWrite(pinR, 200);
      analogWrite(pinG, 100);
    } else {
      analogWrite(pinR, 0);
      analogWrite(pinG, 0);
    }
    delay(100);
  } while (!peripheral);
  
  // when peripheral is found, LED changes to constant orange 
  analogWrite(pinR, 200);
  analogWrite(pinG, 100);


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

    // success, set LED to green
    analogWrite(pinR, 0);
    analogWrite(pinG, 255);

    // safe timestamp of established connection
    connectionTime = millis();
  } else {
    Serial.println("* Peripheral device attributes discovery failed!");
    Serial.println(" ");

    // failed, set LED to red and reboot after 1000ms
    analogWrite(pinR, 255);
    analogWrite(pinG, 0);
    delay(1000);
    digitalWrite(resetPin, LOW);
    return;
  }

  pulseCharacteristic = peripheral.characteristic(devicePulseCharacteristicUuid);
    
  if (!pulseCharacteristic) {
    Serial.println("* Peripheral device does not have heartbeat characteristic!");
    peripheral.disconnect();

    // failed, set LED to red and reboot after 1000ms
    analogWrite(pinR, 255);
    analogWrite(pinG, 0);
    delay(1000);
    digitalWrite(resetPin, LOW);
    return;
  } else {
    connected = true;
    Serial.println("* Peripheral device attributes read succesfully!");
  }
  
  Serial.println("- Peripheral device disconnected!");
}
