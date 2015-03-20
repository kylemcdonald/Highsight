#pragma once

#include "ofMain.h"

class Motor {
public:
    int id = 0;
    float unitsPerCm = 0;
    float refPointCm = 0;
    float refPointUnits = 0;
    
    struct Status {
        string statusMessage = "";
        float encoder0Pos = 0;
        float currentSpeed = 0;
        int stepper = 0;
        int encoder = 0;
    } status;
    
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
            lengthSpeedCps = (curLength - prevLength) * ofGetTargetFrameRate();
        }
        prevLength = curLength;
    }
    ofVec3f getFloorDrop() const {
        ofVec3f floorDrop = pillarAttach;
        floorDrop.z = 0;
        return floorDrop;
    }
    void draw(ofVec3f eyePosition) {
        ofPushMatrix();
        ofPushStyle();
        ofVec3f start = eyePosition + eyeAttach;
        ofDrawLine(pillarAttach, getFloorDrop());
        ofDrawLine(pillarAttach, start);
        float curLength = (pillarAttach - start).length();
        ofTranslate(pillarAttach);
        ofDrawBitmapString(ofToString(roundf(curLength)) + "cm\n" +
                           ofToString(roundf(lengthSpeedCps)) + "cm/s\n" +
                           "status: " + status.statusMessage + "\n" +
                           ofToString(status.encoder0Pos) + " / encoder0Pos\n" +
                           ofToString(status.currentSpeed) + " / currentSpeed\n" +
                           ofToString(status.stepper) + " / stepper\n" +
                           ofToString(status.encoder) + " / encoder",
                           10, 20);
        ofPopStyle();
        ofPopMatrix();
    }
    float getLengthUnits() {
        return (refPointCm - prevLength) * unitsPerCm + refPointUnits;
    }
};