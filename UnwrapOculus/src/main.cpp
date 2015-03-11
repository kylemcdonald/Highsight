#include "ofMain.h"

#include "Fisheye.h"

#include "ofxBlackMagic.h"
#include "ofxTiming.h"
#include "ofxOculusDK2.h"
#include "ofxOsc.h"

class ofApp : public ofBaseApp {
public:
    ofxBlackMagic cam;
    Fisheye fisheye;
	RateTimer cameraTimer, renderTimer;
    ofCamera camera;
    ofxOculusDK2 oculusRift;
    ofxOscReceiver osc;
    float lookAngle = 0;
    bool debug = false;
	
	void setup() {
        ofBackground(0);
        ofHideCursor();
        
        ofSetWindowPosition(1920, 0);
        ofToggleFullscreen();
        ofViewport(ofGetNativeViewport());
        
		cam.setup(1920, 1080, 29.97f);
        fisheye.setup(cam.getColorTexture());
        
        ofXml config;
        config.load("config.xml");
        osc.setup(config.getIntValue("osc/port"));
        
        oculusRift.baseCamera = &camera;
        oculusRift.setup();
	}
	void exit() {
		cam.close();
	}
    void update() {
        if(ofGetKeyPressed('-') && cam.update()) {
            cameraTimer.tick();
        }
        renderTimer.tick();
        while(osc.hasWaitingMessages()) {
            ofxOscMessage msg;
            osc.getNextMessage(&msg);
            if(msg.getAddress() == "/lookAngle") {
                lookAngle = msg.getArgAsFloat(0);
            }
        }
	}
    void draw() {
        ofEnableDepthTest();
        
        oculusRift.beginLeftEye();
        drawScene();
        oculusRift.endLeftEye();
        
        oculusRift.beginRightEye();
        drawScene();
        oculusRift.endRightEye();
        
        oculusRift.draw();
    }
    void drawScene() {
        ofPushMatrix();
        ofScale(100, 100, 100); // avoid clipping
        ofRotateX(-90);
        ofRotateY(lookAngle);
        fisheye.draw(cam.getColorTexture());
        ofPopMatrix();
        
        if(debug) {
            ofDisableDepthTest();
            ofTranslate(-50, 0, -100);
            ofScale(1, -1, 1);
            ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL);
            ofDrawBitmapStringHighlight("Camera: " + ofToString((int) cameraTimer.getFramerate()), 0, 0);
            ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL);
            ofDrawBitmapStringHighlight("Render: " + ofToString((int) renderTimer.getFramerate()), 0, 40);
        }
	}
	void keyPressed(int key) {
        if(key == 'd') {
            debug = !debug;
        }
		if(key == 'f') {
			ofToggleFullscreen();
		}
        if(key == ' ') {
            string path = ofGetTimestampString() + ".jpg";
            ofSaveImage(cam.getColorPixels(), path);
        }
	}
};

int main() {
    ofGLWindowSettings settings;
    settings.width = 1280;
    settings.height = 800;
    settings.setGLVersion(4, 1);
    ofCreateWindow(settings);
	ofRunApp(new ofApp());
}
