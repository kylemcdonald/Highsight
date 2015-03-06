/* motor driver for Highsight
   Joint commission for STRP 2015 and Cinekid 2015, by Kyle McDonald and Ranjit Bhatnagar

   Communicates with server with OSC over UDP.
*/


// LIBRARIES -----------------------------------

// LIDARduino library
// https://github.com/stuthedew/LIDARduino
#include <I2C.h>
#include <LIDARduino.h>
#include <LIDARLite_registers.h>


// David Bouchard's arduino-osc library
// http://www.deadpixel.ca/arduino-osc/
// https://github.com/davidbouchard/arduino-osc
#include <OscUDP.h>

// TimerOne library from pjrc
// http://www.pjrc.com/teensy/td_libs_TimerOne.html
#include <TimerOne.h>

// Ethernet libraries
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// EEPROM library
#include <EEPROM.h>


// IMPORTANT SETTINGS ----------------------------

// network:
const int LOCALNET = 2; // 2 for osx internet sharing 192.168.2.*, 1 for NYCR network 192.168.1.*

// physical:
const float WHEEL_RADIUS = (2.0 + 0.125) * 2.54; // 2 inch wheel + 1/8" grommet, in cm
const float MAX_POSITION = 51.0; // in cm



// NETWORK SETUP  -------

// get motor id (0 for NW motor, 1 for NE, 2 for SE, 3 for SW) from EEPROM address 0
int MOTOR_ID = EEPROM.read(0);

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, MOTOR_ID };
IPAddress listeningIP(192,168,LOCALNET,42+MOTOR_ID); // you need to set this

unsigned int listeningPort = 12001;      // local port to listen on

NetAddress destination;
//IPAddress destinationIP( 255,255,255,255 ); // 255... is broadcast address according to http://forum.arduino.cc/index.php?topic=164119.0
IPAddress destinationIP( 192,168,LOCALNET,255 ); // this is broadcast address when using osx internet sharing, according to ipconfig listing for bridge100
int destinationPort = 12000;

EthernetUDP UDP;
OscUDP etherOSC;  




// MOTOR SETUP -------

const float CM_PER_REV = TWO_PI * WHEEL_RADIUS;
const int STEPS_PER_REV = 1600;
const float CM_PER_STEP = CM_PER_REV / STEPS_PER_REV;
const float STEPS_PER_CM = STEPS_PER_REV / CM_PER_REV;

const float MAX_ACCEL = 60.0; // in cm/sec^2

double goalSpeed = 0;


// can i track steps?
volatile long stepperpos = 0;
volatile int dir = 0;
volatile unsigned long period;
double currentSpeed = 0;

// timer for sending current position
unsigned long lastLoopTime = 0;
unsigned long updateTime = 0;


// MOTOR DRIVER SETUP ------------

// stepper driver pins on ARDUINO ETHERNET 
const int PULSEPIN = 9; // yellow
const int DIRPIN = 8;  // green
const int ENAPIN = 7;  // white

// limit switches
const int HARDLIMITPIN = 6; // active low


// LIDAR-LITE SETUP ------
LIDAR_Lite_I2C lidar;


// position feedback
int last_lidar_estimate = 0;
float last_stepper_estimate = 0;
float last_joint_estimate = 0;



void setup() {
  // LIDAR-Lite setup
  lidar.begin();
  
  // ETHERNET setup
  Ethernet.begin(mac,listeningIP);
  UDP.begin(listeningPort);
  etherOSC.begin(UDP);
  
  destination.set(destinationIP, destinationPort);
  
  // STEPPER setup
  // limit switches
  pinMode(HARDLIMITPIN, INPUT_PULLUP);  
  
  //pinMode(PULSEPIN, OUTPUT); pwm() will do this
  pinMode(DIRPIN, OUTPUT);
  pinMode(ENAPIN, OUTPUT);
  
  digitalWrite(ENAPIN, 1); // enable motor driver
  
  // set up interrupts for motor speed control
  Timer1.initialize(1000);
  Timer1.pwm(PULSEPIN, 0);
  Timer1.start();
  
  Timer1.attachInterrupt(countSteps);
  
  //Serial.begin(9600);
}

