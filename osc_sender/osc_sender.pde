import oscP5.*;
import netP5.*;
import controlP5.*;

ControlP5 cp5;

OscP5 oscP5;
NetAddress myRemoteLocation;

int NUM_MOTORS = 4;

float HOMING_SPEED = 12.0;
float MAX_SPEED = 20.0;



// origin of room coordinates
float CENTERPOINT[] = {40, 55, 0}; // on the floor, approx centered between motors
// table height is about 45, will bump table below z=45

class Winchbot {
  int motorID;
  float x, y, z;
  float stepsPerDistanceUnit;   // how many winch steps for one distance unit (e.g. inch)
  float referencePoint;         // in winch units - a convenient point for measuring
  float offsetInDistanceUnits;  // distance between winch and payload when winch is at reference point 

  Winchbot(int _motorID, float _x, float _y, float _z, float _steps, float _ref, float _offset) {
    int reverse = -1;
    
    motorID = _motorID;
    x = _x;
    y = _y;
    z = _z;
    stepsPerDistanceUnit = reverse * _steps;
    referencePoint = _ref;
    offsetInDistanceUnits = _offset;
  }
  
  // get distance from point, relative to CENTERPOINT
  float distanceFrom(float xr, float yr, float zr) {
    float x0 = xr + CENTERPOINT[0];
    float y0 = yr + CENTERPOINT[1];
    float z0 = zr + CENTERPOINT[2];
    
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

Winchbot winches[] = new Winchbot[4];


// guessing that steps per inch
//    -- rough measurement 2000 steps = 4.25 inches or 470
//    more careful measurements: about 450-459 

float stepsPerInch = 454; 

void setupWinches() {
  //NW
  winches[0] = new Winchbot(3,  4, 111, 69,   stepsPerInch, 13400, 97); // inches
  //NE
  winches[1] = new Winchbot(0,  80, 111, 69,  stepsPerInch, 13200, 79);
  //SE
  winches[2] = new Winchbot(1,  78, 0, 69,    stepsPerInch, 13200, 60);
  //SW
  winches[3] = new Winchbot(2,   0, 0, 69,    stepsPerInch, 13290, 58);
}


Textlabel poslabels[] = new Textlabel[4];
Textlabel statelabels[] = new Textlabel[4];

void setup() {
  setupWinches();
  
  size(800,600);
  cp5 = new ControlP5(this);

    
  for (int i=0; i<4; i++) {
    cp5.addButton("HOME"+i).setPosition(40+90*i, 40);
    poslabels[i] = cp5.addTextlabel("pos"+i).setPosition(60+90*i, 80);
    statelabels[i] = cp5.addTextlabel("state"+i).setPosition(60+90*i, 65);
  }
  
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,12000);
 
  myRemoteLocation = new NetAddress("192.168.2.255",12001); // broadcast address from ifconfig bridge100 listing
  //myRemoteLocation = new NetAddress("192.168.2.42",12001); // direct to motor0
  
  
  for (int m=0; m<NUM_MOTORS; m++) {
    OscMessage myMessage = new OscMessage("/maxspeed");
    myMessage.add(m); // motor 0
    myMessage.add(MAX_SPEED);  // speed must be float!
    oscP5.send(myMessage, myRemoteLocation);
  }
}

void draw() {
  //sendPos((int)(1000+2500*sin(millis()/350.0)));
  background(0);
}


// button handlers
public void HOME0(int val) {
  println("HOME 0 BUTTON");
  sendHome(0);
}  public void HOME1(int val) {
  println("HOME 1 BUTTON");
  sendHome(1);
}  
public void HOME2(int val) {
  println("HOME 2 BUTTON");
  sendHome(2);
}  public void HOME3(int val) {
  println("HOME 3 BUTTON");
  sendHome(3);
}  

void sendHome(int motorID) { 
  OscMessage myMessage = new OscMessage("/home");
  myMessage.add(motorID); // motor 0
  myMessage.add(HOMING_SPEED);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
}

float z = 52;
void keyPressed() {
  println("keyPressed: ", key);
  switch (key) {
    case 'i': z += 1;
      break;
      
    case 'k': z -= 1;
      break;
  }
  mouseDragged();
}

void mousePressed() {
  mouseDragged();
}

void mouseDragged() {
  
  float range = 24; 
  
  float x = -range/2.0 + (mouseX / 800.0)*range;
  float y = -range/2.0 + (mouseY / 600.0)*range;
  //float z = 43;
  
  if (x < -range/2) x = -range/2;
  if (x > range/2) x = range/2;
  if (y < -range/2) y = -range/2;
  if (y > range/2) y = range/2;
  if (z < 42) z = 42;
  if (z > 72) z = 72;
  
  float positions[] = new float[4];
  for (int i=0; i<4; i++) {
    positions[winches[i].motorID] = winches[i].positionToSteps(x,y,z);
  }
      
  
  println(x, y, z, " -> ", 
    winches[0].distanceFrom(x,y,z), winches[1].distanceFrom(x,y,z), 
    winches[2].distanceFrom(x,y,z), winches[3].distanceFrom(x,y,z),
    " ---> ",
    positions[0], positions[1], positions[2], positions[3]);
  
  sendPos(positions[0], positions[1], positions[2], positions[3]);
  
  
  //sendPos(400,400,400,mouseY*100);
  
}


void sendPos(float pos0, float pos1, float pos2, float pos3) {
  OscMessage myMessage = new OscMessage("/go");
  //println("Sending /go/" + pos);
 // myMessage.add(pos0);
  //myMessage.add(pos1);
  
  
  
  myMessage.add(pos0);
  myMessage.add(pos1);
  myMessage.add(pos2);
  myMessage.add(pos3);
  
  oscP5.send(myMessage, myRemoteLocation);
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
    
    if (encs < steps || encs > steps * 8) {
      println("SLIP!");
    }
  
    
    poslabels[motor].setText(""+pos);
    statelabels[motor].setText(state);
    
    println("Motor " + motor + " " + state + " pos " + pos + " and speed " + speed + " steps " 
      + steps + " encs " + encs);
  }
  else println("hey");
}
