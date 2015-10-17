// guideline: pressing any button should always be safe

// if there is ever no current we need to stop.. need to check multiple times though
// rewriting serial to handle out of order messages (and with echo enabled), chart them in realtime
// realtime preview from OF to iPad
// make nodemon start without password?
// return to bottom after enough time has passed

var winston = require('winston');
var logger = new (winston.Logger)({
  transports: [
    new (winston.transports.Console)({ level: 'info' }),
    new (winston.transports.File)({ filename: 'logfile.log', level: 'debug' })
  ]
});

var roboteq = require('./roboteq.js');
var osc = require('osc');
var express = require('express');
var _ = require('lodash');
var app = express();

var revolutionsPerMeter = 16.818; // 14.16meters from floor at 120,000 = ~8611 counts per meter
var encoderResolution = 256;
var minimumVoltage = 22;
var countsPerMeter = revolutionsPerMeter * encoderResolution
var nudgeAmount = 0.10;
var safeDistance = 0.05;
var returnToBottomDuration = 1000 * 60 * 5; // time after last transition to return to bottom
var boxs = 30;
var boxac = 100;
var boxdc = 100;
var slows = 150;
var slowac = 500;
var slowdc = 500;
var fasts = 1300;
var fastac = 15000;
var fastdc = 12000;

function metersToEncoderUnits(meters) {
  return Math.round(countsPerMeter * meters)
}

function encoderUnitsToMeters(encoderUnits) {
  return encoderUnits / countsPerMeter;
}

var positions = {
  'top': 11.40,
  'boxtop': 9.8,
  'boxbottom': 8.8,
  'openair': 8.4,
  'myself': 1.4,
  'bottom': 0.1
};

var transitionDefault = {
  s: slows,
  ac: slowac,
  dc: slowdc
}

var transitions = {
  'shutdown': {end: 'bottom'},
  'top': {end: 'top'},
  'bottom': {end: 'bottom'},
  'scene1': {start: 'top', end: 'boxtop'},
  'scene2': {start: 'boxtop', end: 'boxbottom', s: boxs, ac: boxac, dc: boxdc},
  'scene3': {start: 'boxbottom', end: 'openair'},
  'scene4': {start: 'openair', end: 'myself', s: fasts, ac: fastac, dc: fastdc}, // should be fast
  'scene5': {start: 'myself', end: 'openair'}, // should be slow
};

app.use(express.static(__dirname + '/public'));
app.use(express.static(__dirname + '/bower_components'));

var udpPort = new osc.UDPPort({
    localAddress: 'localhost'
});
udpPort.open();

var server = app.listen(process.env.PORT || 3000, function () {
  var host = server.address().address;
  var port = server.address().port;
  logger.info('Listening at http://%s:%s', host, port);
});

// could pass encoder resolution here?
roboteq.connect({
  // these are limited internally as an error-check
  top: metersToEncoderUnits(11.40),
  bottom: metersToEncoderUnits(0.1),
  speedLimit: 1400,
  accelerationLimit: 15000,
  decelerationLimit: 12000
});

app.get('/roboteq/open', function(req, res) {
  res.json({
    'status': active ? roboteq.serialStatus() : 'Low Voltage',
    'open': roboteq.isOpen()
  })
})

// setInterval(function() {
//   if(active) {
//     var volts = roboteq.getVolts();
//     if(volts) {
//       logger.debug('getVolts', {volts: volts});
//       if(volts < minimumVoltage) {
//         logger.warn('Past minimum voltage, shutting down.');
//         safeApplyTransition('shutdown');
//       }
//     }
//   }
// }, 1000);

var returnToBottomTimeout;
function applyTransition(transitionName) {
  if(returnToBottomTimeout) {
    clearTimeout(returnToBottomTimeout);
  }
  var transition = transitions[transitionName];
  roboteq.setSpeed(transition.s || transitionDefault.s);
  roboteq.setAcceleration(transition.ac || transitionDefault.ac);
  roboteq.setDeceleration(transition.dc || transitionDefault.dc);
  var endPosition = positions[transition.end];
  roboteq.setPosition(metersToEncoderUnits(endPosition));
  returnToBottomTimeout = setTimeout(function() {
    safeApplyTransition('bottom');
    winston.log('Returning to bottom');
  }, returnToBottomDuration);
}

