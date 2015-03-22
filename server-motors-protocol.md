# OSC messages between server and motor controllers

Motors are numbered 0,1,2,3 but position in room may vary. Use a lookup table to remember which is NW, NE, SE, or SW motor.

## Server to motors (broadcast):


### send all 4 motors to set positions (motors handle velocity and acceleration)
Position is in encoder steps (approx 150 steps per cm) and calibration will be handled on the server.
```
/go
	float length0	# goal rope length in encoder steps
	float length1
	float length2
	float length3
```
_NOT TESTED_ - set goal positions with advisory speed (in approx cm/sec):

```
/go
	float length0	# goal rope length in encoder steps
	float speed0	# maximum speed in approximate cm/sec
	float length1
	float speed1
	float length2
	float speed2
	float length3
	float speed3
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


### emergency stop
Should probably always stop all motors - could be dangerous to keep running with one stopped motor.

If motor is OK, its state will change to STOPPED; if HOMING/HOMINGBACKOFF it will go to NOTHOMED; else it will stay in whatever state it was in. (OK and the HOMING states are the only ones where the motor can be moving.)

Homing a motor will clear STOPPED state, as will /resume

Stop all motors:
```
/stop
```

Stop one motor:
```
/stop
	int motorID
```

### resume after stop
Get back in service after e-stop - can address all motors (no argument) or just one. Motor will return to OK state if it was OK before stopping.
```
/resume

/resume
	int motorID
```


### set one or all motors' maximum speed (in approx. cm/sec)
```
/maxspeed
	float maxSpeed
	
/maxspeed
	int motorID
	float maxSpeed
```

### set one or all motors' maximum acceleration (in approx. cm/sec^2)
```
/maxaccel
	float maxAccel
	
/maxaccel
	int motorID
	float maxAccel
```

### set one or all motors' dead zone (in encoder steps)
If encoder is within +/- dead zone of goal, PID calls it good enough and doesn't try to refine further. Default 15 encoder units. Don't make it too small or motors will hunt back and forth forever.

Still dead zone when speed is 0 - have a larger zone (eg 15) so that motor doesn't keep seeking to try to get unimportant precise placement.
Moving dead zone is for speed > 0 - smaller zone (e.g. 4) allows smoother slow movements.

All motors:
```
/deadzone
	int still	# positive integer 
	int moving
```

One motor:
```
/deadzone
	int motorID
	int still
	int moving
```


### power control 
Turn motor power off or on - with motors off it's ok to _slowly_ move ropes by hand.

Driver will go to MOTOROFF state, and then to NOTHOMED state when power is turned back on.

All motors:
```
/motor
	int status (0=off, 1=on)
```

One motor:
```
/motor
	int motorID
	int status (0=off, 1=on)
```


### status report interval
Set the time delay between status messages, in msec

```
/statusinterval
	int interval (in msec)
/statusinterval
	int motorID
	int interval
```

### server IP address
**DISABLED**
Sets the destination IP address for /status messages

```
/serveraddress
	int, int, int, int // the four bytes of the IP address
/serveraddress
	int motorID
	int, int, int, int
```


### enable attempt to remember position through a crash
send 1 or 0 to enable or disable trying to recover the encoder position after crash
```
/rememberposition
	int	// 1 for yes, 0 for no
/rememberposition
	int motorID
	int	// 1 for yes, 0 for no
```


### force calibration by setting position and forcing homed state
Send float to set new encoder position
```
/setposition
	float	// new position
```


## Motors to server

### status report, sent frequently.

Possible states:

* OK - homed and ready to go
* NOTHOMED - hasn't been homed, don't trust position report
* NOTHOMED-OFF - same, and the motor is turned off
* MOTOROFF - motor is turned off (but has been homed)
* HOMING, HOMINGBACKOFF - in the process of homing
* ENDSTOP - hit the endstop unexpectedly, will have to be re-homed.
* STOPPED - stop mode

```
/status
	int motorID
	string state	# see list above
	float position	# in encoder steps
	float velocity	# in approximate cm/sec
	int stepper	# NO LONGER SUPPORTED - # of stepper steps since last status
	int encoder	# NO LONGER SUPPORTED - # of encoder steps
	int reboots	# if using crash recovery, how many times restarted and recovered encoder position since last real homing
	

	
	
