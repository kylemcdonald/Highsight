// todo:
// clean up variables / config
// add homing function
// quadlaterate on startupalso,
// load all config via json/xml
// add screenshot trigger
// move osc output to threaded loop (not graphics loop)

#include "ofMain.h"

#include "ofxAssimpModelLoader.h"
#include "ofxOsc.h"
#include "ofxConnexion.h"
#include "ofxGui.h"

#include "Motor.h"

const float resetWaitTime = 2000;

const float width = 607, depth = 608, height = 357;
//const float eyeWidth = 20, eyeDepth = 20, attachHeight = 3;
const float eyeStartHeight = 100;
//const float eyeWidth = 1.25, eyeDepth = 1.25, attachHeight = 0;
const float eyeWidth = 7.6, eyeDepth = 7.6, attachHeight = 0;
const float minMouseDistance = 20;
const float maxMouseDistance = 250;
const float lookAngleSpeedDps = 90; // degrees / second

const float eyePadding = 40;
const float eyeHeightMin = 80, eyeHeightMax = height - eyePadding;
const float eyeWidthMax = (width / 2) - eyePadding, eyeDepthMax = (depth / 2) - eyePadding;

//  safe zone when controlled by visitor - a stubby cylinder
const float visitorFloor = 270; // cm
const float visitorCeiling = 350; // cm
const float visitorRadius = 230; // cm

enum LiveMode {
    LIVE_MODE_XY,
    LIVE_MODE_XZ
};

class ofApp : public ofBaseApp {
public:
    ofxOscSender oscMotorsSend, oscOculusSend;
    ofxOscReceiver oscMotorsReceive;
    Motor nw, ne, sw, se;
    vector<Motor*> motorsSorted;
    ofEasyCam cam;
    ofVec2f mouseStart, mouseVec;
    ofVec3f moveVecCps;
    float lookAngle, lookAngleDefault;
    float homeSpeedCps = 10, maxSpeedCps = 50; // cm / second
    unsigned long lastResetTime = 0;
    ofImage shadow;
    int liveMode;
    bool live = false;
    
    ofxConnexion connexion;
    
    ofxPanel gui;
    ofParameter<bool> everythingOk, lockLookAngle, visitorMode, motorsStart, motorsPower;
    ofParameter<float> lookAngleOffset, moveSpeedCps;
    ofParameter<ofVec3f> eyePosition, connexionPosition, connexionRotation;
    ofxButton resetBtn, resetLookAngleBtn, toggleFullscreenBtn, visitorModeBtn;
    
