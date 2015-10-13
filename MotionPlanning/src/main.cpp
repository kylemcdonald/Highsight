#include "ofMain.h"

class ofApp : public ofBaseApp {
public:
    ofVec2f position;
    vector<ofVec2f> derivatives;
    vector<float> magnitudes;
    vector<float> limits;
    ofFbo trail;
    float smoothestStep(float t) {
        float t2 = t * t;
        return (-20*t2*t+70*t2-84*t+35)*t2*t2;
    }
    float smoothestStepVelocity(float t) {
        float t2m2 = t*t - t;
        return  -140*t2m2*t2m2*t2m2;
    }
    float smoothestStepAcceleration(float t) {
        float tm1 = t-1;
        return 420*(1-2*t)*tm1*tm1*t*t;
    }
    float smoothestStepJerk(float t) {
        return -840*(t-1)*t*(5*t*t-5*t+1);
    }
    void setup() {
        ofBackground(255);
        trail.allocate(ofGetWidth(), ofGetHeight(), GL_RGBA, 4);
        trail.begin();
        ofClear(255, 0);
        trail.end();
        derivatives.resize(3); // vel, acc, jer
        magnitudes.resize(3, 0);
        limits.resize(3, 0);
        limits[0] = pow(100, 1./1.);
        limits[1] = pow(1, 1./2.);
        limits[2] = pow(10, 1./3.);
    }
    void draw() {
        float t = fmodf((float) mouseX / ofGetWidth(), 1);
//        float t = fmodf(ofGetElapsedTimef() / 10, 1.);
        float p = smoothestStep(t);
        float v = smoothestStepVelocity(t);
        float a = smoothestStepAcceleration(t);
        float j = smoothestStepJerk(t);
        ofSetColor(0);
        ofPushMatrix();
        ofTranslate(0, ofGetHeight() / 2);
        ofDrawRectangle(0, 0, 10, 100*v);
        ofDrawRectangle(15, 0, 10, 10*a);
        ofDrawRectangle(30, 0, 10, j);
        ofPopMatrix();
        ofTranslate(p*ofGetWidth(), 0);
        ofDrawRectangle(0, 0, 2, ofGetHeight());
    }
    void draw2() {
        ofVec2f targetPosition(ofGetMouseX(), ofGetMouseY());
        if(position.x == 0 && position.y == 0) {
            position = targetPosition;
        }

        ofVec2f velocity = targetPosition - position;
        float velocityMagnitude = velocity.length();
        float accelerationMagnitude = velocityMagnitude - magnitudes[0];
        magnitudes[1] = ofClamp(accelerationMagnitude, -limits[1], +limits[1]);
        ofLog() << magnitudes[1];
        magnitudes[0] += magnitudes[1];
        magnitudes[0] = ofClamp(magnitudes[0], -limits[0], +limits[0]);
        velocity.limit(magnitudes[0]);
//        float curAccelerationMagnitude = velocityMagnitude - length;
//        curAccelerationMagnitude = ofClamp(curAccelerationMagnitude, -limits[1], +limits[1]);
        
//        ofVec2f targetAcceleration = targetVelocity - derivatives[0];
//        ofVec2f acceleration = targetAcceleration;
//        length = acceleration.length();
//        if(length > limits[1]) {
//            ofLog() << "acceleration limiting " << length;
//            acceleration.limit(limits[1]);
//        }
//        ofVec2f targetJerk = targetAcceleration - derivatives[1];
//        ofVec2f jerk = targetJerk;
//        length = jerk.length();
//        if(length > limits[2]) {
//            ofLog() << "jerk limiting " << length;
//            jerk.limit(limits[2]);
//        }
//        derivatives[2] = jerk;
//        derivatives[1] += derivatives[2];
//        derivatives[1] = acceleration;
//        derivatives[0] += derivatives[1];
        derivatives[0] = velocity;
        position += derivatives[0];
        
        trail.begin();
        ofFill();
        ofSetColor(255, 0, 0);
        ofDrawCircle(targetPosition.x, targetPosition.y, 1.5, 1.5);
        ofSetColor(0);
        ofDrawCircle(position.x, position.y, 1.5, 1.5);
        trail.end();
        
        ofPushMatrix();
        ofSetColor(255);
        trail.draw(0, 0);
        ofTranslate(position);
        ofSetColor(255, 0, 0);
//        ofDrawLine(ofVec2f(), targetVelocity);
        ofSetColor(0);
//        ofDrawLine(ofVec2f(), velocity);
        ofPopMatrix();
        
        ofTranslate(ofGetWidth() / 2, 0);
        ofSetColor(255, 0, 0);
//        ofDrawRectangle(0, 0, 10, pow(targetVelocity.length(), 1));
//        ofDrawRectangle(15, 0, 10, pow(targetAcceleration.length(), 2));
//        ofDrawRectangle(30, 0, 10, pow(targetJerk.length(), 3));
        ofSetColor(0);
//        ofDrawRectangle(0, 0, 10, pow(velocity.length(), 1));
//        ofDrawRectangle(15, 0, 10, pow(acceleration.length(), 2));
//        ofDrawRectangle(30, 0, 10, pow(jerk.length(), 3));
    }
};

int main() {
    ofSetupOpenGL(32, 32, OF_WINDOW);
    ofSetWindowShape(1280, 1280);
    ofSetWindowPosition((ofGetScreenWidth() - ofGetWidth()) / 2, (ofGetScreenHeight() - ofGetHeight()) / 2);
    ofRunApp(new ofApp());
}
