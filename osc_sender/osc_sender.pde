import oscP5.*;
import netP5.*;
import controlP5.*;

ControlP5 cp5;

OscP5 oscP5;
NetAddress myRemoteLocation;

int[] myLocalAddress = {192,168,2,1};


String state = "ok";

float STARTX = 0, STARTY = 0, STARTZ = 20;


int NUM_MOTORS = 4;

float HOMING_SPEED = 12.0;
float STARTUP_MAX_SPEED = 15.0; // go slow to home position
float MAX_SPEED = 50.0;  // approx cm/sec
boolean max_speed_sent = false;
float MAX_ACCEL = 250.0; // approx cm/sec/sec


// mouse interface
float mouseRange = 100;// +/- 100cm
float boxSize = 500;

// timeout
int lastMessageTime[] = new int[NUM_MOTORS]; 

// message counter
int messageNumber = 0;
int lastReceivedMessageNumber[] = new int[NUM_MOTORS];


// smoothing
class Smoother {
  double x=STARTX,y=STARTY,z=STARTZ;
  double goalx=STARTX, goaly=STARTY, goalz=STARTZ;
  double vx=0,vy=0,vz=0;
  double max_vel = 30.0; // in room units (cm for this test) per second
  //float max_acc = 50.0; // ditto

  void setPos(double _x, double _y, double _z) {
    x = _x;
    y = _y;
    z = _z;
  }
  
  void setGoal(double _x, double _y, double _z) {
    goalx = _x;
    goaly = _y;
    goalz = _z;
  }
  
  void update(int millis) {
    double dx = goalx - x;
    double dy = goaly - y;
    double dz = goalz - z;
    
    double scale = 1.0;
    double d = Math.sqrt(dx*dx + dy*dy + dz*dz);
    if (d==0) return;
    
    
    double max_dist = max_vel * millis / 1000.0;
    if (d > max_dist) {
      scale = max_dist / d;
    }
    
    vx = dx * scale;
    vy = dy * scale;
    vz = dz * scale;
    
    x += vx;
    y += vy;
    z += vz;  
  }
  
  

}



// origin of room coordinates
float CENTERPOINT[] = {0,0,0}; // at the floor, approx centered between motors



class Winchbot {
  int motorID;
  float x, y, z;
  float stepsPerDistanceUnit;   // how many winch steps for one distance unit (e.g. inch)
  float referencePoint;         // in winch units - a convenient point for measuring
  float offsetInDistanceUnits;  // distance between winch and payload when winch is at reference point 
  
  float supportX, supportY, supportZ;    // position of this winch's carriage support point relative to carriage reference point  

  Winchbot(int _motorID, float _x, float _y, float _z, float _steps, float _ref, float _offset, float _sx, float _sy, float _sz) {
    int reverse = -1;
    
    motorID = _motorID;
    x = _x;
    y = _y;
    z = _z;
    stepsPerDistanceUnit = reverse * _steps;
    referencePoint = _ref;
    offsetInDistanceUnits = _offset;
    
    supportX = _sx;
    supportY = _sy;
    supportZ = _sz;
  }
  
  // get distance from point, relative to CENTERPOINT
  float distanceFrom(float xr, float yr, float zr) {
    float x0 = xr + supportX + CENTERPOINT[0];
    float y0 = yr + supportY + CENTERPOINT[1];
    float z0 = zr + supportZ + CENTERPOINT[2];
    
    float dist = (float)Math.sqrt((x-x0)*(x-x0) + (y-y0)*(y-y0) + (z-z0)*(z-z0));
    //println("distanceFrom: " + x0 + "," + y0 + "," + z0 + " -> " + dist);
    return dist;
  }
  
  float positionToSteps(float xr, float yr, float zr) {
    float distance = distanceFrom(xr, yr, zr);
    float steps = (distance - offsetInDistanceUnits)* stepsPerDistanceUnit + referencePoint;

    return steps;
  }
  
  
}

Winchbot winches[] = new Winchbot[NUM_MOTORS];


// guessing that steps per inch
//    -- rough measurement 2000 steps = 4.25 inches or 470
//    more careful measurements: about 450-459 

float stepsPerInch = 454; 
float stepsPerCM = 179;

void setupWinches() {
  //NW                 motorID  pos in room             steps to rope length     support point 
  winches[0] = new Winchbot(3,  -304.5, 303.5, 357,   stepsPerCM, 56833, 505,     -10, 10, 0); // cm
  //NE
  winches[1] = new Winchbot(0,  304.5, 303.5, 357,    stepsPerCM, 55058, 503,      10, 10, 0);
  //SE
  winches[2] = new Winchbot(1,  304.5, -303.5, 357,   stepsPerCM, 54695, 512,      10, -10, 0);
  //SW
  winches[3] = new Winchbot(2,   -304.5, 303.5, 357,  stepsPerCM, 45187, 514,     -10, -10, 0);
}


Textlabel poslabels[] = new Textlabel[NUM_MOTORS];
Textlabel statelabels[] = new Textlabel[NUM_MOTORS];

Textlabel xlabel;// = new Textlabel();
Textlabel ylabel;// = new Textlabel();
Textlabel zlabel;// = new Textlabel();

