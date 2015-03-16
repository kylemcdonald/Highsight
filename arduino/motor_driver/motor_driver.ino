
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

// Watchdog Timer and crash reports derived from ArduinoCrashMonitor by MegunoLink
// https://github.com/Megunolink/ArduinoCrashMonitor
// http://www.megunolink.com/how-to-detect-lockups-using-the-arduino-watchdog/
#include "ApplicationMonitor.h"


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
// Dead zone: stop motor if within +/- desired position in encoder steps
int STILL_DEAD_ZONE = 15; // when desired velocity is 0, big dead zone
int MOVING_DEAD_ZONE = 2; // when moving, smaller dead zone (so slow movements aren't jerky as they jump from one dead zone to the next)


// ENCODER SETUP ---------------------------
// encoder on 2 and 3   // +5 is brown, ground is blue
const int encoder0PinA = 2;  // white
const int encoder0PinB = 3;  // black
const int encoder0PinZ = 4;  // orange
volatile long encoder0Pos = 0;
volatile long encoder0Zero = 0;
volatile long encoder0ZeroState = 0;

// ENDSTOP SETUP ---------------------------
const int EXTENSIONENDSTOPPIN = 6; // endstop for max extension, wired Normally Closed


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
float homingSpeed = 3.0;
bool homed = false;

const int BACKOFF_STEPS = 600; // how many encoder steps to reverse out of endstop (so Zero is this far from the switch)


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

int MSEC_PER_STATUS = 50; // millseconds between sending status messages

// WATCHDOG TIMER ---------
Watchdog::CApplicationMonitor ApplicationMonitor;




// SYSTEM STATE
enum stateEnum {
  NOTHOMED,         // don't know actual position
  HOMING,           // currently seeking home
  HOMINGBACKOFF,    // found home switch, backing off a bit
  HOMINGERROR,      // couldn't find home for some reason
  OK,               // all is well, ready for motion commands
  STOPPED,          // stopped by /stop command
  ENDSTOP,          // unexpectedly hit the end stop, need to home again
  MOTOROFF         // motor power off by /disable command
} state = MOTOROFF;





void setup() {
  setupEthernet();
  setupEncoder();
  setupMotorDriver(); 
  setupEndstops();
  setupPID();
  
  setupWatchdog();
}



unsigned long lastmicros = micros();
unsigned long lastStatusMsgMillis = millis();
long laststepperpos = 0, lastencoder0Pos = 0;
double lastSetpoint = 0;

void loop(){ 
  
  // call off the watchdog
  ApplicationMonitor.IAmAlive();
  ApplicationMonitor.SetData(state);

  
  
  if (state==OK) {
    // PID loop
    pidInput = encoder0Pos;
    // use big dead zone only if setpoint hasn't changed; ie desired speed is 0
    if (
         (lastSetpoint == pidSetpoint && abs(pidInput-pidSetpoint) < STILL_DEAD_ZONE)
      || (lastSetpoint != pidSetpoint && abs(pidInput-pidSetpoint) < MOVING_DEAD_ZONE)
       ) 
    {
      goalSpeed = 0;
    }
    else {
      myPID.Compute();
      goalSpeed = pidOutput;
    }
      
  }
  else if (state==NOTHOMED) {
    goalSpeed = 0;
  }
  lastSetpoint = pidSetpoint;
  
  
  if (state==HOMING) {
    goalSpeed = homingSpeed;
  }
  
  else if (state==HOMINGBACKOFF) {
    goalSpeed = -homingSpeed;
    go(goalSpeed);
    
    if (encoder0Pos > 0) {
      goalSpeed = 0;
      pidSetpoint = encoder0Pos;
      state = OK;
      homed = true;
    }
  }

  // check end stop
  int endstop = digitalRead(EXTENSIONENDSTOPPIN);
  
  bool manualSpeed = false;
  
  if (endstop) {
    if (state==HOMING) {
      state=HOMINGBACKOFF;
      encoder0Pos = -BACKOFF_STEPS;
      manualSpeed = true;
    }
    else if (state==HOMINGBACKOFF) {
      manualSpeed = true;
    }
    else if (state==MOTOROFF) {
      manualSpeed = true;
    }
    
    else {
      state=ENDSTOP;
      goalSpeed = 0;
    }
  }
  
  
  if (!manualSpeed) {
    // update speed from goalspeed taking acceleration limit into account
    unsigned long dt = micros() - lastmicros;
    lastmicros = micros();
    
    updateSpeed(dt);
  }
  
  

  
  
  
  // check for incoming messages 
  etherOSC.listen();
  
  
  // send updates 
  if (millis() - lastStatusMsgMillis >= MSEC_PER_STATUS) {
    lastStatusMsgMillis = millis();
    
    // send stepper and encoder steps so server can check for slip
    long stepper_steps_this_update = stepperpos - laststepperpos;
    laststepperpos = stepperpos;
    
    long encoder_steps_this_update = encoder0Pos - lastencoder0Pos;
    lastencoder0Pos = encoder0Pos;
    
    sendOscStatus(stepper_steps_this_update, encoder_steps_this_update);
  }
  
  
  
  
}