// possible error state where the data from roboteq is stale
// but we still believe it
function getSafeTransitions() {
  var positionEncoder = roboteq.getPosition();
  if(!active || positionEncoder === undefined) {
    return [];
  }
  var positionMeters = encoderUnitsToMeters(positionEncoder);
  logger.debug('getPosition', {positionMeters: positionMeters});
  var safeTransitions = [];
  for(transitionName in transitions) {
    var transition = transitions[transitionName];
    var startPositionName = transition.start;
    if(startPositionName) {
      var startPositionMeters = positions[startPositionName];
      if(Math.abs(positionMeters - startPositionMeters) < safeDistance) {
        safeTransitions.push(transitionName);
      }
    } else {
      safeTransitions.push(transitionName);
    }
  }
  return safeTransitions;
}

var active = true;
function safeApplyTransition(transitionName) {
  var safe = getSafeTransitions();
  if(safe.indexOf(transitionName) > -1) {
    logger.verbose('Applying safe transition: ' + transitionName);
    applyTransition(transitionName);
  } else {
    logger.warn('Ignoring unsafe transition: ' + transitionName);      
  }
  if(transitionName === 'shutdown') {
    active = false;
  }
}

app.get('/roboteq/transition', function(req, res) {
  var name = req.query.name;
  logger.verbose('/roboteq/transition to ' + name);
  safeApplyTransition(name);
  res.sendStatus(200);
})

app.get('/roboteq/automatic/clear', function(req, res) {
  roboteq.clearBufferHistory();
  res.sendStatus(200);
})

app.get('/roboteq/automatic/start', function(req, res) {
  roboteq.startAutomaticSending(10);
  res.sendStatus(200);
})

app.get('/roboteq/automatic/stop', function(req, res) {
  roboteq.stopAutomaticSending();
  res.sendStatus(200);
})

app.get('/roboteq/nudge/up', function(req, res) {
  logger.verbose('/roboteq/nudge/up');
  roboteq.setSpeed(slows);
  roboteq.setAcceleration(slowac);
  roboteq.setDeceleration(slowdc);
  roboteq.setPositionRelative(+metersToEncoderUnits(nudgeAmount));
  res.sendStatus(200);
})

app.get('/roboteq/nudge/down', function(req, res) {
  logger.verbose('/roboteq/nudge/down'); 
  roboteq.setSpeed(slows);
  roboteq.setAcceleration(slowac);
  roboteq.setDeceleration(slowdc);
  roboteq.setPositionRelative(-metersToEncoderUnits(nudgeAmount));
  res.sendStatus(200);
})

app.get('/roboteq/get/latency', function(req, res) {
  var result = roboteq.getLatency();
  res.json({'ms': result});
})

app.get('/roboteq/get/speed', function(req, res) {
  var result = roboteq.getSpeed();
  res.json({'speed': result});
})

app.get('/roboteq/get/position', function(req, res) {
  var result = roboteq.getPosition();
  res.json({'position': result, 'meters': encoderUnitsToMeters(result)});
})

app.get('/roboteq/get/motor/volts', function(req, res) {
  var result = roboteq.getMotorVolts();
  res.json({'volts': result});
})

app.get('/roboteq/get/motor/amps', function(req, res) {
  var result = roboteq.getMotorAmps();
  res.json({'amps': result});
})

app.get('/roboteq/get/battery/amps', function(req, res) {
  var result = roboteq.getBatteryAmps();
  res.json({'amps': result});
})

app.get('/roboteq/get/destinationReached', function(req, res) {
  var result = roboteq.getDestinationReached();
  res.json({'destinationReached': result});
})

app.get('/roboteq/set/echo', function(req, res) {
  var enable = (req.query.echo === 'true');
  roboteq.setEcho(enable);
  res.sendStatus(200);
})

app.get('/transitions/safe', function(req, res) {
  var safeTransitions = getSafeTransitions();
  res.json(safeTransitions);
})

app.get('/transitions/all', function(req, res) {
  res.json(Object.keys(transitions));
})

function sendOsc(address, args) {
  udpPort.send({
    address: address,
    args: args
  }, 'localhost', 9000);
}

app.get('/oculus/lookAngle/add', function (req, res) {
  var angle = Number(req.query.value);
  logger.verbose('/oculus/lookAngle/add', {angle: angle})
  sendOsc('/lookAngle/add', [angle]);
  res.sendStatus(200);
})

app.get('/oculus/screenshot', function (req, res) {
  logger.verbose('/screenshot');
  sendOsc('/screenshot');
  res.sendStatus(200);
})

app.get('/logs', function (req, res) {
  logger.query({limit: req.query.limit || 100}, function (err, results) {
    if (err) console.log(err);
    var all = _.where(results.file, {'message': req.query.message});
    res.set('Content-type', 'text/csv');
    res.send('time\t' + req.query.field + '\n' + all.map(function (log) {
      return [log.timestamp, log[req.query.field]].join('\t');
    }).join('\n'));
  });
})