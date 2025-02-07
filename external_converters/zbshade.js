const { windowCovering } = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['zbShade4'],
    model: 'zbShade4',
    vendor: 'DIY',
    description: 'Automatically generated definition',
    // fromZigbee: [fz.customFromZigbee], // Override the fromZigbee converter
    extend: [windowCovering({ "controls": ["tilt"], "stateSource": "tilt", "coverInverted": true })],
    meta: {},
};

module.exports = definition;