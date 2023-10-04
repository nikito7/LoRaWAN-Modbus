function decodeUplink(input) {
    var data = {};
    var warnings = [];

    if (input.fPort == 10) {
        data.HH = input.bytes[0];
        data.MM = input.bytes[1];
        data.SS = input.bytes[2];
        data.Clock = ("0" + data.HH).slice(-2) + ":" + ("0" + data.MM).slice(-2) + ":" + ("0" + data.SS).slice(-2);    
        data.VL1 = ((input.bytes[3] << 8) + input.bytes[4]) / 10.0;
        data.CL1 = ((input.bytes[5] << 8) + input.bytes[6]) / 10.0;

        //data.Year = (input.bytes[0] << 8) + input.bytes[1];
        //data.Month = input.bytes[2];
        //data.Day = input.bytes[3];
        //data.WeekDay = input.bytes[4];
    }
    else {
        warnings.push("Unsupported fPort");
    }
    return {
        data: data,
        warnings: warnings
    };
}

// EOF
