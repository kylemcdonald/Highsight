// try using px instead of p to avoid overwriting commands
// check ?dr 1

var serialport = require('serialport');

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

var serial;
var serialStatus = 'initializing';
function updateSerialStatus(status) {
	if(status != serialStatus) {
		console.log('Roboteq is ' + status);
	}
	serialStatus = status;
}

var dataCallbackHook;
function dataCallback(data) {
	console.log('data: ' + data);
	if(data == '+' || data == '-') {
  	console.log('ignoring callback');
  	return;
	}
  if(dataCallbackHook) {
    dataCallbackHook(data);
    delete dataCallbackHook;
  }
}
exports.connect = function() {
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
    dataCallbackHook = cb;
  }
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
		cb(parts[1]);
	});
}

// based on:
// http://www.roboteq.com/index.php/docman/motor-controllers-documents-and-files/documentation/user-manual/7-nextgen-controllers-user-manual/file
// general notes (p 115):
// - commands are not case sensitive
// - commands are terminated by carriage return (hex 0x0d, '\r')
// - controller will echo every command it receives
// - for commands where no reply is expected, it will return a '+' character
// - for bad commands, it will return a '-' character
exports.setEcho = function(enable) {
  if(enable) {
    exports.command('^echof 0');
  } else {
    exports.command('^echof 1');
  }
}
exports.setAcceleration = function(acceleration) {
	exports.command('!ac 1 ' + acceleration);
}
exports.setDeceleration = function(deceleration) {
	exports.command('!dc 1 ' + deceleration);
}
exports.setSpeed = function(speed) { // units are .1 * RPM / s, called "set velocity" in manual
	exports.command('!s 1 ' + speed);
}
exports.setPosition = function(position) {
	exports.command('!p 1 ' + position);
}
exports.getPosition = function(cb) {
	exports.query('?c 1', cb); // also called "encoder counter absolute"
}
exports.getSpeed = function(cb) { // units are .1 * RPM / s, called "set velocity" in manual
	exports.query('?s 1', cb);
}
// returns internal voltage * 10 : main battery voltage * 10 : v5out on dsub in millivolts, see p 186
exports.getVolts = function(cb) {
	exports.query('?v 1', function(volts) {
  	cb(volts / 10);  	
	});
}
exports.getMotorAmps = function(cb) { // returns units of amps * 10, p 173
	exports.query('?A 1', function(amps) {
  	cb(amps / 10);  	
	});
}
exports.getBatteryAmps = function(cb) { // returns units of amps * 10, p 175
	exports.query('?BA 1', function(amps) {
  	cb(amps / 10);	
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