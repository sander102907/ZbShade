const m = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['zbShade'],
    model: 'zbShade',
    vendor: 'DIY',
    description: 'Automatically generated definition',
    // fromZigbee: [fz.customFromZigbee], // Override the fromZigbee converter
    extend: [m.windowCovering({ "controls": ["tilt"], "stateSource": "tilt", "coverInverted": true }), m.battery()],
    meta: {},
};

module.exports = definition;