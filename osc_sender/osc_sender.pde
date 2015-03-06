import oscP5.*;
import netP5.*;
import controlP5.*;

ControlP5 cp5;

OscP5 oscP5;
NetAddress myRemoteLocation;


void setup() {
  size(800,600);
  cp5 = new ControlP5(this);
  
  cp5.addButton("HOME")
    .setPosition(40,40)
    .setSize(80,20);
  
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,12000);
 
  myRemoteLocation = new NetAddress("192.168.2.255",12001); // broadcast address from ifconfig bridge100 listing
  //myRemoteLocation = new NetAddress("192.168.2.42",12001); // direct to motor0
  
  
  OscMessage myMessage = new OscMessage("/maxspeed");
  myMessage.add(0); // motor 0
  myMessage.add(70.0);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
}

void draw() {
  //sendPos((int)(1000+2500*sin(millis()/350.0)));
}


// button handlers
public void HOME(int val) {
  println("HOME BUTTON");
  
  OscMessage myMessage = new OscMessage("/home");
  myMessage.add(0); // motor 0
  myMessage.add(3.0);  // speed must be float!
  oscP5.send(myMessage, myRemoteLocation);
  
}


void mousePressed() {
  sendPos(mouseY*10);
}

void mouseDragged() {
  sendPos(mouseY*10);
}

void sendPos(float pos) {
  OscMessage myMessage = new OscMessage("/go");
  //println("Sending /go/" + pos);
  for (int i=0; i<4; i++) {
    myMessage.add(pos);
  }
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
  
    
    println("Motor " + motor + " " + state + " pos " + pos + " and speed " + speed + " steps " 
      + steps + " encs " + encs);
  }
  else println("hey");
}
