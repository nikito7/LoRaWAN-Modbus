function decodeUplink(input) {
    var data = {};
    var warnings = [];

    if (input.fPort == 70) {
        data.HH = input.bytes[0];
        data.MM = input.bytes[1];
        data.SS = input.bytes[2];
        data.AAc = ("0" + data.HH).slice(-2)
                 + ":" + ("0" + data.MM).slice(-2)
                 + ":" + ("0" + data.SS).slice(-2);
        data.VL1 = ((input.bytes[3] << 8)
                 + input.bytes[4]) / 10.0;
        data.CL1 = ((input.bytes[5] << 8)
                 + input.bytes[6]) / 10.0;
        data.VL2 = ((input.bytes[7] << 8)
                 + input.bytes[8]) / 10.0;
        data.CL2 = ((input.bytes[9] << 8)
                 + input.bytes[10]) / 10.0;
        data.VL3 = ((input.bytes[11] << 8)
                 + input.bytes[12]) / 10.0;
        data.CL3 = ((input.bytes[13] << 8)
                 + input.bytes[14]) / 10.0;
        data.CLT = ((input.bytes[15] << 8)
                 + input.bytes[16]) / 10.0;
        // 
        data.Freq = ((input.bytes[17] << 8)
                 + input.bytes[18]) / 10.0;
        //
        data.PF = ((input.bytes[19] << 8)
                 + input.bytes[20]) / 1000.0;
        data.PF1 = ((input.bytes[21] << 8)
                 + input.bytes[22]) / 1000.0;
        data.PF2 = ((input.bytes[23] << 8)
                 + input.bytes[24]) / 1000.0;
        data.PF3 = ((input.bytes[25] << 8)
                 + input.bytes[26]) / 1000.0;
    }
    else if (input.fPort == 71) {
        data.HH = input.bytes[0];
        data.MM = input.bytes[1];
        data.SS = input.bytes[2];
        data.AAc = ("0" + data.HH).slice(-2)
                 + ":" + ("0" + data.MM).slice(-2)
                 + ":" + ("0" + data.SS).slice(-2);
        data.API = ((input.bytes[3] << 24)
                 + (input.bytes[4] << 16)
                 + (input.bytes[5] << 8)
                 + input.bytes[6]);
        data.APE = ((input.bytes[7] << 24)
                 + (input.bytes[8] << 16)
                 + (input.bytes[9] << 8)
                 + input.bytes[10]);
        // 
        data.API1 = (input.bytes[11] << 8)
                  + input.bytes[12];
        data.APE1 = (input.bytes[13] << 8)
                  + input.bytes[14];
        data.API2 = (input.bytes[15] << 8)
                  + input.bytes[16];
        data.APE2 = (input.bytes[17] << 8)
                  + input.bytes[18];
        data.API3 = (input.bytes[19] << 8)
                  + input.bytes[20];
        data.APE3 = (input.bytes[21] << 8)
                  + input.bytes[22];
    }
    else if (input.fPort == 72) {
        data.HH = input.bytes[0];
        data.MM = input.bytes[1];
        data.SS = input.bytes[2];
        data.AAc = ("0" + data.HH).slice(-2)
                 + ":" + ("0" + data.MM).slice(-2)
                 + ":" + ("0" + data.SS).slice(-2);
        data.TET1 = ((input.bytes[3] << 24)
                 + (input.bytes[4] << 16)
                 + (input.bytes[5] << 8)
                 + input.bytes[6]) / 1000.0;
        data.TET2 = ((input.bytes[7] << 24)
                 + (input.bytes[8] << 16)
                 + (input.bytes[9] << 8)
                 + input.bytes[10]) / 1000.0;
        data.TET3 = ((input.bytes[11] << 24)
                 + (input.bytes[12] << 16)
                 + (input.bytes[13] << 8)
                 + input.bytes[14]) / 1000.0;
        data.TEI = ((input.bytes[15] << 24)
                 + (input.bytes[16] << 16)
                 + (input.bytes[17] << 8)
                 + input.bytes[18]) / 1000.0;
        data.TEE = ((input.bytes[19] << 24)
                 + (input.bytes[20] << 16)
                 + (input.bytes[21] << 8)
                 + input.bytes[22]) / 1000.0;
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
