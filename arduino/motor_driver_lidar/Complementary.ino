float tau=0.2; // 0.075
float a=0.0;

// a=tau / (tau + loop time)
// newAngle = angle measured with atan2 using the accelerometer
// newRate =  angle measured using the gyro
// looptime = loop time in millis() 


float Complementary(float newAngle, float newRate,int looptime) {
  float dtC = float(looptime)/1000.0;                                    
  a=tau/(tau+dtC) ;
  x_angleC= a* (x_angleC + newRate * dtC) + (1-a) * (newAngle);

  return x_angleC;
}

