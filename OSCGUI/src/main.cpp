/*
 todo:
 - account for and limit acceleration profile
 */

#include "ofMain.h"

#include "ofxOsc.h"
#include "ofxGui.h"

float defaultLength = 300;
float minLength = 100, maxLength = 600;
float maxSpeed = +100;
string host = "localhost";
int sendPort = 12001, receivePort = 12000;

template <class T>
void clamp(ofParameter<T>& x) {
    x = ofClamp(x, x.getMin(), x.getMax());
}

class SyncPanel {
public:
    ofxPanel gui;
    ofParameter<float> nwLength, neLength, seLength, swLength;
    ofParameter<float> nwSpeed, neSpeed, seSpeed, swSpeed;
    void setup(string name) {
        gui.setup(name);
        gui.add(nwLength.set("NW Length", defaultLength, minLength, maxLength));
        gui.add(nwSpeed.set("NW Speed", 0, -maxSpeed, maxSpeed));
        gui.add(neLength.set("NE Length", defaultLength, minLength, maxLength));
        gui.add(neSpeed.set("NE Speed", 0, -maxSpeed, maxSpeed));
        gui.add(seLength.set("SE Length", defaultLength, minLength, maxLength));
        gui.add(seSpeed.set("SE Speed", 0, -maxSpeed, maxSpeed));
        gui.add(swLength.set("SW Length", defaultLength, minLength, maxLength));
        gui.add(swSpeed.set("SW Speed", 0, -maxSpeed, maxSpeed));
    }
};

class ofApp : public ofBaseApp {
public:
    ofxOscSender oscSend;
    ofxOscReceiver oscReceive;
    ofXml config;
    
    SyncPanel local, remote;
    ofxPanel zeros;
    ofxButton nwZero, neZero, seZero, swZero;
    
    void setup() {
        ofBackground(255);
        ofSetFrameRate(60);
        
        config.load("config.xml");
        defaultLength = config.getFloatValue("cable/length/default");
        minLength = config.getFloatValue("cable/length/min");
        maxLength = config.getFloatValue("cable/length/max");
        host = config.getValue("osc/host");
        sendPort = config.getIntValue("osc/sendPort");
        receivePort = config.getIntValue("osc/receive");
        
        oscSend.setup(host, sendPort);
        oscReceive.setup(receivePort);
        
        local.setup("Local");
        remote.setup("Remote");
        
        zeros.setup("Zeros");
        zeros.add(nwZero.setup("NW Zero"));
        zeros.add(neZero.setup("NE Zero"));
        zeros.add(seZero.setup("SE Zero"));
        zeros.add(swZero.setup("SW Zero"));

        nwZero.addListener(this, &ofApp::zeroNW);
        neZero.addListener(this, &ofApp::zeroNE);
        seZero.addListener(this, &ofApp::zeroSE);
        swZero.addListener(this, &ofApp::zeroSW);
        
        local.gui.loadFromFile("settings.xml");
        
        local.gui.setPosition(10, 10);
        zeros.setPosition(10, 200);
        remote.gui.setPosition(10, 310);
        
    }
    void receiveOsc() {
        while(oscReceive.hasWaitingMessages()) {
            ofxOscMessage msg;
            oscReceive.getNextMessage(&msg);
            for(int i = 0; i < msg.getNumArgs(); i++) {
                float x = msg.getArgAsFloat(i);
                switch(i) {
                    case 0: remote.nwLength = x; break;
                    case 1: remote.nwSpeed = x; break;
                    case 2: remote.neLength = x; break;
                    case 3: remote.neSpeed = x; break;
                    case 4: remote.seLength = x; break;
                    case 5: remote.seSpeed = x; break;
                    case 6: remote.swLength = x; break;
                    case 7: remote.swSpeed = x; break;
                }
            }
        }
    }
    void sendOsc() {
        ofxOscMessage msg;
        msg.setAddress("/motors");
        msg.addFloatArg(local.nwLength);
        msg.addFloatArg(local.nwSpeed);
        msg.addFloatArg(local.neLength);
        msg.addFloatArg(local.neSpeed);
        msg.addFloatArg(local.seLength);
        msg.addFloatArg(local.seSpeed);
        msg.addFloatArg(local.swLength);
        msg.addFloatArg(local.swSpeed);
        oscSend.sendMessage(msg);
    }
    void updateOsc() {
        receiveOsc();
        sendOsc();
    }
    void zeroNW() { local.nwSpeed = 0; }
    void zeroNE() { local.neSpeed = 0; }
    void zeroSE() { local.seSpeed = 0; }
    void zeroSW() { local.swSpeed = 0; }
    void update() {
        float cpsToCpf = 1. / 60.;
        local.nwLength += local.nwSpeed * cpsToCpf;
        local.neLength += local.neSpeed * cpsToCpf;
        local.seLength += local.seSpeed * cpsToCpf;
        local.swLength += local.swSpeed * cpsToCpf;
        clamp(local.nwLength);
        clamp(local.neLength);
        clamp(local.seLength);
        clamp(local.swLength);
        updateOsc();
        ofSetWindowTitle(ofToString(roundf(ofGetFrameRate())));
    }
    void draw() {
        local.gui.draw();
        remote.gui.draw();
        zeros.draw();
    }
};
int main() {
    ofSetupOpenGL(225, 500, OF_WINDOW);
    ofRunApp(new ofApp());
}