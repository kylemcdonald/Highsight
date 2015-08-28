// try using px instead of p to avoid overwriting commands
// check ?dr 1

var serialport = require('serialport');
var express = require('express');
var app = express();

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
app.use(express.static(__dirname + '/bower_components'));

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

function getComName(description, cb) {
	serialport.list(function(err, ports) {
		ports.forEach(function(port) {
			for(key in description) {
				if(description[key] == port[key]) {
					cb(null, port.comName);
					return;
				}
			}
		})
		cb('Cannot find matching port.');
	})
}

var serial;
var serialStatus = 'initializing';
function updateSerialStatus(status) {
	if(status != serialStatus) {
		console.log('Serial is ' + status);
	}
	serialStatus = status;
}
function connect(description) {
	if(serial && serial.isOpen()) return;
	getComName(description, function(err, comName) {
		if(err) {
			updateSerialStatus('not found, searching');
			reconnect(description);
			return;
		}
		serial = new serialport.SerialPort(comName, {
			baudrate: 115200,
			parser: serialport.parsers.readline('\r'),
			disconnectedCallback: function() {
				updateSerialStatus('disconnected, reconnecting');
				setTimeout(function() { connect(description) }, 1000);
			}
		}, false);
		serial.open(function(err) {
			if(err) {
				updateSerialStatus('error connecting, reconnecting');
				reconnect(description);
			} else {
				updateSerialStatus('connected');
			}
		});
	})
}
function reconnect(description) {
	setTimeout(function() { connect(description) }, 1000);	
}
connect({manufacturer: 'Roboteq'});

app.get('/serial/open', function(req, res) {
	res.json({
		'status': serialStatus,
		'open': Boolean(serial && serial.isOpen())
	})
})

app.get('/serial/list', function(req, res) {
	serialport.list(function (err, ports) {
		if(err) res.sendStatus(500);
		else res.json(ports);
	})
})

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