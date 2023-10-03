function decodeUplink(input) {
    var data = {};
    var warnings = [];

    if (input.fPort == 10) {
        data.cnt = (input.bytes[0] << 8) + input.bytes[1];
        data.err = (input.bytes[2]);
        // byte[3] = 0x77
        data.volt = ((input.bytes[4] << 8) + input.bytes[5]) / 10.0;
        data.volt1 = ((input.bytes[6] << 8) + input.bytes[7]) / 10.0;
        data.cur1 = ((input.bytes[8] << 8) + input.bytes[9]) / 10.0;
    }
    else {
        warnings.push("Unsupported fPort");
    }
    return {
        data: data,
        warnings: warnings
    };
}

