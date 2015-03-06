
// quadrature encoder example with dual interrupts
//
// modified from http://playground.arduino.cc/Main/RotaryEncoders#Example3
// (changed position variable to signed long)

// TimerOne library from pjrc
// http://www.pjrc.com/teensy/td_libs_TimerOne.html
#include <TimerOne.h>


// PID_v1 library
// https://github.com/br3ttb/Arduino-PID-Library/
// http://playground.arduino.cc/Code/PIDLibrary
#include <PID_v1.h>


// David Bouchard's arduino-osc library
// http://www.deadpixel.ca/arduino-osc/
// https://github.com/davidbouchard/arduino-osc
#include <OscUDP.h>



// Ethernet libraries
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// EEPROM library
#include <EEPROM.h>


// IMPORTANT SETTINGS ----------------------------

// network:
const int LOCALNET = 2; // 2 for osx internet sharing 192.168.2.*, 1 for NYCR network 192.168.1.*


// PID SETUP --------------------------
double pidSetpoint, pidInput, pidOutput;
const double consKp=0.08, consKi=0.012, consKd=0.00001;
PID myPID(&pidInput, &pidOutput, &pidSetpoint, consKp, consKi, consKd, REVERSE); // DIRECT or REVERSE
const int CLOSE_ENOUGH = 15; // stop motor if within +/- desired position in encoder steps


// ENCODER SETUP ---------------------------
// encoder on 2 and 3
const int encoder0PinA = 2;
const int encoder0PinB = 3;
const int encoder0PinZ = 4;
volatile long encoder0Pos = 0;
volatile long encoder0Zero = 0;
volatile long encoder0ZeroState = 0;


// MOTOR DRIVER SETUP ---------------------------
// stepper driver pins on ARDUINO ETHERNET 
const int PULSEPIN = 9; // yellow
const int DIRPIN = 8;  // green
const int ENAPIN = 7;  // white


// MOTOR SETUP -------
const float WHEEL_RADIUS = (2.0 + 0.125) * 2.54; // 2 inch wheel + 1/8" grommet, in cm

const float CM_PER_REV = TWO_PI * WHEEL_RADIUS;
const int STEPS_PER_REV = 1600;
const float CM_PER_STEP = CM_PER_REV / STEPS_PER_REV;
const float STEPS_PER_CM = STEPS_PER_REV / CM_PER_REV;

float MAX_ACCEL = 500.0; // in approx cm/sec^2

float MAX_SPEED = 30.0; // in approx cm/sec

double goalSpeed = 0;

// MOTOR TIMING/POS
volatile long stepperpos = 0;
volatile int dir = 0;
volatile unsigned long period;
double currentSpeed = 0;



// NETWORK SETUP  -------

// get motor id (0 for NW motor, 1 for NE, 2 for SE, 3 for SW) from EEPROM address 0
int MOTOR_ID = EEPROM.read(0);

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, MOTOR_ID };
IPAddress listeningIP(192,168,LOCALNET,42+MOTOR_ID); // you need to set this

unsigned int listeningPort = 12001;      // local port to listen on

NetAddress destination;
//IPAddress destinationIP( 255,255,255,255 ); // 255... is broadcast address according to http://forum.arduino.cc/index.php?topic=164119.0
IPAddress destinationIP( 192,168,LOCALNET,255 ); // this is broadcast address when using osx internet sharing, according to ifconfig listing for bridge100
int destinationPort = 12000;

EthernetUDP UDP;
OscUDP etherOSC;  







void setup() {
  setupEthernet();
  setupEncoder();
  setupMotorDriver(); 
  setupPID();
 
  //Serial.begin (115200);
}



unsigned long lastmicros = micros();
long laststepperpos = 0, lastencoder0Pos = 0;
int printcounter = 0;

void loop(){ 
  // PID loop
  pidInput = encoder0Pos;
  if (abs(pidInput-pidSetpoint) < CLOSE_ENOUGH) {
    goalSpeed = 0;
  }
  else {
    myPID.Compute();
    goalSpeed = pidOutput;
  }
    
  // update speed taking acceleration limit into account
  unsigned long dt = micros() - lastmicros;
  lastmicros = micros();
  
  updateSpeed(dt);
  
  
  
  // check for slip
  long stepper_steps_this_update = stepperpos - laststepperpos;
  laststepperpos = stepperpos;
  
  long encoder_steps_this_update = encoder0Pos - lastencoder0Pos;
  lastencoder0Pos = encoder0Pos;
  
  
  
  
  
  
  // check for incoming messages 
  etherOSC.listen();
  
  
  // send updates 
  if (printcounter==0) {
    sendOscStatus(stepper_steps_this_update, encoder_steps_this_update);
  }
  if (printcounter++ > 100) printcounter = 0;
  
  
  
  
  delay(1);
}






