// guideline: pressing any button should always be safe
// need to fix position-getting
// use position to determine what state we are in before acting
// make state graph: from any position there are multiple actions
// implement nudge up and down
// if there is ever no current we need to stop

var roboteq = require('./roboteq.js');
var express = require('express');
var app = express();

var revolutionsPerMeter = 16.818; // 14.16meters from floor at 120,000 = ~8611 counts per meter
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution;
var top = Math.round(countsPerMeter * 4.0);
var bottom = Math.round(countsPerMeter * 0.25);
var slows = 800;
var slowac = 2000;
var fasts = 400;//1300; // 1300 is quite reliable, 1400 and 1350 will crash if loop error is set to 500ms
var fastac = 1000;//15000; // 15000 
var fastdc = 1000;//12000; // 12000

app.use(express.static(__dirname + '/public'));
app.use(express.static(__dirname + '/bower_components'));

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

roboteq.connect({
	// these are limited internally as an error-check
	top: Math.round(countsPerMeter * 5.0),
	bottom: Math.round(countsPerMeter * 0.25),
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

app.get('/roboteq/go/down', function(req, res) {
	console.log('/roboteq/go/down');
	roboteq.setSpeed(fasts);
	roboteq.setAcceleration(fastac);
	roboteq.setDeceleration(fastdc);
	roboteq.setPosition(bottom);
	res.sendStatus(200);
})

app.get('/roboteq/go/up', function(req, res) {
	console.log('/roboteq/go/up');
	roboteq.setSpeed(slows);
	roboteq.setAcceleration(slowac);
	roboteq.setDeceleration(slowac);
	roboteq.setPosition(top);
	res.sendStatus(200);
})

app.get('/roboteq/get/position', function(req, res) {
	roboteq.getPosition(function(result) {
		console.log('got robo result: ' + result);
		// res.json({'encoderCounterAbsolute': result});
	})
	res.json({'encoderCounterAbsolute': '1'});
})