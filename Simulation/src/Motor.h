#pragma once

#include "ofMain.h"

class Motor {
public:
    static float statusTimeoutSeconds;
    
    string name;
    int id = 0;
    float unitsPerCm = 0;
    float refPointCm = 0;
    float refPointUnits = 0;
    
    struct Status {
        string statusMessage = "OK";
        float encoder0Pos = 0;
        float currentSpeed = 0;
        bool reboot = false;
    } status;
    float lastMessageTime = 0;
    
    ofVec3f eyeAttach, pillarAttach;
    float prevLength, lengthSpeedCps;
    Motor() : prevLength(0), lengthSpeedCps(0) {
    }
    void setup(string name, ofXml& xml, string address = "") {
        this->name = name;
        id = xml.getIntValue(address + "id");
        unitsPerCm = xml.getFloatValue(address + "unitsPerCm");
        refPointCm = xml.getFloatValue(address + "refPoint/cm");
        refPointUnits = xml.getFloatValue(address + "refPoint/units");
    }
    void update(ofVec3f eyePosition) {
        ofVec3f start = eyePosition + eyeAttach;
        float curLength = (pillarAttach - start).length();
        if(prevLength > 0) {
            lengthSpeedCps = (curLength - prevLength) * ofGetTargetFrameRate();
        }
        prevLength = curLength;
    }
    ofVec3f getFloorDrop() const {
        ofVec3f floorDrop = pillarAttach;
        floorDrop.z = 0;
        return floorDrop;
    }
    float getTimeoutDuration() const {
        return ofGetElapsedTimef() - lastMessageTime;
    }
    bool getTimeout() const {
        return lastMessageTime != 0 && getTimeoutDuration() > statusTimeoutSeconds;
    }
    void draw(ofVec3f eyePosition) {
        ofPushMatrix();
        ofPushStyle();
        ofVec3f start = eyePosition + eyeAttach;
        ofDrawLine(pillarAttach, getFloorDrop());
        ofDrawLine(pillarAttach, start);
        float targetLengthCm = (pillarAttach - start).length();
        float targetLengthUnits = cmToUnits(targetLengthCm);
        float curPositionUnits = status.encoder0Pos;
        float curPositionCm = unitsToCm(curPositionUnits);
        string currentStatus = status.statusMessage;
        if(getTimeout()) {
            currentStatus = "TIMEOUT (" + ofToString((int) getTimeoutDuration()) + "s)";
        }
        if(reboot) {
            currentStatus += " (rebooted)";
        }
        ofTranslate(pillarAttach);
        ofDrawBitmapString(name + " (" + ofToString(id) + ")\n"+
                           "target\n  position: ~" + ofToString(roundf(targetLengthCm)) + "cm / ~" + ofToString(roundf(targetLengthUnits)) + " units\n" +
                           "  speed: ~" + ofToString(roundf(lengthSpeedCps)) + "cm/s\n" +
                           "current\n  status: " + currentStatus + "\n" +
                           "  position: ~" + ofToString(roundf(curPositionCm)) + " cm / ~" + ofToString(roundf(curPositionUnits)) + " units\n" +
                           "  speed: " + ofToString(status.currentSpeed) + " cm/s",
                           10, 20);
        ofPopStyle();
        ofPopMatrix();
    }
    float unitsToCm(float units) {
        return (refPointUnits - units) / unitsPerCm + refPointCm;
    }
    float cmToUnits(float cm) {
        return (refPointCm - cm) * unitsPerCm + refPointUnits;
    }
    float getLengthUnits() {
        return cmToUnits(prevLength);
    }
};

float Motor::statusTimeoutSeconds = 0;