void loop() {
  // send a message every 100 ms
  
  // avoid using delay() since it just blocks everything  
  // so here is a simple timer that controls how often we send
  unsigned long now = micros();
  unsigned long elapsed = now - lastLoopTime;
  if (elapsed==0) return;
  
  
  float stepper_in_cm = stepperpos * CM_PER_STEP;
  float delta_stepper_in_cm = stepper_in_cm - last_stepper_estimate;
  last_stepper_estimate = stepper_in_cm;
  
  lastLoopTime = now;
  
  if (now - updateTime > 100000) {
    OscMessage msg("/motor");

    msg.add(MOTOR_ID);
    msg.add(stepper_in_cm); 
    msg.add(currentSpeed);
    
    etherOSC.send(msg, destination);
    
    // reset the timer
    updateTime = now;
    
  } // end if
  
  updateSpeed(elapsed);
  checkLimitSwitch();
  checkPositionLimits();
  
  // check for incoming messages 
  etherOSC.listen();

  // update lidar
  int lidar_in_cm = lidar.easyDistance();
  int delta_lidar_in_cm = last_lidar_estimate - lidar_in_cm;
  last_lidar_estimate = lidar_in_cm;
  
  
  float stepper_cm_per_second = delta_stepper_in_cm * 1000000.0 / elapsed;
  
  float kalmanEstimate = kalmanCalculate(lidar_in_cm, stepper_cm_per_second, elapsed / 1000);
  float compEstimate1 = Complementary(lidar_in_cm, stepper_cm_per_second, elapsed / 1000);
  float compEstimate2 = Complementary2(lidar_in_cm, stepper_cm_per_second, elapsed / 1000);
  
  {
    OscMessage msg("/lidar");
    msg.add(MOTOR_ID);
    msg.add(lidar_in_cm);
    msg.add(kalmanEstimate);
    msg.add(compEstimate1);
    msg.add(compEstimate2);
    etherOSC.send(msg, destination);
  }
  
  
  
}


void oscEvent(OscMessage &m) { 
  m.plug("/motors", oscSpeed);  
}


void oscSpeed(OscMessage &m) {
  // /motors/nwLength, nwSpeed, neLength, neSpeed, seLength, seSpeed, swLength, swSpeed
  float value = m.getFloat(2 * MOTOR_ID + 1);
  goalSpeed = value;
}


void updateSpeed(unsigned long elapsedMicros) {
  double acceleration = goalSpeed - currentSpeed;
  double maxAccelThisUpdate = MAX_ACCEL * elapsedMicros / 1000000.0;

  if (acceleration > maxAccelThisUpdate) {
    acceleration = maxAccelThisUpdate;
  }
  else if (acceleration < -maxAccelThisUpdate) {
    acceleration = -maxAccelThisUpdate;
  }
  
  //double elapsedSeconds = elapsedMicros / 1000000.0;
  double accelerationStep = acceleration;
  currentSpeed += accelerationStep;
  go(currentSpeed);
}


void checkLimitSwitch() {
  int hardLimit = digitalRead(HARDLIMITPIN);
  if (hardLimit==1) {
    if (currentSpeed > 0) {
      go(0);
      goalSpeed = 0;
    }
    stepperpos = 0;
  }
}

void checkPositionLimits() {
  if (stepperpos >= MAX_POSITION * STEPS_PER_CM) {
    if (currentSpeed < 0) {
      go(0);
      goalSpeed = 0;
    }
  }
}


// speed in cms per second (negative to go backwards)
// returns actual period
unsigned long go(float cps) {
  currentSpeed = cps;
  if (cps==0) {
    Timer1.pwm(PULSEPIN, 0);
    period = 0;
    return 0;
  }
  if (cps < 0) {
    cps = -cps;
    dir = 0;
  } else {
    dir = 1;
  }    
  
  digitalWrite(DIRPIN, dir);
  
  period = 1000000.0 / cps / STEPS_PER_CM;
  // try to get duty period to be about 50 us
  unsigned long duty = (1024.0 * 50.0) / period;
  if (duty < 1) duty = 1;
  else if (duty > 511) duty = 511;
  
  Timer1.pwm(PULSEPIN, duty, period);
  return period;
}

void countSteps() {
  if (!period) return;
  
  if (dir) stepperpos--;
  else stepperpos++;
}



// kalman from http://robottini.altervista.org/kalman-filter-vs-complementary-filter
float x_angle=0;
float x_angleC=0;
float x_angleRG=0;
float  x_angleRA=0;
float x_angle2C=0;

int val_gyro = 0;                    //value of individual accelerometer or gyroscope sensor
float accZ=0;
float accY=0;
float x1=0;
float y1=0;
float x2=0;
float y2=0;
