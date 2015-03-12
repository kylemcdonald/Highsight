#include "ofMain.h"
#include "ofxBlackMagic.h"
#include "ofxTiming.h"
#include "Fisheye.h"

class ofApp : public ofBaseApp {
public:
    ofxBlackMagic cam;
    Fisheye fisheye;
	RateTimer timer;
    ofEasyCam easyCam;
	
	void setup() {
		ofSetLogLevel(OF_LOG_VERBOSE);
		cam.setup(1920, 1080, 29.97f);
        fisheye.setup();
	}
	void exit() {
		cam.close();
	}
	void update() {
		if(cam.update()) {
			timer.tick();
		}
	}
    void draw() {
        cam.drawColor();
        
        easyCam.setPosition(0, 0, 0);
        easyCam.setFov(80);
        easyCam.begin();
        ofEnableDepthTest();
        ofScale(100, 100, 100);
        fisheye.draw(cam.getColorTexture());
        easyCam.end();
        
        ofDisableDepthTest();
		ofDrawBitmapStringHighlight(ofToString((int) timer.getFramerate()), 10, 20);
	}
	void keyPressed(int key) {
		if(key == 'f') {
			ofToggleFullscreen();
		}
        if(key == ' ') {
            string path = ofToString(ofGetFrameNum(), 8) + ".tiff";
            ofSaveImage(cam.getColorPixels(), path);
        }
	}
};

int main() {
	ofSetupOpenGL(1920, 1080, OF_WINDOW);
    ofSetWindowShape(1920, 1080);
	ofRunApp(new ofApp());
}
