const SerialPort = require('serialport')
const parsers = SerialPort.parsers
// Use a `\r\n` as a line terminator
const parser = new parsers.Readline({ delimiter: '\r\n', })
const port = new SerialPort('/dev/ttyUSB0', { baudRate: 230400, })

port.pipe(parser)

port.on('open', function () { console.log('Port open'); });

parser.on('data', console.log);

// o interval tem que ser 5 ou superior... usar 10 para ter alguma margem :-)
// o número máximo de carateres é 8. Com 9 ou 10 já estoira.
setInterval(function () { port.write('12345678') }, 10);