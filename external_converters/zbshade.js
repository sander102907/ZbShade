const m = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const { postfixWithEndpointName } = require('zigbee-herdsman-converters/lib/utils');

const fzLocal = {
    cover_position_tilt: {
        cluster: "closuresWindowCovering",
        type: ["attributeReport", "readResponse"],
        options: [exposes.options.invert_cover()],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            // Zigbee officially expects 'open' to be 0 and 'closed' to be 100 whereas
            // HomeAssistant etc. work the other way round.
            // For zigbee-herdsman-converters: open = 100, close = 0
            // ubisys J1 will report 255 if lift or tilt positions are not known, so skip that.
            const metaInvert = model.meta?.coverInverted;
            const invert = metaInvert ? !options.invert_cover : options.invert_cover;
            const coverStateFromTilt = model.meta?.coverStateFromTilt;
            if (msg.data.currentPositionLiftPercentage !== undefined && msg.data.currentPositionLiftPercentage <= 100) {
                const value = msg.data.currentPositionLiftPercentage;
                result[postfixWithEndpointName("position", msg, model, meta)] = invert ? value : 100 - value;
                if (!coverStateFromTilt) {
                    result[postfixWithEndpointName("state", msg, model, meta)] = metaInvert
                        ? value === 0
                            ? "CLOSE"
                            : "OPEN"
                        : value === 100
                            ? "CLOSE"
                            : "OPEN";
                }
            }
            if (msg.data.currentPositionTiltPercentage !== undefined && msg.data.currentPositionTiltPercentage <= 100) {
                const value = msg.data.currentPositionTiltPercentage;
                result[postfixWithEndpointName("tilt", msg, model, meta)] = invert ? value : 100 - value;
                if (coverStateFromTilt) {
                    result[postfixWithEndpointName("state", msg, model, meta)] = metaInvert
                        ? value === 0
                            ? "CLOSE"
                            : "OPEN"
                        : value === 100
                            ? "CLOSE"
                            : "OPEN";
                }
            }
            if (msg.data.windowCoveringMode !== undefined) {
                result[postfixWithEndpointName("cover_mode", msg, model, meta)] = {
                    reversed: (msg.data.windowCoveringMode & (1 << 0)) > 0,
                    calibration: (msg.data.windowCoveringMode & (1 << 1)) > 0,
                    maintenance: (msg.data.windowCoveringMode & (1 << 2)) > 0,
                    led: (msg.data.windowCoveringMode & (1 << 3)) > 0,
                };
            }
            
            return result;
        },
    },
}


const windowCovering = () => {
    return {
        ...m.windowCovering({ "controls": ["tilt"], "stateSource": "tilt", "coverInverted": false }),
        fromZigbee: [fzLocal.cover_position_tilt]
    };
}


const definition = {
    zigbeeModel: ['zbShade'],
    model: 'zbShade',
    vendor: 'DIY',
    description: 'Custom Zigbee shade with tilt control',
    fromZigbee: [], // Override the fromZigbee converter
    extend: [windowCovering(), m.battery()],
    meta: {},
};

module.exports = definition;