import oscP5.*;
import netP5.*;
import controlP5.*;

ControlP5 cp5;

OscP5 oscP5;
NetAddress myRemoteLocation;

int NUM_MOTORS = 4;

float HOMING_SPEED = 12.0;
float STARTUP_MAX_SPEED = 15.0; // go slow to home position
float MAX_SPEED = 60.0;  // approx cm/sec
boolean max_speed_sent = false;
float MAX_ACCEL = 300.0; // approx cm/sec/sec


// mouse interface
float mouseRange = 36;
float boxSize = 500;


// smoothing
class Smoother {
  double x=0,y=0,z=52;
  double goalx=0, goaly=0, goalz=52;
  double vx=0,vy=0,vz=0;
  double max_vel = 24.0; // in room units (inches for this test) per second
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
float CENTERPOINT[] = {40, 55, 0}; // on the floor, approx centered between motors
// table height is about 45, will bump table below z=45


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

Winchbot winches[] = new Winchbot[4];


// guessing that steps per inch
//    -- rough measurement 2000 steps = 4.25 inches or 470
//    more careful measurements: about 450-459 

float stepsPerInch = 454; 

void setupWinches() {
  //NW                 motorID  pos in room   rope length calibration    support point 
  winches[0] = new Winchbot(3,  4, 111, 69,   stepsPerInch, 27355, 68    , -1, 1, 0); // inches
  //winches[0] = new Winchbot(3,  -7, 108.75, 69,   stepsPerInch, 13400, 97    , -1, 1, 0); // inches
  //NE
  winches[1] = new Winchbot(0,  80, 111, 69,  stepsPerInch, 24229, 54.5    , 1, 1, 0);
  //winches[1] = new Winchbot(0,  73.75, 111, 69,  stepsPerInch, 13200, 79    , 1, 1, 0);
  //SE
  //winches[2] = new Winchbot(1,  78, 0, 69,    stepsPerInch, 13200, 60    , 1, -1, 0);
  winches[2] = new Winchbot(1,  78, 0, 69,    stepsPerInch, 7865, 72    , 1, -1, 0);
  //winches[2] = new Winchbot(1,  74.5, 0, 69,    stepsPerInch, 13200, 60    , 1, -1, 0);
  //SW
  winches[3] = new Winchbot(2,   0, 0, 69,    stepsPerInch, 6820, 72.75    , -1, -1, 0);
  //winches[3] = new Winchbot(2,   0, 0, 69,    stepsPerInch, 13290, 58    , -1, -1, 0);
}


Textlabel poslabels[] = new Textlabel[4];
Textlabel statelabels[] = new Textlabel[4];
Smoother smoother = new Smoother();

void setup() {
  setupWinches();
  
  size(800,600);
  cp5 = new ControlP5(this);

    
  for (int i=0; i<4; i++) {
    cp5.addButton("HOME"+i).setPosition(40+90*i, 10);
    poslabels[i] = cp5.addTextlabel("pos"+i).setPosition(60+90*i, 50);
    statelabels[i] = cp5.addTextlabel("state"+i).setPosition(60+90*i, 35);
  }
  
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,12000);
 
  myRemoteLocation = new NetAddress("192.168.2.255",12001); // broadcast address from ifconfig bridge100 listing
  //myRemoteLocation = new NetAddress("192.168.2.42",12001); // direct to motor0
  
  
  for (int m=0; m<NUM_MOTORS; m++) {
    OscMessage myMessage = new OscMessage("/maxspeed");
    myMessage.add(m); // motor 0
    myMessage.add(STARTUP_MAX_SPEED);  // speed must be float!
    oscP5.send(myMessage, myRemoteLocation);
    
    myMessage = new OscMessage("/maxaccel");
    myMessage.add(m); // motor 0
    myMessage.add(MAX_ACCEL);  // speed must be float!
    oscP5.send(myMessage, myRemoteLocation);
    
    
  }
}


int last_time = millis();

void draw() {
  //sendPos((int)(1000+2500*sin(millis()/350.0)));
  background(0);
  
  fill(80);
  rect(400-boxSize/2, 300-boxSize/2, boxSize, boxSize);
  
  int dt = millis() - last_time;
  last_time = millis();
  
  //   float x = -range/2.0 + (mouseX / 800.0)*range;
  //  float y = -range/2.0 + (1.0 - mouseY / 600.0)*range;
  
  fill(255);
  ellipse((float)(smoother.x+mouseRange/2.0 )* boxSize/mouseRange + (400-boxSize/2), 
    (float)(-smoother.y+mouseRange/2.0 )* boxSize/mouseRange + (300-boxSize/2), 
    10, 10);
  smoother.update(dt);
  transmitPositions((float)smoother.x, (float)smoother.y, (float)smoother.z);
}

void transmitPositions(float x, float y, float z) {
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
      
    default:
      return; // don't call mouseDragged
  }
  mouseDragged();
}

void mousePressed() {
  mouseDragged();

}

void mouseDragged() {
  if (mouseX < 400-boxSize/2 || mouseX > 400+boxSize/2 || mouseY < 300-boxSize/2 || mouseY > 300+boxSize/2) return;
  
  
  
  if (!max_speed_sent) {
    max_speed_sent = true;
    for (int m=0; m<NUM_MOTORS; m++) {
      OscMessage myMessage = new OscMessage("/maxspeed");
      myMessage.add(m); // motor 0
      myMessage.add(MAX_SPEED);  // speed must be float!
      oscP5.send(myMessage, myRemoteLocation);
    }
  }
  
  float range = mouseRange; 
  
  float x = -range/2.0 + ((mouseX-(400-boxSize/2.0)) / boxSize)*range;
  float y = -range/2.0 + (1.0 - (mouseY-(300-boxSize/2.0)) / boxSize)*range;
  //println("------",(mouseX-(400-boxSize/2.0)), mouseY-(300-boxSize/2.0),x,y);

  
  if (x < -range/2) x = -range/2;
  if (x > range/2) x = range/2;
  if (y < -range/2) y = -range/2;
  if (y > range/2) y = range/2;
  if (z < 36) z = 36;
  if (z > 60) z = 60;
  
  smoother.setGoal(x, y, z);
  
  /*
  
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
  
  
  */
  
  //sendPos(400,400,400,mouseY*100);
  
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