    void setup() {
        ofSetFrameRate(60);
        
        ofXml config;
        if(!config.load("config.xml")) {
            ofExit();
        }
        
        maxSpeedCps = config.getFloatValue("motors/speed/max");
        
        nw.setup(config, "motors/nw/");
        ne.setup(config, "motors/ne/");
        se.setup(config, "motors/se/");
        sw.setup(config, "motors/sw/");
        motorsSorted.resize(4);
        motorsSorted[nw.id] = &nw;
        motorsSorted[ne.id] = &ne;
        motorsSorted[se.id] = &se;
        motorsSorted[sw.id] = &sw;
        
        lookAngleDefault = config.getFloatValue("oculus/lookAngle/default");
        lookAngleOffset = config.getFloatValue("oculus/lookAngle/offset");
        
        oscOculusSend.setup("localhost", config.getIntValue("oculus/osc/sendPort"));
        oscMotorsSend.setup(config.getValue("motors/osc/host"), config.getIntValue("motors/osc/sendPort"));
        oscMotorsReceive.setup(config.getIntValue("motors/osc/receivePort"));
        
        mouseStart.set(0, 0);
        shadow.load("shadow.png");
        cam.setFov(50);
        
        nw.eyeAttach.set(-eyeWidth / 2, +eyeDepth / 2, attachHeight);
        ne.eyeAttach.set(+eyeWidth / 2, +eyeDepth / 2, attachHeight);
        sw.eyeAttach.set(-eyeWidth / 2, -eyeDepth / 2, attachHeight);
        se.eyeAttach.set(+eyeWidth / 2, -eyeDepth / 2, attachHeight);
        
        nw.pillarAttach.set(-width / 2, +depth / 2, height);
        ne.pillarAttach.set(+width / 2, +depth / 2, height);
        sw.pillarAttach.set(-width / 2, -depth / 2, height);
        se.pillarAttach.set(+width / 2, -depth / 2, height);
        
        connexion.start();
        ofAddListener(connexion.connexionEvent, this, &ofApp::connexionData);
        
        gui.setup();
        gui.add(everythingOk.set("Everything OK", true));
        gui.add(visitorMode.set("Visitor mode", true));
        gui.add(toggleFullscreenBtn.setup("Toggle fullscreen"));
        toggleFullscreenBtn.addListener(this, &ofApp::toggleFullscreen);
        gui.add(resetBtn.setup("Reset everything"));
        resetBtn.addListener(this, &ofApp::reset);
        gui.add(motorsStart.set("Motors start", false));
        motorsStart.addListener(this, &ofApp::setMotorsStart);
        gui.add(motorsPower.set("Motors power", false));
        motorsPower.addListener(this, &ofApp::setMotorsPower);
        gui.add(resetLookAngleBtn.setup("Reset look angle"));
        resetLookAngleBtn.addListener(this, &ofApp::resetLookAngle);
        gui.add(lockLookAngle.set("Lock look angle", false));
        gui.add(lookAngleOffset.set("Look angle offset", lookAngleOffset, -180, +180));
        gui.add(moveSpeedCps.set("Move speed", 0, 0, maxSpeedCps));
        moveSpeedCps.addListener(this, &ofApp::moveSpeedChange);
        gui.add(connexionPosition.set("Connexion Position",
                                      ofVec3f(),
                                      ofVec3f(-1, -1, -1),
                                      ofVec3f(+1, +1, +1)));
        gui.add(connexionRotation.set("Connexion Rotation",
                                      ofVec3f(),
                                      ofVec3f(-1, -1, -1),
                                      ofVec3f(+1, +1, +1)));
        gui.add(eyePosition.set("Eye position",
                                ofVec3f(0, 0, eyeStartHeight),
                                ofVec3f(-eyeWidthMax, -eyeDepthMax, eyeHeightMin),
                                ofVec3f(+eyeWidthMax, +eyeDepthMax, eyeHeightMax)));

        motorsPower = true;
        motorsStart = true;
        reset();
    }
    void reset() {
        lastResetTime = ofGetElapsedTimeMillis();
        moveSpeedCps = homeSpeedCps;
        eyePosition = ofVec3f(0, 0, eyeStartHeight);
        resetLookAngle();
    }
    void resetLookAngle() {
        lookAngle = lookAngleDefault;
    }
    void toggleFullscreen() {
        ofToggleFullscreen();
    }
    void connexionData(ConnexionData& data) {
        if(data.getButton(0) || data.getButton(1)) {
            ofxOscMessage msg;
            msg.setAddress("/save");
            oscOculusSend.sendMessage(msg);
        }
        // raw right push+tilt: +x pos, -y rot
        // raw forward push+tilt: -y pos, -x rot
        ofVec3f npos = data.getNormalizedPosition();
        ofVec3f nrot = data.getNormalizedRotation();
        // processed right push+tilt: +x pos, +y rot
        // processed forward push+tilt: +y pos, +x rot
        connexionPosition = ofVec3f(+npos.x, -npos.y, -npos.z);
        connexionRotation = ofVec3f(-nrot.x, -nrot.y, -nrot.z);
    }
    void sendMotorsAllCommand(string address) {
        ofLog() << address;
        ofxOscMessage msg;
        msg.setAddress(address);
        oscMotorsSend.sendMessage(msg, false);
    }
    void setMotorsStart(bool& start) {
        sendMotorsAllCommand(start ? "/resume" : "/stop");
    }
    void setMotorsPower(bool& power) {
        int powerInt = power ? 1 : 0;
        ofLog() << "/motor " << powerInt;
        ofxOscMessage msg;
        msg.setAddress("/motor");
        msg.addIntArg(powerInt);
        oscMotorsSend.sendMessage(msg, false);
    }
    void sendMotorsEachCommand(string address, float value) {
        ofLog() << address << " " << value;
        for(int i = 0; i < 4; i++) {
            ofxOscMessage msg;
            msg.setAddress(address);
            msg.addIntArg(i);
            msg.addFloatArg(value);
            oscMotorsSend.sendMessage(msg, false);
        }
    }
    void moveSpeedChange(float& value) {
        sendMotorsEachCommand("/maxspeed", moveSpeedCps);
    }
    void exit() {
        motorsStart = false;
        motorsPower = false;
        connexion.stop();
    }
    void update() {
        updateStatus();
        updateConnexion();
        updateMouse();
        updateEye();
        if(everythingOk) {
            updateMotors();
        }
        updateOculus();
    }
    void updateStatus() {
        while(oscMotorsReceive.hasWaitingMessages()) {
            ofxOscMessage msg;
            oscMotorsReceive.getNextMessage(&msg);
            if(msg.getAddress() == "/status") {
                int motorId = msg.getArgAsInt32(0);
                Motor::Status& status = motorsSorted[motorId]->status;
                status.statusMessage = msg.getArgAsString(1);
                status.encoder0Pos = msg.getArgAsFloat(2);
                status.currentSpeed = msg.getArgAsFloat(3);
            }
        }
        
        everythingOk = true;
        for(int i = 0; i < 4; i++) {
            string& msg = motorsSorted[i]->status.statusMessage;
            // whitelist of non-problematic states
            if(!(msg == "OK" ||
                 msg == "HOMING" ||
                 msg == "HOMINGBACKOFF")) {
                everythingOk = false;
            }
        }
    }
    void updateConnexion() {
        moveVecCps = ofVec3f(connexionRotation->y,
                             connexionRotation->x,
                             connexionPosition->z);
        moveVecCps *= moveSpeedCps;
        
        if(!lockLookAngle) {
            float lookAngleDps = connexionRotation->z * lookAngleSpeedDps;
            float lookAngleFps = lookAngleDps / ofGetTargetFrameRate();
            lookAngle += lookAngleFps;
        }
    }
    void updateMouse() {
        ofVec2f mouseCur(mouseX, mouseY);
        mouseVec = mouseCur - mouseStart;
        if(mouseVec.length() > maxMouseDistance) {
            mouseVec *= maxMouseDistance / mouseVec.length();
        }
        if(live && mouseVec.length() > minMouseDistance) {
            float mouseLength = mouseVec.length();
            moveVecCps = mouseVec / mouseLength;
            moveVecCps.y *= -1;
            moveVecCps *= ofNormalize(mouseLength, minMouseDistance, maxMouseDistance);
            moveVecCps *= moveSpeedCps;
            ofVec3f eyeUpdate = eyePosition;
            if(liveMode == LIVE_MODE_XZ) { // swap z and y
                moveVecCps.z = moveVecCps.y;
                moveVecCps.y = 0;
            }
        }
    }
    void updateEye() {
        ofVec3f moveVecFps = moveVecCps / ofGetTargetFrameRate();
        moveVecFps.rotate(lookAngle, ofVec3f(0, 0, 1));
        eyePosition += moveVecFps;
        
        // hard limits on eye position
        eyePosition = ofVec3f(ofClamp(eyePosition->x, -eyeWidthMax, +eyeWidthMax),
                              ofClamp(eyePosition->y, -eyeDepthMax, +eyeDepthMax),
                              ofClamp(eyePosition->z, eyeHeightMin, eyeHeightMax));
        
        if (visitorMode) {
            // enforce audience-control flight zone
            eyePosition = ofVec3f(ofClamp(eyePosition->x, -visitorRadius, +visitorRadius),
                                  ofClamp(eyePosition->y, -visitorRadius, +visitorRadius),
                                  ofClamp(eyePosition->z, visitorFloor, visitorCeiling));
            float radial = sqrt(eyePosition->x * eyePosition->x + eyePosition->y * eyePosition->y);
            if (radial > visitorRadius) {
                eyePosition = ofVec3f(eyePosition->x * visitorRadius / radial,
                                      eyePosition->y * visitorRadius / radial,
                                      eyePosition->z);
            }
        }
    }
    void updateMotors() {
        nw.update(eyePosition);
        ne.update(eyePosition);
        sw.update(eyePosition);
        se.update(eyePosition);
        
        unsigned long curTime = ofGetElapsedTimeMillis();
        unsigned long curDuration = curTime - lastResetTime;
        if(curDuration > resetWaitTime && moveSpeedCps < maxSpeedCps) {
            bool ready = true;
            for(int i = 0; i < 4; i++) {
                if(motorsSorted[i]->status.currentSpeed != 0) {
                    ready = false;
                }
            }
            if(ready) {
                moveSpeedCps = maxSpeedCps;
            }
        }
        
        ofxOscMessage motors;
        motors.setAddress("/go");
        for(int i = 0; i < 4; i++) {
            motors.addFloatArg(MAX(0, motorsSorted[i]->getLengthUnits()));
        }
        oscMotorsSend.sendMessage(motors, false);
    }
    void updateOculus() {
        ofxOscMessage oculus;
        oculus.setAddress("/lookAngle");
        oculus.addFloatArg(lookAngle+lookAngleOffset);
        oscOculusSend.sendMessage(oculus);
    }
    void draw() {
        if(everythingOk) {
            ofBackground(128);
        } else {
            ofBackground(128, 0, 0);
        }
        
        cam.begin();
        ofPushMatrix();
        
        // setup the space for viewing
        ofRotateX(-70);
        ofRotateZ(-15);
        ofScale(1.7, 1.7, 1.7);
        ofTranslate(0, 0, -height / 3);
        
        ofPushMatrix();
        ofPushStyle();
        ofTranslate(eyePosition->x, eyePosition->y, 0);
        shadow.setAnchorPercent(.5, .5);
        float shadowSize = ofMap(eyePosition->z, 0, height, 80, 300);
        float shadowAlpha = ofMap(eyePosition->z, 0, height, 64, 20);
        ofSetColor(255, shadowAlpha);
        shadow.draw(0, 0, shadowSize, shadowSize);
        ofPopStyle();
        ofPopMatrix();
        
        ofPushMatrix();
        ofTranslate(eyePosition);
        ofDrawBox(0, 0, 0, eyeWidth, eyeDepth, attachHeight);
        ofSetColor(ofColor::white);
        ofRotate(lookAngle);
        // draw arrow twice: once at eye, once on floor
        for(int i = 0; i < 2; i++) {
            ofPushMatrix();
            ofDrawLine(0, 0, 0, 90);
            ofRotateY(45);
            ofDrawTriangle(5, 90, 0, 100, -5, 90);
            ofRotateY(90);
            ofDrawTriangle(5, 90, 0, 100, -5, 90);
            ofPopMatrix();
            ofTranslate(0, 0, -eyePosition->z);
        }
        ofPopMatrix();
        
        ofPushStyle();
        nw.draw(eyePosition);
        ne.draw(eyePosition);
        sw.draw(eyePosition);
        se.draw(eyePosition);
        ofPolyline floor;
        floor.close();
        floor.addVertex(nw.getFloorDrop());
        floor.addVertex(ne.getFloorDrop());
        floor.addVertex(se.getFloorDrop());
        floor.addVertex(sw.getFloorDrop());
        floor.draw();
        ofPopStyle();
        
        ofPushMatrix();
        ofPushStyle();
        ofNoFill();
        ofSetColor(255, 32);
        int n = 12;
        for(int i = 0; i < n; i++) {
            if(visitorMode) {
                float z = ofMap(i, 0, n - 1, visitorFloor, visitorCeiling);
                ofDrawCircle(0, 0, z, visitorRadius);
            } else {
                float z = ofMap(i, 0, n - 1, eyeHeightMin, eyeHeightMax);
                ofDrawRectangle(-eyeWidthMax, -eyeDepthMax, z, eyeWidthMax*2, eyeDepthMax*2);
            }
        }
        ofPopStyle();
        ofPopMatrix();
        
        ofPopMatrix();
        cam.end();
        
        if(live) {
            ofPushMatrix();
            ofPushStyle();
            ofSetCircleResolution(64);
            ofVec2f mouseCur(mouseX, mouseY);
            ofFill();
            ofTranslate(mouseStart);
            ofSetColor(0, 128);
            ofDrawCircle(0, 0, maxMouseDistance); // background
            ofSetColor(255, 128);
            ofSetLineWidth(4);
            ofDrawLine(0, 0, mouseVec.x, mouseVec.y); // line
            ofSetLineWidth(2);
            ofDrawLine(-maxMouseDistance, 0, +maxMouseDistance, 0); // x
            ofDrawLine(0, -maxMouseDistance, 0, +maxMouseDistance); // y
            ofSetColor(255);
            ofSetLineWidth(4);
            ofDrawCircle(0, 0, minMouseDistance); // inner circle
            ofNoFill();
            ofDrawCircle(0, 0, maxMouseDistance); // outer circle
            ofDrawBitmapStringHighlight(ofToString(roundf(moveVecCps.length())) + "cm/s", mouseVec);
            string axis0, axis1;
            if(liveMode == LIVE_MODE_XY) {
                axis0 = "X", axis1 = "Y";
            } else if(liveMode == LIVE_MODE_XZ) {
                axis0 = "X", axis1 = "Z";
            }
            float padding = 20;
            ofDrawBitmapStringHighlight(axis0 + ":" + ofToString(roundf(moveVecCps.x)) + "cm/s", minMouseDistance + padding, 0);
            ofDrawBitmapStringHighlight(axis1 + ":" + ofToString(roundf(moveVecCps.y)) + "cm/s", 0, -(minMouseDistance + padding));
            ofPopStyle();
            ofPopMatrix();
        }
        
        gui.draw();
        
        drawCursor();
    }
    void drawCursor() {
        // custom cursor
        ofPushMatrix();
        ofHideCursor();
        ofPushStyle();
        ofTranslate(ofGetMouseX()+.5, ofGetMouseY()+.5);
        ofSetColor(ofColor::white, 64);
        ofDrawCircle(0, 0, 12);
        ofSetColor(ofColor::black, 128);
        ofDrawCircle(0, 0, 6);
        ofSetColor(ofColor::white, 255);
        ofDrawCircle(0, 0, 1.5);
        ofSetColor(ofColor::black, 255);
        ofDrawCircle(0, 0, .5);
        ofPopMatrix();
        ofPopStyle();
    }
    void keyPressed(int key) {
        if(key == ' ' && !live) {
            mouseStart.set(mouseX, mouseY);
            live = true;
            liveMode = LIVE_MODE_XY;
        }
        if(key == '\t' && !live) {
            mouseStart.set(mouseX, mouseY);
            live = true;
            liveMode = LIVE_MODE_XZ;
        }
        if(key == 'r') {
            reset();
        }
        if(key == 'f') {
            toggleFullscreen();
        }
    }
    void keyReleased(int key) {
        if(key == ' ' || key == '\t') {
            live = false;
        }
    }
};

int main() {
    ofSetupOpenGL(1280, 720, OF_WINDOW);
    ofSetWindowShape(1280*2, 720*2);
    ofSetWindowPosition((ofGetScreenWidth() - ofGetWindowWidth()) / 2,
                        (ofGetScreenHeight() - ofGetWindowHeight()) / 2);
    ofRunApp(new ofApp());
}
