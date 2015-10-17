#include "ofMain.h"

#include "Fisheye.h"

#include "ofxTiming.h"
#include "ofxOculusDK2.h"
#include "ofxOsc.h"
#include "ofxSyphon.h"

class ofApp : public ofBaseApp {
public:
    ofxSyphonClient cam;
    Fisheye fisheye;
	RateTimer cameraTimer, renderTimer;
    DelayTimer screenshotTimer;
    ofCamera camera;
    ofxOculusDK2 oculusRift;
    ofxOscReceiver osc;
    float targetLookAngle = 117.5;
    float lookAngle = 0;
    bool debug = false;
	
	void setup() {
        ofBackground(0);
        ofHideCursor();
        
        cam.setup();
        cam.set("","Black Syphon");
        fisheye.setup();
        
        screenshotTimer.setPeriod(60);
        
        ofXml config;
        config.load("config.xml");
        osc.setup(config.getIntValue("osc/port"));
        
        oculusRift.baseCamera = &camera;
        oculusRift.setup();
	}
    void saveScreen(string prefix) {
        cam.save(prefix + ofGetTimestampString() + "-camera.tiff");
//        ofSaveScreen(prefix + ofGetTimestampString() + "-oculus.tiff");
    }
    int goFullscreen = 0;
    void update() {
        if( goFullscreen == 2 ){
            ofSetFullscreen(false);
            ofSetWindowPosition(1920, 0);
        }
        if( goFullscreen == 4 ){
            ofSetFullscreen(true);
            ofViewport(ofGetNativeViewport());
        }
        goFullscreen++;
        if(goFullscreen > 4 && ofGetWindowPositionX() < 1440) {
            goFullscreen = 0;
        }
        
        renderTimer.tick();
        while(osc.hasWaitingMessages()) {
            ofxOscMessage msg;
            osc.getNextMessage(&msg);
            if(msg.getAddress() == "/lookAngle/set") {
                targetLookAngle = msg.getArgAsFloat(0);
            }
            if(msg.getAddress() == "/lookAngle/add") {
                targetLookAngle += msg.getArgAsFloat(0);
                ofLog() << "targetLookAngle: " << targetLookAngle;
            }
            if(msg.getAddress() == "/screenshot") {
                saveScreen("button/");
            }
        }
        lookAngle = ofLerp(lookAngle, targetLookAngle, .1);
        if(screenshotTimer.tick()) {
//            saveScreen("automatic/"); // uncomment to enable automatic screenshot
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
        
        // this is essential for setting up the texture
        cam.bind();
        cam.unbind();
    }
    void drawScene() {
        ofPushMatrix();
        ofScale(100, -100, 100); // avoid clipping
        ofRotateX(+90);
        ofRotateZ(+lookAngle);
        cam.bind();
        fisheye.draw();
        cam.unbind();
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
        fisheye.keyPressed(key);
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
