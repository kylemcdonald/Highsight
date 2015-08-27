var express = require('express');
var app = express();

var serialPort = require("serialport");
serialPort.list(function (err, ports) {
	console.log('Available ports:');
	ports.forEach(function(port) {
		console.log("comName: " + port.comName);
		console.log("\tpnpId: " + port.pnpId);
		console.log("\tmanufacturer: " + port.manufacturer);
	});
});

app.use(express.static(__dirname + '/public'));

// app.get('/', function (req, res) {
// 	res.send('Hello World!');
// });

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

var SerialPort = require("serialport").SerialPort
var serialPort = new SerialPort("/dev/cu.usbmodem1411", {
  baudrate: 115200,
  disconnectedCallback: function() {
  	// todo: need to reconnect here
  }
});

function tellRobo(command) {
	serialPort.write(command + '\r\n', function(err, results) {
		if(err) console.log('err ' + err);
		if(results) console.log('results ' + results);
	});
}

var revolutionsPerMeter = 16.818;
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution;
var top = countsPerMeter * 9.75; // 14.16meters from floor at 120,000 = ~8611 counts per meter
var bottom = countsPerMeter * 0.10;
var slows = "800";
var fasts = "1300"; // 1300 seemed quite reliable, 1400 and 1350 crashed on first attempt
var downac = "15000"; // 15000
var downdc = "12000"; // 12000

app.get('/goDownFast', function(req, res) {
	tellRobo('!s 1 ' + fasts);
	tellRobo('!ac 1 ' + downac);
	tellRobo('!dc 1 ' + downdc);
	tellRobo('!p 1 ' + bottom);
	res.send('done');
})

app.get('/goUpSlow', function(req, res) {
	tellRobo('!s 1 ' + slows);
	tellRobo('!ac 1 2000');
	tellRobo('!dc 1 2000');
	tellRobo('!p 1 ' + top);
	res.send('done');
})

app.get('/goDownSlow', function(req, res) {
	tellRobo('!s 1 ' + slows);
	tellRobo('!ac 1 2000');
	tellRobo('!dc 1 2000');
	tellRobo('!p 1 ' + bottom);
	res.send('done');
})

app.get('/inch', function(req, res) {
	tellRobo('!p 1 1000');
	res.send('done');
})