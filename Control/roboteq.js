// try using px instead of p to avoid overwriting commands
// check ?dr 1

var serialport = require('serialport');

function clamp(x, low, high) {
	if(x < low) return low;
	if(x > high) return high;
	return x;
}

function getComName(cb) {
	serialport.list(function(err, ports) {
		var found = ports.some(function(port) {
			if(port.manufacturer == 'Roboteq') {
				cb(null, port.comName);
				return true;				
			} else {
  			return false;
			}
		})
		if(!found) {
  		cb('Cannot find matching port.');
		}
	})
}

var config = {};
var serial;
var serialStatus = 'initializing';
function updateSerialStatus(status) {
	if(status != serialStatus) {
		console.log('Roboteq is ' + status);
	}
	serialStatus = status;
}

var dataCallbackHooks = [];
function dataCallback(data) {
	if(data == '+') {
		console.log('dataCallback: no reply expected');
		if(dataCallbackHooks.length > 0) {
			dataCallbackHooks.pop();
		}
		return;
	}
	if(data == '-') {
		console.log('dataCallback: bad command');
		if(dataCallbackHooks.length > 0) {
			dataCallbackHooks.pop();
		}
		return;
	}
	console.log('dataCallback: ' + data);
	if(dataCallbackHooks.length > 0) {
		// console.log("deleting callback hook");
		dataCallbackHooks.pop()(data);
	}
}

exports.connect = function(params) {
	if(params) config = params;
	if(serial && serial.isOpen()) return;
	getComName(function(err, comName) {
		if(err) {
			updateSerialStatus('not found, searching');
			reconnect();
			return;
		}
		serial = new serialport.SerialPort(comName, {
			baudrate: 115200,
			parser: serialport.parsers.readline('\r'),
			disconnectedCallback: function() {
				updateSerialStatus('disconnected, reconnecting');
				reconnect();
			}
		}, false);
		serial.on('data', dataCallback);
		serial.open(function(err) {
			if(err) {
				updateSerialStatus('error connecting, reconnecting');
				reconnect();
			} else {
				updateSerialStatus('connected');
				exports.setEcho(false);
			}
		});
	})
}
function reconnect(description) {
	setTimeout(exports.connect, 1000);
}

exports.isOpen = function() {
	return Boolean(serial && serial.isOpen());
}

exports.serialStatus = function() {
	return serialStatus;
}

exports.command = function(command) {
	if(!exports.isOpen()) {
		console.log('Ignored command: ' + command);
		return;
	}
	write(command + '\r');
}

function write(command, cb) {
	if(cb) {
		dataCallbackHooks.push(cb);
	}
	console.log('write: ' + command);
	serial.write(command, function(err) {
		if(err) console.log('err ' + err);    
	})
}

exports.query = function(query, cb) {
	if(!exports.isOpen()) {
		console.log('Ignored query: ' + query);
		cb();
		return;
	}
	write(query + '\r', function(data) {
		var parts = data.split('=');
		var number = Number(parts[1]); // this will fail if a command does not return a number
		cb(number);
	});
}

function unsafe(x) {
	if(x === undefined || x == null || isNaN(x)) {
		console.log("unsafe value passed to roboteq: " + x);
		return true;
	}
	return false;
}

// based on:
// http://www.roboteq.com/index.php/docman/motor-controllers-documents-and-files/documentation/user-manual/7-nextgen-controllers-user-manual/file
// general notes (p 115):
// - commands are not case sensitive
// - commands are terminated by carriage return (hex 0x0d, '\r')
// - controller will echo every command it receives
// - for commands where no reply is expected, it will return a '+' character
// - for bad commands, it will return a '-' character
exports.safeCall = function(cb) {
	cb();
	// exports.getMotorAmps(function(amps) {
	// 	if(amps > 0) {
	// 		cb();
	// 	}
	// })
}
exports.setEcho = function(enable) {
	if(enable) {
		exports.command('^echof 0');
	} else {
		exports.command('^echof 1');
	}
}
exports.setAcceleration = function(acceleration) {
	if(unsafe(acceleration)) return;
	acceleration = clamp(acceleration, 0, config.accelerationLimit);
	exports.command('!ac 1 ' + acceleration);
}
exports.setDeceleration = function(deceleration) {
	if(unsafe(deceleration)) return;
	deceleration = clamp(deceleration, 0, config.decelerationLimit);
	exports.command('!dc 1 ' + deceleration);
}
exports.setSpeed = function(speed) { // units are .1 * RPM / s, called "set velocity" in manual
	if(unsafe(speed)) return;
	speed = clamp(speed, 0, config.speedLimit);
	exports.command('!s 1 ' + speed);
}
exports.setPosition = function(position) {
	if(unsafe(position)) return;
	exports.safeCall(function() {
		position = clamp(position, config.bottom, config.top);
		exports.command('!p 1 ' + position);
	})
}
exports.setPositionRelative = function(distance) {
	if(unsafe(distance)) return;
	exports.getPosition(function(result) {
		console.log('setPositionRelative to ' + distance + ' plus ' + result);
		exports.setPosition(result + Number(distance));
	})
}
exports.getPosition = function(cb) {
	exports.query('?c 1', cb); // also called "encoder counter absolute"
}
exports.getSpeed = function(cb) { // units are .1 * RPM / s, called "set velocity" in manual
	exports.query('?s 1', cb);
}
// returns voltage * 10 : main battery voltage * 10 : v5out on dsub in millivolts, see p 186
// ?v 1 returns internal voltage
// ?v 2 returns motor voltage
// ?v 3 return 5v on dsub output
exports.getVolts = function(cb) {
	exports.query('?v 2', function(volts) {
		cb(volts / 10.);
	});
}
exports.getMotorAmps = function(cb) { // returns units of amps * 10, p 173
	exports.query('?A 1', function(amps) {
		cb(amps / 10.); 
	});
}
exports.getBatteryAmps = function(cb) { // returns units of amps * 10, p 175
	exports.query('?BA 1', function(amps) {
		cb(amps / 10.);
	})
}
exports.getDestinationReached = function(cb) { // p 179, p 104
	exports.query('?DR 1', cb);
}

// follow commands are untested
exports.getFaults = function(cb) {
	exports.query('?FF 1', cb); // see p 180 for the meaning of each bit
}
exports.getRuntimeStatus = function(cb) {
	exports.query('?FM 1', cb); // see p 181 for the meaning of each bit
}
exports.getStatus = function(cb) {
	exports.query('?FS 1', cb); // see p 181 for the meaning of each bit
}
// see p 188 and 189 for a way to set up the roboteq to automatically return stats