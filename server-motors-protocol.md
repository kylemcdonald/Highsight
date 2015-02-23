# OSC messages between server and motor controllers

Motors are [NW, NE, SE, SW] in that order

## Server to motors (broadcast):

```
/motors
	float nwLength	# goal cable length in cm
	float nwSpeed	# goal cable speed in cm/sec
	float neLength
	float neSpeed
	float seLength
	float seSpeed
	float swLength
	float swSpeed
	
```

## Motors to server

### status report, sent frequently:
```
/motor
	int id			# 0=NW, 1=NE, etc
	float position	# in cm
	float speed		# in cm/sec
```