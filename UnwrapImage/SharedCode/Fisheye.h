#pragma once

#include "ofMain.h"

class Fisheye {
public:
    ofMesh mesh, sampleMesh;
    ofVec2f offset = ofVec2f(960, 530);
    float width = 1920, height = 1080;
    float radius = 530;
    float fov = 180;
    int radiusResolution = 24;
    int thetaResolution = 92;
    
    void setup() {
        mesh = ofPlanePrimitive(1, 1, thetaResolution, radiusResolution).getMesh();
        sampleMesh = mesh;
        int k = 0;
        for(int i = 0; i < radiusResolution; i++) {
            for(int j = 0; j < thetaResolution; j++) {
                float sampleRadius = ofMap(i, 0, radiusResolution - 1, 0, radius);
                float sampleTheta = ofMap(j, 0, thetaResolution - 1, 0, 360);
                ofVec2f samplePos(sampleRadius, 0);
                samplePos.rotate(sampleTheta);
                ofVec3f vertex(0, 0, 1);
                vertex.rotate(0, ofMap(i, 0, radiusResolution - 1, 0, fov) / 2., 0);
                vertex.rotate(0, 0, sampleTheta);
                sampleMesh.setVertex(k, samplePos);
                ofVec2f texPos = samplePos + offset;
                mesh.setTexCoord(k, texPos);
                mesh.setVertex(k, vertex);
                k++;
            }
        }
    }
    void draw() {
        ofPushStyle();
        ofSetColor(ofColor::white);
        ofRotateX(180);
        mesh.drawFaces();
        ofPopStyle();
    }
};