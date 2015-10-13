// guideline: pressing any button should always be safe

// if there is ever no current we need to stop.. need to work on this though

// use position to determine what state we are in before acting
// make state graph: from any position there are multiple actions
// need the serial to be blocking: easiest version is don't make a call if the dataCallback is defined
// make the controls bigger for the ipad
// debug info on ipad? more battery usage
// serial should work without echo changing
// 1:48 ipad 42% battery
// could keep a websockets connection open and stop if the iPad disconnects
// get the rotation corrected for oculus view

var roboteq = require('./roboteq.js');
var express = require('express');
var app = express();

var revolutionsPerMeter = 16.818; // 14.16meters from floor at 120,000 = ~8611 counts per meter
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution
var nudgeAmount = 0.10;
var safeDistance = 0.10;
var top = 8.0;
var bottom = 0.25;
var slows = 100;
var slowac = 1000;
var slowdc = 1000;
var fasts = 1300; // 1300 is quite reliable, 1400 and 1350 will crash if loop error is set to 500ms
var fastac = 15000; // 15000 
var fastdc = 12000; // 12000

function metersToEncoderUnits(meters) {
	return Math.round(countsPerMeter * meters)
}

function encoderUnitsToMeters(encoderUnits) {
	return encoderUnits / countsPerMeter;
}

var positions = {
	'top': 11.5,
	'boxtop': 10.05,
	'boxview': 9.5,
	'boxbottom': 8.9,
	'openair': 8.5,
	'bottom': 0.25
};

var transitionDefault = {
	s: slows,
	ac: slowac,
	dc: slowdc
}

var transitions = {
	'shutdown': {end: 'bottom'},
	'setup': {end: 'top'},
	'scene1': {start: 'top', end: 'boxtop'},
	'scene2': {start: 'boxtop', end: 'boxview'},
	'scene3': {start: 'boxview', end: 'boxview'}, // pause
	'scene4': {start: 'boxview', end: 'boxbottom'},
	'scene5': {start: 'boxbottom', end: 'openair'},
	'scene6': {start: 'openair', end: 'bottom', s: fasts, ac: fastac, dc: fastdc}, // should be fast
	'scene7': {start: 'bottom', end: 'openair'}, // should be slow
};

app.use(express.static(__dirname + '/public'));
app.use(express.static(__dirname + '/bower_components'));

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

// could pass encoder resolution here?
roboteq.connect({
	// these are limited internally as an error-check
	top: metersToEncoderUnits(11.5),
	bottom: metersToEncoderUnits(0.25),
	speedLimit: 1400,
	accelerationLimit: 15000,
	decelerationLimit: 12000
});

app.get('/roboteq/open', function(req, res) {
	res.json({
		'status': roboteq.serialStatus(),
		'open': roboteq.isOpen()
	})
})

function applyTransition(transition) {
	roboteq.setSpeed(transition.s || transitionDefault.s);
	roboteq.setAcceleration(transition.ac || transitionDefault.ac);
	roboteq.setDeceleration(transition.dc || transitionDefault.dc);
	var endPosition = positions[transition.end];
	roboteq.setPosition(metersToEncoderUnits(endPosition));
}

function safeApplyTransition(transition) {
	var startPositionName = transition.start;
	if(startPositionName) {
		var startPositionMeters = positions[startPositionName];
		roboteq.getPosition(function(positionEncoder) {
			var positionMeters = encoderUnitsToMeters(positionEncoder);
			if(Math.abs(positionMeters - startPositionMeters) < safeDistance) {
				applyTransition(transition);
			} else {
				console.log('ignoring unsafe call to safeApplyTransition');
			}
		})
	} else {
		applyTransition(transition);
	}
}

app.get('/roboteq/transition', function(req, res) {
	var name = req.query.name;
	console.log('/roboteq/transition to ' + name);
	var transition = transitions[name];
	if(transition) {
		safeApplyTransition(transition);
	}
	res.sendStatus(200);
})

app.get('/roboteq/nudge/up', function(req, res) {
	console.log('/roboteq/nudge/up');
	roboteq.setSpeed(slows);
	roboteq.setAcceleration(slowac);
	roboteq.setDeceleration(slowdc);
	roboteq.setPositionRelative(+metersToEncoderUnits(nudgeAmount));
	res.sendStatus(200);
})

app.get('/roboteq/nudge/down', function(req, res) {
	console.log('/roboteq/nudge/down');	
	roboteq.setSpeed(slows);
	roboteq.setAcceleration(slowac);
	roboteq.setDeceleration(slowdc);
	roboteq.setPositionRelative(-metersToEncoderUnits(nudgeAmount));
	res.sendStatus(200);
})

app.get('/roboteq/get/speed', function(req, res) {
  roboteq.getSpeed(function(result) {
    res.json({'speed': result});
  })
})

app.get('/roboteq/get/position', function(req, res) {
  roboteq.getPosition(function(result) {
    res.json({'position': result, 'meters': encoderUnitsToMeters(result)});
  })
})

app.get('/roboteq/get/volts', function(req, res) {
  roboteq.getVolts(function(result) {
    res.json({'volts': result});
  })
})

app.get('/roboteq/get/motor/amps', function(req, res) {
  roboteq.getMotorAmps(function(result) {
    res.json({'amps': result});
  })
})

app.get('/roboteq/get/battery/amps', function(req, res) {
  roboteq.getBatteryAmps(function(result) {
    res.json({'amps': result});
  })
})

app.get('/roboteq/get/destinationReached', function(req, res) {
  roboteq.getDestinationReached(function(result) {
    res.json({'destinationReached': result});
  })
})

app.get('/roboteq/set/echo', function(req, res) {
  var enable = (req.query.echo == 'true');
  roboteq.setEcho(enable);
  res.sendStatus(200);
})