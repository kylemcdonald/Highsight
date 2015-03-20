// todo:
// send osc to motors
// move osc output to threaded loop (not graphcis loop)
// load config via json/xml

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxOsc.h"
#include "ofxConnexion.h"
#include "ofxGui.h"

const float width = 607, depth = 608, height = 357;
//const float eyeWidth = 20, eyeDepth = 20, attachHeight = 3;
const float eyeStartHeight = 100;
//const float eyeWidth = 1.25, eyeDepth = 1.25, attachHeight = 0;
const float eyeWidth = 7.6, eyeDepth = 7.6, attachHeight = 0;
const float eyePadding = 0;
const float minMouseDistance = 20;
const float maxMouseDistance = 250;
const float lookAngleSpeedDps = 90; // degrees / second
const float frameRate = 60;

const float lookAngleDefault = 180; // startup angle
const float oculusLookAngleOffset = 180; // correct for angle between kiosk and carriage

const float safetyFloor = 80; // never go lower than this!
const float eyeHeightMin = eyePadding + safetyFloor, eyeHeightMax = height - eyePadding;
const float eyeWidthMax = (width / 2) - eyePadding, eyeDepthMax = (depth / 2) - eyePadding;

//  safe zone when controlled by visitor - a stubby cylinder
const float visitorFloor = 270; // cm
const float visitorCeiling = 350; // cm
const float visitorRadius = 230; // cm

enum LiveMode {
    LIVE_MODE_XY,
    LIVE_MODE_XZ
};

void drawLineHighlight(ofVec3f start, ofVec3f end) {
    ofPushStyle();
    ofSetLineWidth(4);
    ofSetColor(255);
    ofDrawLine(start, end);
    ofSetLineWidth(2);
    ofSetColor(0);
    ofDrawLine(start, end);
    ofPopStyle();
}

class Motor {
public:
    int id = 0;
    float unitsPerCm = 0;
    float refPointCm = 0;
    float refPointUnits = 0;
    
    ofVec3f eyeAttach, pillarAttach;
    float prevLength, lengthSpeedCps;
    Motor() : prevLength(0), lengthSpeedCps(0) {
    }
    void setup(ofXml& xml, string address = "") {
        id = xml.getIntValue(address + "id");
        unitsPerCm = xml.getFloatValue(address + "unitsPerCm");
        refPointCm = xml.getFloatValue(address + "refPoint/cm");
        refPointUnits = xml.getFloatValue(address + "refPoint/units");
    }
    void update(ofVec3f eyePosition) {
        ofVec3f start = eyePosition + eyeAttach;
        float curLength = (pillarAttach - start).length();
        if(prevLength > 0) {
            lengthSpeedCps = (curLength - prevLength) * frameRate;
        }
        prevLength = curLength;
    }
    void draw(ofVec3f eyePosition) {
        ofVec3f start = eyePosition + eyeAttach;
        drawLineHighlight(start, pillarAttach);
        ofVec3f label = start.getInterpolated(pillarAttach, .5);
        float curLength = (pillarAttach - start).length();
        ofSetColor(0);
        ofDrawBitmapString(ofToString(roundf(curLength)) + "cm @ " +
                           ofToString(roundf(lengthSpeedCps)) + "cm/s", label);
    }
    float getLengthUnits() {
        return (refPointCm - prevLength) * unitsPerCm + refPointUnits;
    }
};

class ofApp : public ofBaseApp {
public:
    ofxOscSender oscMotorsSend, oscOculusSend;
    ofxOscReceiver oscMotorsReceive;
    Motor nw, ne, sw, se;
    ofEasyCam cam;
    ofVec2f mouseStart, mouseVec;
    ofVec3f moveVecCps;
    float moveSpeedCps = 100; // cm / second
    float lookAngle = lookAngleDefault;
    ofImage shadow;
    int liveMode;
    bool live = false;
    ofParameter<bool> lockLookAngle = true;
    ofxConnexion connexion;
    ofxPanel gui;
    ofParameter<ofVec3f> eyePosition;
    ofParameter<ofVec3f> connexionPosition, connexionRotation;
    ofxButton resetLookAngleBtn, toggleFullscreenBtn;
    ofxButton visitorModeButton;
    
    ofParameter<bool> visitorMode = true;
    
