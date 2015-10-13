var roboteq = require('./roboteq.js');
var express = require('express');
var app = express();

var revolutionsPerMeter = 16.818;
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution;
var top = Math.round(countsPerMeter * 4.6);// 9.25 // 14.16meters from floor at 120,000 = ~8611 counts per meter
var bottom = Math.round(countsPerMeter * 0.10);
var slows = 200;
var slowac = 2000;
var fasts = 1300; // 1300 is quite reliable, 1400 and 1350 will crash if loop error is set to 500ms
var fastac = 15000; // 15000 
var fastdc = 12000; // 12000

app.use(express.static(__dirname + '/public'));
app.use(express.static(__dirname + '/bower_components'));

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

roboteq.connect(); // should pass encoder resolution here

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

app.get('/roboteq/get/speed', function(req, res) {
  roboteq.getSpeed(function(result) {
    res.json({'speed': result});
  })
})

app.get('/roboteq/get/position', function(req, res) {
  roboteq.getPosition(function(result) {
    res.json({'position': result});
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