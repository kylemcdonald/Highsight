

// newAngle = angle measured with atan2 using the accelerometer
// newRate =  angle measured using the gyro
// looptime = loop time in millis() 


float Complementary2(float newAngle, float newRate,int looptime) {
 float k=10; 
 float dtc2=float(looptime)/1000.0;

x1 = (newAngle -   x_angle2C)*k*k;
y1 = dtc2*x1 + y1;
x2 = y1 + (newAngle -   x_angle2C)*2*k + newRate;
x_angle2C = dtc2*x2 + x_angle2C;


  return x_angle2C;
}