void setupEthernet() {
  Ethernet.begin(mac,listeningIP);
  UDP.begin(listeningPort);
  etherOSC.begin(UDP);
  
  destination.set(destinationIP, destinationPort);
}


void setupPID() {
  pidSetpoint = 0;
  myPID.SetOutputLimits(-MAX_SPEED, MAX_SPEED);
  myPID.SetSampleTime(20);
  myPID.SetMode(AUTOMATIC);
}

void pidSetMaxSpeed(float ms) {
  myPID.SetOutputLimits(-ms, ms);
}

void setupEncoder() {
  pinMode(encoder0PinA, INPUT); 
  pinMode(encoder0PinB, INPUT); 
  pinMode(encoder0PinZ, INPUT);
  // encoder pin on interrupt 0 (pin 2)
  attachInterrupt(0, doEncoderA, CHANGE);
  // encoder pin on interrupt 1 (pin 3)
  attachInterrupt(1, doEncoderB, CHANGE);  
}

void setupMotorDriver() {
  //pinMode(PULSEPIN, OUTPUT); pwm() will do this
  pinMode(DIRPIN, OUTPUT);
  pinMode(ENAPIN, OUTPUT);
  
  digitalWrite(ENAPIN, 1); // enable motor driver
  
  // set up interrupts for motor speed control
  Timer1.initialize(1000);
  Timer1.pwm(PULSEPIN, 0);
  Timer1.start();
  
  Timer1.attachInterrupt(countSteps);
}


// MOTOR HANDLERS ----------------------------------------

// move actual speed towards goal speed
// dt is time since last update in microseconds
void updateSpeed(unsigned long dt) {
  float requestedDS = goalSpeed - currentSpeed;
  float maxDS = MAX_ACCEL * dt / 1000000.0;
  if (requestedDS > maxDS) requestedDS = maxDS;
  else if (requestedDS < -maxDS) requestedDS = -maxDS;
  
  go(currentSpeed + requestedDS);  
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




// ENCODER HANDLERS -----------------------------



void doEncoderA(){

  // look for a low-to-high on channel A
  if (digitalRead(encoder0PinA) == HIGH) { 
    // check channel B to see which way encoder is turning
    if (digitalRead(encoder0PinB) == LOW) {  
      encoder0Pos = encoder0Pos + 1;         // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }
  else   // must be a high-to-low edge on channel A                                       
  { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(encoder0PinB) == HIGH) {   
      encoder0Pos = encoder0Pos + 1;          // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
  
  // mark Z
  int zeropin = digitalRead(encoder0PinZ);
  if (encoder0ZeroState == 0 && zeropin == 1) {
    encoder0Zero = encoder0Pos;
  }
  encoder0ZeroState = zeropin;
}

void doEncoderB(){

  // look for a low-to-high on channel B
  if (digitalRead(encoder0PinB) == HIGH) {   
   // check channel A to see which way encoder is turning
    if (digitalRead(encoder0PinA) == HIGH) {  
      encoder0Pos = encoder0Pos + 1;         // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }
  // Look for a high-to-low on channel B
  else { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(encoder0PinA) == LOW) {   
      encoder0Pos = encoder0Pos + 1;          // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
}




// OSC MESSAGE HANDLERS -----------------------


// send status message

void sendOscStatus(long stepper, long encoder) {
  OscMessage msg("/status");
 
  msg.add(MOTOR_ID);
  msg.add("OK");
  msg.add((float)encoder0Pos); 
  msg.add(currentSpeed);
  msg.add(stepper);
  msg.add(encoder);
  
  etherOSC.send(msg, destination);
}

void oscEvent(OscMessage &m) { 
  m.plug("/go", oscGo); 
  m.plug("/maxspeed", oscSetMaxSpeed); 
}


void oscGo(OscMessage &m) {
  // /go/nwPos,nePos,sePos,swPos long ints
  int value = m.getFloat(MOTOR_ID);
  pidSetpoint = value;
}

void oscSetMaxSpeed(OscMessage &m) {
  int motor = m.getInt(0);
  if (motor==MOTOR_ID) {
    pidSetMaxSpeed(m.getFloat(1));
  }
}



