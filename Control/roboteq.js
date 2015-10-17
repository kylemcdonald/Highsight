// based on:
// http://www.roboteq.com/index.php/docman/motor-controllers-documents-and-files/documentation/user-manual/7-nextgen-controllers-user-manual/file
// general notes (p 115):
// - commands are not case sensitive (but always returned as capitalized)
// - commands are terminated by carriage return (hex 0x0d, '\r')
// - controller will echo every command it receives (unless this is disabled)
// - for commands where no reply is expected, it will return a '+' character
// - for bad commands, it will return a '-' character

var serialport = require('serialport');

function unsafe(x) {
  if(x === undefined || x === null || isNaN(x)) {
    console.log('unsafe value passed to roboteq: ' + x);
    return true;
  }
  return false;
}

function clamp(x, low, high) {
  if(x < low) return low;
  if(x > high) return high;
  return x;
}

function emptyObject(x) {
  return Object.keys(x).length == 0;
}

function getComName(cb) {
  serialport.list(function(err, ports) {
    var found = ports.some(function(port) {
      if(port.manufacturer === 'Roboteq') {
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

var config = { };
var serial;
var serialStatus = 'initializing';
function setSerialStatus(status) {
  if(status !== serialStatus) {
    console.log('Roboteq is ' + status);
  }
  serialStatus = status;
}

// listeners are called every time
var listeners = { };
var cache = { };
var lastUpdate;
function addListener(name, cb) {
  if(!(name in listeners)) {
    // console.log('creating listener array for ' + name);
    listeners[name] = [ ];
  }
  // console.log('adding listener for ' + name);
  listeners[name].push(cb);
}
function callListeners(name, value) {
  if(name in listeners) {
    listeners[name].forEach(function (cb) {
      // console.log('calling listener  ' + name + ' with ' + value)
      cb(value);
    })
  } else {
    // console.log('no listeners registered for ' + name);    
  }
}

function dataCallback(data) {
  if(data === '+') {
    // console.log('dataCallback: no reply expected');
    return;
  }
  if(data === '-') {
    console.log('dataCallback: bad command');
    return;
  }
  var parts = data.split('=');
  if(parts.length > 1) {
    var name = parts[0];
    var value = parts[1];
    lastUpdate = new Date();
    callListeners(name, value);
  }
}

function addDefaultListeners() {
  if(emptyObject(listeners)) {
    var voltsMultiplier = 1. / 10.;
    var millivoltsMultiplier = 1. / 1000.;
    var ampsMultiplier = 1. / 10.;
    // also called "encoder counter absolute"
    addListener('C', function(value) {
      var parts = value.split(':');
      cache.position = Number(parts[0]);
    })
    // units are .1 * RPM / s, called "set velocity" in manual
    addListener('S', function(value) {
      var parts = value.split(':');
      cache.speed = Number(parts[0]);
    })
    // returns voltage * 10 : main battery voltage * 10 : v5out on dsub in millivolts, see p 186
    addListener('V', function(value) {
      var parts = value.split(':');
      cache.internalVolts = Number(parts[0]) * voltsMultiplier;
      cache.motorVolts = Number(parts[1]) * voltsMultiplier;
      cache.dsubVolts = Number(parts[2]) * millivoltsMultiplier;
    })
    // returns units of amps * 10, p 173
    addListener('A', function(value) {
      var parts = value.split(':');
      cache.motorAmps = Number(parts[0]) * ampsMultiplier;
    })
    // returns units of amps * 10, p 175
    addListener('BA', function(value) {
      var parts = value.split(':');
      cache.batteryAmps = Number(parts[0]) * ampsMultiplier;
    })
    // p 179, p 104
    addListener('DR', function(value) {
      var parts = value.split(':');
      cache.destinationReached = Number(parts[0]);
    })
  }
}

function startupSequence() {
  exports.setEcho(false);
  // automatic sending is described on p 188 and 189
  exports.command('#'); // stop automatic sending
  exports.command('# C'); // clear command history
  exports.command('?C'); // encoder counter position
  exports.command('?S'); // speed
  exports.command('?V'); // volts
  exports.command('?A'); // motor amps
  exports.command('?BA'); // battery amps
  exports.command('?DR'); // destination reached
  exports.command('# 10'); // start automatic sending
}

exports.connect = function(params) {
  addDefaultListeners();
  if(params) config = params;
  if(serial && serial.isOpen()) return;
  getComName(function(err, comName) {
    if(err) {
      setSerialStatus('not found, searching');
      reconnect();
      return;
    }
    serial = new serialport.SerialPort(comName, {
      baudrate: 115200,
      parser: serialport.parsers.readline('\r'),
      disconnectedCallback: function() {
        setSerialStatus('disconnected, reconnecting');
        reconnect();
      }
    }, false);
    serial.on('data', dataCallback);
    serial.open(function(err) {
      if(err) {
        setSerialStatus('error connecting, reconnecting');
        reconnect();
      } else {
        setSerialStatus('connected');
        startupSequence();
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
  // console.log('write: ' + command);
  if(!exports.isOpen()) {
    // console.log('Ignored command: ' + command);
    return;
  }
  command += '\r';
  serial.write(command, function(err) {
    if(err) console.log('serial.write error: ' + err);
  })
}

// set/command
exports.setEcho = function(enable) {
  if(enable) {
    exports.command('^ECHOF 0');
  } else {
    exports.command('^ECHOF 1');
  }
}
exports.setAcceleration = function(acceleration) {
  acceleration = Number(acceleration);
  if(unsafe(acceleration)) return;
  acceleration = clamp(acceleration, 0, config.accelerationLimit);
  exports.command('!AC 1 ' + acceleration);
}
exports.setDeceleration = function(deceleration) {
  deceleration = Number(deceleration);
  if(unsafe(deceleration)) return;
  deceleration = clamp(deceleration, 0, config.decelerationLimit);
  exports.command('!DC 1 ' + deceleration);
}
exports.setSpeed = function(speed) {
  speed = Number(speed);
  if(unsafe(speed)) return;
  speed = clamp(speed, 0, config.speedLimit);
  exports.command('!S 1 ' + speed);
}
exports.setPosition = function(position) {
  position = Number(position);
  if(unsafe(position)) return;
  position = clamp(position, config.bottom, config.top);
  exports.command('!P 1 ' + position);
}
exports.setPositionRelative = function(distance) {
  distance = Number(distance);
  if(unsafe(distance)) return;
  var currentPosition = cache.position;
  if(unsafe(currentPosition)) return;
  exports.setPosition(currentPosition + distance);
}
exports.getLatency = function() {
  return lastUpdate ? new Date() - lastUpdate : undefined;
}

// get internal cached state: returns parsed data
exports.getCache = function() { return cache }
exports.getPosition = function() { return cache.position }
exports.getSpeed = function() { return cache.speed }
exports.getMotorVolts = function() { return cache.motorVolts }
exports.getMotorAmps = function() { return cache.motorAmps }
exports.getBatteryAmps = function() { return cache.batteryAmps }
exports.getDestinationReached = function() { return cache.destinationReached }