Smoother smoother = new Smoother();

void setup() {
  setupWinches();
  
  size(800,600);
  cp5 = new ControlP5(this);


  cp5.addButton("STOP").setPosition(729,10).setSize(50,40);
  cp5.addButton("resume").setPosition(734,55).setSize(40,20);
  
  cp5.addButton("MOTORS_OFF").setPosition(729,110).setSize(50,40);
  cp5.addButton("MOTORS_ON").setPosition(734,155).setSize(40,20);
  
  cp5.addButton("CRASH_TEST").setPosition(729,200);
    
  for (int i=0; i<NUM_MOTORS; i++) {
    cp5.addButton("HOME"+i).setPosition(40+90*i, 10);
    poslabels[i] = cp5.addTextlabel("pos"+i).setPosition(60+90*i, 50);
    statelabels[i] = cp5.addTextlabel("state"+i).setPosition(60+90*i, 35);
  }
  
  xlabel = cp5.addTextlabel("xpos"+1).setPosition(10,500);
  ylabel = cp5.addTextlabel("ypos"+1).setPosition(10,515);
  zlabel = cp5.addTextlabel("zpos"+1).setPosition(10,530);
  
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,12000);
 
  myRemoteLocation = new NetAddress("192.168.2.255",12001); // broadcast address from ifconfig bridge100 listing
  //myRemoteLocation = new NetAddress("192.168.2.42",12001); // direct to motor0
  
  

  // tell motors to send /status directly to me instead of broadcast
  OscMessage myMessage = new OscMessage("/serveraddress");
  for (int i=0; i<4; i++) {
    myMessage.add(myLocalAddress[i]);
  }
  oscP5.send(myMessage, myRemoteLocation);
  
  // set movement parameters
  myMessage = new OscMessage("/maxspeed");
  myMessage.add(STARTUP_MAX_SPEED);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
  myMessage = new OscMessage("/maxaccel");
  myMessage.add(MAX_ACCEL);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
  myMessage = new OscMessage("/deadzone");
  myMessage.add(15);
  myMessage.add(4);
  oscP5.send(myMessage, myRemoteLocation);
  
  
}


int last_time = millis();

void draw() {
  //sendPos((int)(1000+2500*sin(millis()/350.0)));
  background(0);
  
  fill(80);
  rect(400-boxSize/2, 300-boxSize/2, boxSize, boxSize);
  
  if (state.equals("stopped")) {
    // stop sending updates and stop time
  }
  else {    
    int dt = millis() - last_time;
    
    smoother.update(dt);
    transmitPositions((float)smoother.x, (float)smoother.y, (float)smoother.z);
  }
  
  last_time = millis();
  fill(255);
  ellipse((float)(smoother.x+mouseRange/2.0 )* boxSize/mouseRange + (400-boxSize/2), 
    (float)(-smoother.y+mouseRange/2.0 )* boxSize/mouseRange + (300-boxSize/2), 
    10, 10);
    
    
  // check for uncommunicative motor
  int now = millis();
  for (int i=0; i<NUM_MOTORS; i++) {
    if (now - lastMessageTime[i] > 1000) {
      statelabels[i].setText("TIMEOUT!");
    }
  }
}

void transmitPositions(float x, float y, float z) {
  float positions[] = new float[NUM_MOTORS];
  for (int i=0; i<NUM_MOTORS; i++) {
    positions[winches[i].motorID] = winches[i].positionToSteps(x,y,z);
  }
  
  xlabel.setText("X=" + x);
  ylabel.setText("Y=" + y);
  zlabel.setText("Z=" + z);  
  
  /*
  println(x, y, z, " -> ", 
    winches[0].distanceFrom(x,y,z), winches[1].distanceFrom(x,y,z), 
    winches[2].distanceFrom(x,y,z), winches[3].distanceFrom(x,y,z),
    " ---> ",
    positions[0], positions[1], positions[2], positions[3]);
  */
  sendPos(positions[0], positions[1], positions[2], positions[3]);
 
}


// button handlers
public void HOME0(int val) {
  sendHome(0);
}  public void HOME1(int val) {
  sendHome(1);
}  
public void HOME2(int val) {
  sendHome(2);
}  public void HOME3(int val) {
  sendHome(3);
}  
public void STOP(int val) {
  OscMessage myMessage = new OscMessage("/stop");
  oscP5.send(myMessage, myRemoteLocation);
  state="stopped";
}
public void resume(int val) {
  OscMessage myMessage = new OscMessage("/resume");
  oscP5.send(myMessage, myRemoteLocation);
  state="ok";
}

public void MOTORS_OFF(int val) {
  OscMessage myMessage = new OscMessage("/motor");
  myMessage.add(0);
  oscP5.send(myMessage, myRemoteLocation);
  state="stopped";
}
public void MOTORS_ON(int val) {
  OscMessage myMessage = new OscMessage("/motor");
  myMessage.add(1);
  oscP5.send(myMessage, myRemoteLocation);
  state="ok";
}

public void CRASH_TEST(int val) {
  OscMessage myMessage = new OscMessage("/crashtest");
  oscP5.send(myMessage, myRemoteLocation);
}


