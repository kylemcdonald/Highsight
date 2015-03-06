# OSC messages between server and motor controllers

Motors are [NW, NE, SE, SW] in that order, or numbered 0,1,2,3

## Server to motors (broadcast):


### send all 4 motors to set positions (motors handle velocity and acceleration)
Position is in encoder steps (approx 150 steps per cm) and calibration will be handled on the server.
```
/go
	float nwLength	# goal rope length in encoder steps
	float neLength
	float seLength
	float swLength
```

### request one motor to find its home position
Best to use a nice slow speed. If you think the motor is already pretty well homed and just want to confirm quickly, 
you can send it quickly to a position near home and then home it slowly.

Motor will report its state as HOMING or HOMINGBACKOFF until homing is complete.  

TODO: if homing seems to take longer than a reasonable time, state should go to HOMINGERROR

```
/home
	int motorID
	float speed	  # 10-12cm/sec is ok in small scale testing
```


### set one motor's maximum speed (in approx. cm/sec)
```
/maxspeed
	int motorID
	float maxSpeed
```


## Motors to server

### status report, sent frequently:
```
/status
	int motorID
	string state	# OK (not yet implemented: NOTHOMED, HOMING, STOPPED, or ENDSTOP)
	float position	# in encoder steps
	float velocity	# in approximate cm/sec
	

	
	