    void setup() {
        ofBackground(0);
        ofDisableArbTex();
        ofSetFrameRate(frameRate);
        
        ofXml config;
        config.load("config.xml");
        
        moveSpeedCps = config.getFloatValue("motors/speed/max");
        
        nw.setup(config, "motors/nw/");
        ne.setup(config, "motors/ne/");
        se.setup(config, "motors/se/");
        sw.setup(config, "motors/sw/");
        
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
        gui.add(connexionPosition.set("Connexion Position",
                                      ofVec3f(),
                                      ofVec3f(-1, -1, -1),
                                      ofVec3f(+1, +1, +1)));
        gui.add(connexionRotation.set("Connexion Rotation",
                                      ofVec3f(),
                                      ofVec3f(-1, -1, -1),
                                      ofVec3f(+1, +1, +1)));
        gui.add(resetLookAngleBtn.setup("Reset look angle"));
        gui.add(toggleFullscreenBtn.setup("Toggle fullscreen"));
        gui.add(lockLookAngle.set("Lock look angle", false));
        resetLookAngleBtn.addListener(this, &ofApp::resetLookAngle);
        toggleFullscreenBtn.addListener(this, &ofApp::toggleFullscreen);
        gui.add(eyePosition.set("Eye position",
                                ofVec3f(0, 0, eyeStartHeight),
                                ofVec3f(-eyeWidthMax, -eyeDepthMax, eyeHeightMin),
                                ofVec3f(+eyeWidthMax, +eyeDepthMax, eyeHeightMax)));
        
        
        gui.add(visitorMode.set("Visitor mode", true));
        
        
        sendMotorPower(true);
        sendMotorsAllCommand("/resume");
        sendMotorsEachCommand("/maxspeed", moveSpeedCps);
    }
    void resetLookAngle() {
        lookAngle = 0;
    }
    void toggleFullscreen() {
        ofToggleFullscreen();
    }
    void connexionData(ConnexionData& data) {
        connexionPosition = data.getNormalizedPosition();
        connexionRotation = data.getNormalizedRotation();
    }
    void sendMotorsAllCommand(string address) {
        ofxOscMessage msg;
        msg.setAddress(address);
        oscMotorsSend.sendMessage(msg, false);
    }
    void sendMotorPower(bool power) {
        ofxOscMessage msg;
        msg.setAddress("/motor");
        msg.addIntArg(power ? 1 : 0);
        oscMotorsSend.sendMessage(msg, false);
    }
    void sendMotorsEachCommand(string address, float value) {
        for(int i = 0; i < 4; i++) {
            ofxOscMessage msg;
            msg.setAddress(address);
            msg.addIntArg(i);
            msg.addFloatArg(value);
            oscMotorsSend.sendMessage(msg, false);
        }
    }
    void exit() {
        sendMotorsAllCommand("/stop");
        sendMotorPower(false);
        connexion.stop();
    }
    void update() {
        updateConnexion();
        updateMouse();
        
        // clamp min position above floor
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
        
        updateMotors();
        updateOculus();
    }
    void updateMotors() {
        nw.update(eyePosition);
        ne.update(eyePosition);
        sw.update(eyePosition);
        se.update(eyePosition);
        
        ofxOscMessage motors;
        motors.setAddress("/go");
        float sorted[] = {0, 0, 0, 0};
        sorted[nw.id] = MAX(0, nw.getLengthUnits());
        sorted[ne.id] = MAX(0, ne.getLengthUnits());
        sorted[se.id] = MAX(0, se.getLengthUnits());
        sorted[sw.id] = MAX(0, sw.getLengthUnits());
        motors.addFloatArg(sorted[0]);
        motors.addFloatArg(sorted[1]);
        motors.addFloatArg(sorted[2]);
        motors.addFloatArg(sorted[3]);
        oscMotorsSend.sendMessage(motors, false);
    }
    void updateOculus() {
        ofxOscMessage oculus;
        oculus.setAddress("/lookAngle");
        oculus.addFloatArg(lookAngle+oculusLookAngleOffset);
        oscOculusSend.sendMessage(oculus);
    }
    void updateConnexion() {
        moveVecCps = ofVec3f(connexionPosition->x,
                             -connexionPosition->y,
                             -connexionPosition->z);
        moveVecCps.rotate(lookAngle, ofVec3f(0, 0, 1));
        moveVecCps *= moveSpeedCps;
        ofVec3f moveVecFps = moveVecCps / frameRate;
        eyePosition += moveVecFps;
        
        if(!lockLookAngle) {
            float lookAngleDps = -connexionRotation->z * lookAngleSpeedDps;
            float lookAngleFps = lookAngleDps / frameRate;
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
            ofVec3f moveVecFps = moveVecCps / frameRate;
            ofVec3f eyeUpdate = eyePosition;
            if(liveMode == LIVE_MODE_XY) {
                eyeUpdate.x += moveVecFps.x;
                eyeUpdate.y += moveVecFps.y;
            } else if(liveMode == LIVE_MODE_XZ) {
                eyeUpdate.x += moveVecFps.x;
                eyeUpdate.z += moveVecFps.y;
            }
            eyePosition = eyeUpdate;
        }
    }
    void draw() {
        cam.begin();
        ofPushMatrix();
        ofEnableDepthTest();
        
        // setup the space for viewing
        ofRotateX(-80);
        ofRotateZ(-20);
        ofScale(1.7, 1.7, 1.7);
        ofTranslate(0, 0, -height / 3);
        
        // draw the room
        
        ofDisableDepthTest();
        
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
        // draw the carriage
        ofSetColor(ofColor::black);
        ofRotate(lookAngle);
        ofDrawLine(0, 0, 0, 90);
        ofDrawTriangle(5, 90, 0, 100, -5, 90);
        ofPopMatrix();
        
        ofPushStyle();
        nw.draw(eyePosition);
        ne.draw(eyePosition);
        sw.draw(eyePosition);
        se.draw(eyePosition);
        ofPopStyle();
        
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
        ofSetColor(ofColor::black, 64);
        ofDrawCircle(0, 0, 12);
        ofSetColor(ofColor::white, 128);
        ofDrawCircle(0, 0, 6);
        ofSetColor(ofColor::black, 255);
        ofDrawCircle(0, 0, 1.5);
        ofSetColor(ofColor::white, 255);
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
            resetLookAngle();
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