void sendHome(int motorID) { 
  OscMessage myMessage = new OscMessage("/home");
  myMessage.add(motorID); // motor 0
  myMessage.add(HOMING_SPEED);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
}

float x=0, y=0, z = 0;
void keyPressed() {
  switch (key) {
    case 'i': z += 3;
      break;
      
    case 'k': z -= 3;
      break;
      
    default:
      return; // don't call mouseDragged
  }
  
  if (z < 20) z = 20;
  if (z > 300) z = 300;
  
  smoother.setGoal(x, y, z);
  
}

void mousePressed() {
  mouseDragged();

}

void mouseDragged() {
  if (state != "ok") return;
  
  if (mouseX < 400-boxSize/2 || mouseX > 400+boxSize/2 || mouseY < 300-boxSize/2 || mouseY > 300+boxSize/2) return;
  
  
  
  if (!max_speed_sent) {
    max_speed_sent = true;
    //for (int m=0; m<NUM_MOTORS; m++) {
      OscMessage myMessage = new OscMessage("/maxspeed");
      //myMessage.add(m); // motor 0
      myMessage.add(MAX_SPEED);  // speed must be float!
      oscP5.send(myMessage, myRemoteLocation);
    //}
  }
  
  float range = mouseRange; 
  
  x = -range/2.0 + ((mouseX-(400-boxSize/2.0)) / boxSize)*range;
  y = -range/2.0 + (1.0 - (mouseY-(300-boxSize/2.0)) / boxSize)*range;
  //println("------",(mouseX-(400-boxSize/2.0)), mouseY-(300-boxSize/2.0),x,y);

  
  if (x < -range/2) x = -range/2;
  if (x > range/2) x = range/2;
  if (y < -range/2) y = -range/2;
  if (y > range/2) y = range/2;
  if (z < 20) z = 20;
  if (z > 300) z = 300;
  
  smoother.setGoal(x, y, z);
  
  
}


boolean sendPos(float pos0, float pos1, float pos2, float pos3) {
  if (pos0 < 0 || pos1 < 0 || pos2 < 0 || pos3 < 0) return false;
  
  
  
  OscMessage myMessage = new OscMessage("/go");
  //println("Sending /go/" + pos);
 // myMessage.add(pos0);
  //myMessage.add(pos1);
  
  
  
  myMessage.add(pos0);
  myMessage.add(pos1);
  myMessage.add(pos2);
  myMessage.add(pos3);
  myMessage.add(messageNumber++);
  
  if (messageNumber > 32000) messageNumber = 0;
  
  oscP5.send(myMessage, myRemoteLocation);
  
  return true;
}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {
  /* print the address pattern and the typetag of the received OscMessage */
  
  /*
  print("### received an osc message.");
  print(" addrpattern: "+theOscMessage.addrPattern());
  println(" typetag: "+theOscMessage.typetag());
  */
  
  String addr = theOscMessage.addrPattern();
  if (addr.equals("/status")) {
    int motor = theOscMessage.get(0).intValue();
    String state = theOscMessage.get(1).stringValue();
    
    float pos = theOscMessage.get(2).floatValue();
    float speed = theOscMessage.get(3).floatValue();
    
    int steps = theOscMessage.get(4).intValue();
    int encs = theOscMessage.get(5).intValue();

    if (theOscMessage.arguments().length >= 7) {  
      int serial = theOscMessage.get(6).intValue();
      int skipped = serial - lastReceivedMessageNumber[motor];
      lastReceivedMessageNumber[motor] = serial;
    
      if (skipped != 1) {
        println("MOTOR " + motor + " serial skipped " + skipped);
      }
    }

    /*
    if (encs < steps || encs > steps * 8) {
      println("SLIP!");
    }
    */
    
    poslabels[motor].setText(""+pos);
    statelabels[motor].setText(state);
    lastMessageTime[motor] = millis();
    
    /*
    println("Motor " + motor + " " + state + " pos " + pos + " and speed " + speed + " steps " 
      + steps + " encs " + encs);
    */
  }
  else if (addr.equals("/crashreport")) {
    int motor, address, data;
    if (theOscMessage.arguments().length == 2) {
      // obsolete crash report
      motor = -1; //theOscMessage.get(0).intValue();
      address = theOscMessage.get(0).intValue();
      data = theOscMessage.get(1).intValue();
      println("CRASH REPORT motor unknown, address " + address + " words, " + (address*2) + " bytes, data " + data);
    } else if (theOscMessage.arguments().length==3) {
      motor = theOscMessage.get(0).intValue();
      address = theOscMessage.get(1).intValue();
      data = theOscMessage.get(2).intValue();
      println("CRASH REPORT motor " + motor + " address " + address + " words, " + (address*2) + " bytes, data " + data);
          
    }
    else {
      print("CRASH REPORT?? ");
      for (int i=0; i<theOscMessage.arguments().length; i++) {
        print("arg " + i + "=" + theOscMessage.get(i).intValue() + "  ");
      }
      println();
    }

    
  }
  else println("HEY, unknown message from a motor: " + addr);
}
