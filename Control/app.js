var roboteq = require('./roboteq.js');
var express = require('express');
var app = express();

var revolutionsPerMeter = 16.818;
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution;
var top = Math.round(countsPerMeter * 9.25); // 14.16meters from floor at 120,000 = ~8611 counts per meter
var bottom = Math.round(countsPerMeter * 0.10);
var slows = 800;
var slowac = 2000;
var fasts = 1300; // 1300 seemed quite reliable, 1400 and 1350 crashed on first attempt
var fastac = 15000; // 15000 
var fastdc = 12000; // 12000

app.use(express.static(__dirname + '/public'));
app.use(express.static(__dirname + '/bower_components'));

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

roboteq.connect();

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
})

app.get('/roboteq/go/up', function(req, res) {
	console.log('/roboteq/go/up');
	roboteq.setSpeed(slows);
	roboteq.setAcceleration(slowac);
	roboteq.setDeceleration(slowac);
	roboteq.setPosition(top);
})

app.get('/roboteq/get/position', function(req, res) {
	roboteq.getPosition(function(result) {
		console.log('got robo result: ' + result);
		res.json({'encoderCounterAbsolute': result});
	})
})