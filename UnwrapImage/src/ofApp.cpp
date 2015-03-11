#include "ofApp.h"

using namespace ofxCv;
using namespace cv;

void ofApp::setup() {
    dir.listDir("images");
    index = 0;
    
    gui.setup();
    gui.add(offset.set("Offset", ofVec2f(960, 530), ofVec2f(0, 0), ofVec2f(1920, 1080)));
    gui.add(radius.set("Radius", 530, (1080/2)-200, (1080/2)+200));
    gui.add(fov.set("Field of view", 180, 160, 200));
    gui.add(radiusResolution.set("Radius resolution", 24, 10, 32));
    gui.add(thetaResolution.set("Theta resolution", 92, 32, 128));
    
    ofSetCircleResolution(64);
}

void ofApp::update() {
    fisheye.offset = offset;
    fisheye.radius = radius;
    fisheye.fov = fov;
    fisheye.radiusResolution = radiusResolution;
    fisheye.thetaResolution = thetaResolution;
    fisheye.setup(img.getTexture());
}

void ofApp::draw() {
    ofBackground(0);
    ofPushMatrix();
    ofScale(.5, .5);
    if(img.isAllocated()) {
//        img.draw(0, 0);
    }
    ofNoFill();
    ofTranslate((ofVec2f) offset);

    ofPushStyle();
    ofSetColor(255);
//    fisheye.sampleMesh.drawWireframe();
    ofSetColor(255, 0, 0, 128);
//    ofDrawCircle(0, 0, radius);
//    ofDrawLine(-ofGetWidth(), 0, +ofGetWidth(), 0);
//    ofDrawLine(0, -ofGetHeight(), 0, +ofGetHeight());
    ofPopStyle();
    ofPopMatrix();
    
    cam.setPosition(ofVec3f());
    cam.setFov(80);
    cam.begin();
    ofEnableDepthTest();
    ofScale(100, 100, 100);
    fisheye.draw(img.getTexture());
    ofDisableDepthTest();
    cam.end();
    
    gui.draw();
}

void ofApp::keyPressed(int key) {
    img.load(dir[index]);
    img.update();
    index = (index + 1) % dir.size();
}