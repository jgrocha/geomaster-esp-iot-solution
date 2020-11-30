const SerialPort = require('serialport')
const parsers = SerialPort.parsers
// Use a `\r\n` as a line terminator
const parser = new parsers.Readline({ delimiter: '\n', })
const port = new SerialPort('/dev/ttyUSB0', { baudRate: 230400 })
port.pipe(parser)
// port.on('open', function () { console.log('Port open'); });

// parser.on('data', console.log);

parser.on('data', function (data) {
    console.log('Data:', data);
    enviaParaMosquitto(data);
});

// o interval tem que ser 5 ou superior... usar 10 para ter alguma margem :-)
// o número máximo de carateres é 8. Com 9 ou 10 já estoira.
// setInterval(function () { port.write('ABCDEF\r\n') }, 1000);

var enviaParaRoqDry = function (mensagem) {
    var parts = mensagem.match(/.{1,3}/g);
    parts.forEach(function (element) {
        console.log(element);
        port.write(element);
    });
    port.write('\n');
};

var enviaParaMosquitto = function (mensagem) {
    // 'bicafe/tmp/1 23.4'.split(/\s/)
    var [topico,payload] = mensagem.split(/\s/);
    client.publish(topico, payload)
};

var mqtt = require('mqtt')
var client = mqtt.connect('mqtt://labs.activeng.pt:1883', { clientId: 'Raspberry', username: 'activeng', password: 'costanova20181215' })

client.on('connect', function () {
    client.subscribe('cafeina/#', function (err) {
        if (!err) {
            client.publish('geomaster/raspberry', 'Hello mqtt')
            // mosquitto_sub -h labs.activeng.pt -p 1883 -t geomaster/raspberry -u activeng -P costanova20181215
        }
    })
})

client.on('message', function (topic, message) {
    var carta = topic + ' ' + message.toString();
    console.log(carta)
    enviaParaRoqDry(carta);
})