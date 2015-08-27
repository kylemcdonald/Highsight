// try using px instead of p to avoid overwriting commands
// check ?dr 1
// look for Roboteq manufacturer name

var serialport = require('serialport');
var express = require('express');
var app = express();

var comName = '/dev/cu.usbmodem1411';
var revolutionsPerMeter = 16.818;
var encoderResolution = 256;
var countsPerMeter = revolutionsPerMeter * encoderResolution;
var top = countsPerMeter * 9.25; // 14.16meters from floor at 120,000 = ~8611 counts per meter
var bottom = countsPerMeter * 0.10;
var slows = '800';
var fasts = '1300'; // 1300 seemed quite reliable, 1400 and 1350 crashed on first attempt
var downac = '15000'; // 15000
var downdc = '12000'; // 12000

app.use(express.static(__dirname + '/public'));

var server = app.listen(process.env.PORT || 3000, function () {
	var host = server.address().address;
	var port = server.address().port;
	console.log('Listening at http://%s:%s', host, port);
});

function listPorts() {
	serialport.list(function (err, ports) {
		console.log('Available ports:');
		ports.forEach(function(port) {
			console.log('comName: ' + port.comName);
			console.log('\tpnpId: ' + port.pnpId);
			console.log('\tmanufacturer: ' + port.manufacturer);
		});
	});
}
listPorts();

var serial;

function connect() {
	console.log('Trying to connect to ' + comName);
	if(serial && serial.isOpen()) return;
	serial = new serialport.SerialPort(comName, {
		baudrate: 115200,
		parser: serialport.parsers.readline('\r'),
		disconnectedCallback: function() {
			console.log('Got disconnected!');
			reconnect();
		}
	}, false);
	serial.open(function(err) {
		if(err) {
			reconnect();
			return;
		} else {
			console.log('Opened ' + comName);
		}
	});
}
function reconnect() {
	setTimeout(connect, 1000);	
}

connect();

function tellRobo(command) {
	if(!serial.isOpen()) return;
	serial.write(command + '\r\n', function(err, results) {
		if(err) console.log('err ' + err);
		if(results) console.log('results ' + results);
	});
}

function askRobo(command, cb) {
	if(!serial.isOpen()) {
		cb();
		return;
	}
	serial.on('data', function(data) {
		if(!data) {
			console.log('data event with no data');
			return;
		}
		console.log('got data: ' + data);
		var parts = data.split('=')[0];
		var type = parts[0].toLowerCase();
		cb(type, parts[1]);
	});
	serial.write(command + '\r\n', function(err, results) {
		if(err) console.log('err ' + err);
		if(results) console.log('results ' + results);
	});
}

app.get('/goDownFast', function(req, res) {
	console.log('/goDownFast');
	tellRobo('!s 1 ' + fasts);
	tellRobo('!ac 1 ' + downac);
	tellRobo('!dc 1 ' + downdc);
	tellRobo('!p 1 ' + bottom);
	res.send('done');
})

app.get('/goUpSlow', function(req, res) {
	console.log('/goUpSlow');
	tellRobo('!s 1 ' + slows);
	tellRobo('!ac 1 2000');
	tellRobo('!dc 1 2000');
	tellRobo('!p 1 ' + top);
	res.send('done');
})

app.get('/goDownSlow', function(req, res) {
	console.log('/goDownSlow');
	tellRobo('!s 1 ' + slows);
	tellRobo('!ac 1 2000');
	tellRobo('!dc 1 2000');
	tellRobo('!p 1 ' + bottom);
	res.send('done');
})

app.get('/readEncoderCounterAbsolute', function(req, res) {
	askRobo('?c 1', function(result) {
		console.log('got robo result: ' + result);
		res.json({'encoderCounterAbsolute': result});
	})
})