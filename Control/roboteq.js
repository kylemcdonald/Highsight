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
function updateSerialStatus(status) {
  if(status !== serialStatus) {
    console.log('Roboteq is ' + status);
  }
  serialStatus = status;
}

// step 1: only respond to callbacks with the correct event
// step 2: add recentData that keeps track of most recent replies
// step 3: ability to add listeners that update when new data is available
// step 4: switch from query-based to cache-based requests and regular polling
// step 5: try to re-enable echo

// listeners are called every time
var listeners = { };
var recentData = { };
function addListener(name, cb) {
  if(!(name in listeners)) {
    console.log('creating listener array for ' + name);
    listeners[name] = [ ];
  }
  console.log('adding listener for ' + name);
  listeners[name].push(cb);
}
function callListeners(name, value) {
  recentData[name] = value;
  if(name in listeners) {
    listeners[name].forEach(function (cb) {
      console.log('calling listener  ' + name + ' with ' + value)
      cb(value);
    })
  } else {
    console.log('no listeners registered for ' + name);    
  }
}

// callbacks are called once and deleted
var callbacks = { };
function addCallback(name, cb) {
  if(!(name in callbacks)) {
    console.log('creating callback array for ' + name);
    callbacks[name] = [ ];
  }
  console.log('adding callback for ' + name);
  callbacks[name].push(cb);
}
function callCallbacks(name, value) {
  if(name in callbacks) {
    var cbs = callbacks[name];
    while(cbs.length > 0) {
      console.log('calling callback ' + name + ' with ' + value)
      cbs.pop()(value);
    }
  } else {
    console.log('no callbacks registered for ' + name);
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
    var value = Number(parts[1]); 
    callCallbacks(name, value);
    callListeners(name, value);
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

function write(command, cb) {
  if(cb) {
    var name = command.substring(1).split(' ')[0];
    addCallback(name, cb);
  }
  // console.log('write: ' + command);
  serial.write(command, function(err) {
    if(err) console.log('err ' + err);    
  })
}

exports.command = function(command) {
  if(!exports.isOpen()) {
    // console.log('Ignored command: ' + command);
    return;
  }
  write(command + '\r');
}

exports.query = function(query, cb) {
  if(!exports.isOpen()) {
    // console.log('Ignored query: ' + query);
    cb();
    return;
  }
  write(query + '\r', cb);
}

// based on:
// http://www.roboteq.com/index.php/docman/motor-controllers-documents-and-files/documentation/user-manual/7-nextgen-controllers-user-manual/file
// general notes (p 115):
// - commands are not case sensitive (but always returned as capitalized)
// - commands are terminated by carriage return (hex 0x0d, '\r')
// - controller will echo every command it receives (unless this is disabled)
// - for commands where no reply is expected, it will return a '+' character
// - for bad commands, it will return a '-' character

// set/command
exports.setEcho = function(enable) {
  if(enable) {
    exports.command('^ECHOF 0');
  } else {
    exports.command('^ECHOF 1');
  }
}
exports.setAcceleration = function(acceleration) {
  if(unsafe(acceleration)) return;
  acceleration = clamp(acceleration, 0, config.accelerationLimit);
  exports.command('!AC 1 ' + acceleration);
}
exports.setDeceleration = function(deceleration) {
  if(unsafe(deceleration)) return;
  deceleration = clamp(deceleration, 0, config.decelerationLimit);
  exports.command('!DC 1 ' + deceleration);
}
exports.setSpeed = function(speed) { // units are .1 * RPM / s, called "set velocity" in manual
  if(unsafe(speed)) return;
  speed = clamp(speed, 0, config.speedLimit);
  exports.command('!S 1 ' + speed);
}
exports.setPosition = function(position) {
  if(unsafe(position)) return;
  position = clamp(position, config.bottom, config.top);
  exports.command('!P 1 ' + position);
}
exports.setPositionRelative = function(distance) {
  if(unsafe(distance)) return;
  exports.setPosition(result + Number(distance));
}
exports.startAutomaticSending = function(interval) {
  if(unsafe(interval)) return;
  interval = clamp(interval, 200, 10000); // max 5hz
  exports.command('# ' + interval);
}
exports.stopAutomaticSending = function() {
  exports.command('#');
}
exports.clearBufferHistory = function() {
  exports.command('# C');
}

// get/query
exports.getPosition = function(cb) {
  exports.query('?C 1', cb); // also called "encoder counter absolute"
}
exports.getSpeed = function(cb) { // units are .1 * RPM / s, called "set velocity" in manual
  exports.query('?S 1', cb);
}
// returns voltage * 10 : main battery voltage * 10 : v5out on dsub in millivolts, see p 186
// ?v 1 returns internal voltage
// ?v 2 returns motor voltage
// ?v 3 return 5v on dsub output
exports.getVolts = function(cb) {
  exports.query('?V 2', function(volts) {
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

// following commands are untested
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