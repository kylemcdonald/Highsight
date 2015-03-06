# OSC messages between server and motor controllers

Motors are [NW, NE, SE, SW] in that order, or numbered 0,1,2,3

## Server to motors (broadcast):


### send all 4 motors to set positions (motors handle velocity and acceleration)
Position is in encoder steps (approx 150 steps per cm) and calibration will be handled on the server.
```
/go
	int nwLength	# goal rope length in encoder steps
	int neLength
	int seLength
	int swLength
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
	int position	# in encoder steps
	float velocity	# in approximate cm/sec
	

	
	