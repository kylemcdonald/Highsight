#include "ofApp.h"

void ofApp::setup() {
    useVideo = false;
    drawGui = true;
    dir.listDir("images");
    index = 0;
    
    gui.setup();
    float baseResolution = 720;
    float referenceResolution = 1080;
    float scaleResolution = baseResolution / referenceResolution;
    gui.add(offset.set("Offset", scaleResolution * ofVec2f(960, 530), ofVec2f(0, 0), scaleResolution * ofVec2f(1920, 1080)));
    gui.add(radius.set("Radius", scaleResolution * 530, scaleResolution * ((1080/2)-200), scaleResolution * ((1080/2)+200)));
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
    fisheye.setup();
    
    if(useVideo) {
        video.update(); 
    }
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
    if(useVideo) {
        video.bind();
    } else {
        img.bind();
    }
    fisheye.draw();
    if(useVideo) {
        video.unbind();
    } else {
        img.unbind();
    }
    ofDisableDepthTest();
    cam.end();
    
    if(drawGui) {
        gui.draw();
    }
}

void ofApp::dragEvent(ofDragInfo event) {
    useVideo = true;
    video.load(event.files[0]);
    video.setLoopState(OF_LOOP_NORMAL);
    video.play();
}

void ofApp::keyPressed(int key) {
    if(key == 'f') {
        ofToggleFullscreen();
    } else if(key == '\t') {
        drawGui = !drawGui;
    } else {
        img.load(dir[index]);
        img.update();
        index = (index + 1) % dir.size();
    }
}