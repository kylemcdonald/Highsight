// todo:
// send osc to motors
// move osc output to threaded loop (not graphcis loop)
// load config via json/xml

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxOsc.h"
#include "ofxConnexion.h"
#include "ofxGui.h"

float width = 634, depth = 664, height = 420;
float eyeWidth = 20, eyeDepth = 20, attachHeight = 3;
float eyeHeightMin = 30, eyeHeightMax = 380;
float eyeWidthMax = 280, eyeDepthMax = 280;
float minMouseDistance = 20;
float maxMouseDistance = 250;
float moveSpeedCps = 100; // cm / second
float lookAngleSpeedDps = 90; // degrees / second
float frameRate = 60;

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

class Cable {
public:
    ofVec3f eyeAttach, pillarAttach;
    float prevLength, lengthSpeedCps;
    Cable() : prevLength(0), lengthSpeedCps(0) {
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
};

class ofApp : public ofBaseApp {
public:
    ofxOscSender oscMotorsSend, oscOculusSend;
    ofxOscReceiver oscMotorsReceive;
    Cable nw, ne, sw, se;
    ofEasyCam cam;
    ofImage roomTexture, eyeTexture;
    ofMesh roomMesh, eyeMesh;
    ofxAssimpModelLoader roomModel, eyeModel;
    ofVec3f eyePosition;
    ofVec2f mouseStart, mouseVec;
    ofVec3f moveVecCps;
    float lookAngle = 0;
    ofImage shadow;
    int liveMode;
    bool live = false;
    ofxConnexion connexion;
    ofxPanel gui;
    ofParameter<ofVec3f> connexionPosition, connexionRotation;
    ofxButton resetLookAngleBtn, toggleFullscreenBtn;
    void setup() {
        ofBackground(0);
        ofDisableArbTex();
        ofSetFrameRate(frameRate);
        
        ofXml config;
        config.load("config.xml");
        
        moveSpeedCps = config.getFloatValue("motors/speed/max");
        
        oscOculusSend.setup("localhost", config.getIntValue("oculus/osc/sendPort"));
        oscMotorsSend.setup(config.getValue("motors/osc/host"), config.getIntValue("motors/osc/sendPort"));
        oscMotorsReceive.setup(config.getIntValue("motors/osc/sendPort"));
        
        roomModel.loadModel("room-amb.dae");
        roomTexture.load("room-amb.jpg");
        roomMesh = roomModel.getMesh(0);
        
        eyeModel.loadModel("eye-amb.dae");
        eyeTexture.load("eye-amb.jpg");
        eyeMesh = eyeModel.getMesh(0);
        eyePosition.set(0, 0, eyeHeightMin);
        
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
        resetLookAngleBtn.addListener(this, &ofApp::resetLookAngle);
        toggleFullscreenBtn.addListener(this, &ofApp::toggleFullscreen);
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
    void exit() {
        connexion.stop();
    }
    void update() {
        updateConnexion();
        updateMouse();
        
        // clamp min position above floor
        eyePosition.x = ofClamp(eyePosition.x, -eyeWidthMax, +eyeWidthMax);
        eyePosition.y = ofClamp(eyePosition.y, -eyeDepthMax, +eyeDepthMax);
        eyePosition.z = ofClamp(eyePosition.z, eyeHeightMin, eyeHeightMax);
        
        nw.update(eyePosition);
        ne.update(eyePosition);
        sw.update(eyePosition);
        se.update(eyePosition);
        
        ofxOscMessage motors;
        motors.setAddress("/motors");
        motors.addFloatArg(nw.prevLength);
        motors.addFloatArg(nw.lengthSpeedCps);
        motors.addFloatArg(ne.prevLength);
        motors.addFloatArg(ne.lengthSpeedCps);
        motors.addFloatArg(sw.prevLength);
        motors.addFloatArg(sw.lengthSpeedCps);
        motors.addFloatArg(se.prevLength);
        motors.addFloatArg(se.lengthSpeedCps);
        oscMotorsSend.sendMessage(motors);
        
        ofxOscMessage oculus;
        oculus.setAddress("/lookAngle");
        oculus.addFloatArg(lookAngle);
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
        
        float lookAngleDps = -connexionRotation->z * lookAngleSpeedDps;
        float lookAngleFps = lookAngleDps / frameRate;
        lookAngle += lookAngleFps;
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
            if(liveMode == LIVE_MODE_XY) {
                eyePosition.x += moveVecFps.x;
                eyePosition.y += moveVecFps.y;
            } else if(liveMode == LIVE_MODE_XZ) {
                eyePosition.x += moveVecFps.x;
                eyePosition.z += moveVecFps.y;
            }
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
        ofSetColor(255);
        roomTexture.bind();
        roomMesh.drawFaces();
        roomTexture.unbind();
        
        ofDisableDepthTest();
        
        ofPushMatrix();
        ofPushStyle();
        ofTranslate(eyePosition.x, eyePosition.y, 0);
        shadow.setAnchorPercent(.5, .5);
        float shadowSize = ofMap(eyePosition.z, 0, height, 80, 300);
        float shadowAlpha = ofMap(eyePosition.z, 0, height, 64, 20);
        ofSetColor(255, shadowAlpha);
        shadow.draw(0, 0, shadowSize, shadowSize);
        ofPopStyle();
        ofPopMatrix();
        
        ofPushMatrix();
        ofTranslate(eyePosition);
        eyeTexture.bind();
        ofEnableDepthTest();
        eyeMesh.drawFaces();
        ofDisableDepthTest();
        eyeTexture.unbind();
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