void setupWatchdog() {
  OscMessage msg("/crashreport");
  ApplicationMonitor.Dump(msg);
  ApplicationMonitor.EnableWatchdog(Watchdog::CApplicationMonitor::Timeout_4s);
  etherOSC.send(msg, destination);
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
  
  digitalWrite(ENAPIN, 0); // disable motor driver
  
  // set up interrupts for motor speed control
  Timer1.initialize(1000);
  Timer1.pwm(PULSEPIN, 0);
  Timer1.start();
  
  Timer1.attachInterrupt(countSteps);
}

void setupEndstops() {
  // endstop switch is NC and wired to ground, so switch activation will make input HIGH
  pinMode(EXTENSIONENDSTOPPIN, INPUT_PULLUP); 
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


// enable or disable motor
void motorEnable(bool power) {
  digitalWrite(ENAPIN, power);
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
  
  switch(state) {
    case NOTHOMED:  
      msg.add("NOTHOMED");
      break;
    case HOMING:
      msg.add("HOMING");
      break;    
    case HOMINGBACKOFF:
      msg.add("HOMINGBACKOFF");
      break;
    case ENDSTOP:  
      msg.add("ENDSTOP");
      break;
    case OK:  
      msg.add("OK");
      break;
    case STOPPED:
      msg.add("STOPPED");
      break;
    case MOTOROFF:
      msg.add("MOTOROFF");
      break;
      
    default:  
       msg.add("UNKNOWN");
       break;
  }
  
  msg.add((float)encoder0Pos); 
  msg.add(currentSpeed);
  msg.add(stepper);
  msg.add(encoder);
  
  etherOSC.send(msg, destination);
}

void oscEvent(OscMessage &m) { 
  m.plug("/go", oscGo); 
  m.plug("/go2", oscGo2);
  m.plug("/home", oscHome);
  m.plug("/maxspeed", oscSetMaxSpeed); 
  m.plug("/maxaccel", oscSetMaxAccel);
  m.plug("/deadzone", oscSetDeadZone);
  
  m.plug("/stop", oscStop);
  m.plug("/resume", oscResume);
  
  m.plug("/motor", oscSetMotorPower);
  
  m.plug("/crashtest", oscCrash);
}


void oscGo(OscMessage &m) {
  // /go/motor0pos,motor1pos,motor2pos,motor3pos long ints
  if (state != OK) return; 
  
  double value = m.getFloat(MOTOR_ID);
  pidSetpoint = value;
  pidSetMaxSpeed(MAX_SPEED);
}


void oscGo2(OscMessage &m) {
  if (state != OK) return;
  
  double pos = m.getFloat(MOTOR_ID*2);
  double spd = m.getFloat(MOTOR_ID*2+1);
  pidSetpoint = pos;
  pidSetMaxSpeed(spd);
}



void oscHome(OscMessage &m) {
  if (state==MOTOROFF) return;
  
  int motor = m.getInt(0);
  if (motor==MOTOR_ID && state!=HOMING) {
    state = HOMING;
    homingSpeed = m.getFloat(1);
  }
}

void oscSetMaxSpeed(OscMessage &m) {
  if (m.size()==1 || (m.size()==2 && m.getInt(0)==MOTOR_ID)) {
    MAX_SPEED = m.getFloat(m.size()-1);
    pidSetMaxSpeed(MAX_SPEED);
  }
}


void oscSetMaxAccel(OscMessage &m) {
  if (m.size()==1 || (m.size()==2 && m.getInt(0)==MOTOR_ID)) {
    MAX_ACCEL = m.getFloat(m.size()-1);
  }
}

// /deadzone [motorID] stillDeadZone movingDeadZone
void oscSetDeadZone(OscMessage &m) {
  if (m.size()==2 || (m.size()==3 && m.getInt(0)==MOTOR_ID)) {
    STILL_DEAD_ZONE = m.getInt(m.size()-2);
    MOVING_DEAD_ZONE = m.getInt(m.size()-1);
  }
}


void oscSetStatusInterval(OscMessage &m) {
  if (m.size()==1 || (m.size()==2 && m.getInt(0)==MOTOR_ID)) {
    int msec = m.getInt(m.size()-1);
    if (msec<3 || msec>200) return;
    MSEC_PER_STATUS = msec;
  }
}



// STOP: 
void oscStop(OscMessage &m) {
  if (m.size()==0 || (m.size()==1 && m.getInt(0)==MOTOR_ID)) {
    if (state==OK) state = STOPPED;
    else if (state==HOMING || state==HOMINGBACKOFF) state = NOTHOMED;
  }
  goalSpeed = 0;
}

// RESUME: 
void oscResume(OscMessage &m) {
  if (m.size()==0 || (m.size()==1 && m.getInt(0)==MOTOR_ID)) {
    if (state==STOPPED) state = OK; 
  }
}


// MOTOR POWER:
void oscSetMotorPower(OscMessage &m) {
  if (m.size()==1 || (m.size()==2 && m.getInt(0)==MOTOR_ID)) {
    int p = m.getInt(m.size()-1);
    if (p) {
      if (state==MOTOROFF) {
        if (homed) state = OK;
        else state = NOTHOMED;
      }
      motorEnable(true);
    }
    else {
      state = MOTOROFF;
      motorEnable(false);
    }
  }
}

// WATCHDOG TEST
void oscCrash(OscMessage &m) {
  delay(5000); // watchdog timer is 4 seconds so 5 second delay should trigger it
}





