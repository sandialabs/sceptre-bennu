#ifndef __MODBUS_CONSTANTS_HPP__
#define __MODBUS_CONSTANTS_HPP__

namespace bennu {
namespace comms {
namespace modbus {

    //Minimum and maximum data unit sizes.
    const uint8_t    MB_MIN_REQUEST_PDU_LENGTH           =   5;      //Supported requests have a function code (1 byte) and 4 bytes of arguments.
    const uint8_t    MB_MIN_REQUEST_ADU_LENGTH           =   12;     //MBAP header (7 bytes) plus minimum supported request PDU length (5 bytes).
    const uint8_t    MB_MAX_PDU_LENGTH                   =   253;    //Limited by RS485 ADU max of 256 bytes minus 3 bytes ModBus overhead.
    const uint16_t   MB_MAX_ADU_LENGTH                   =   260;    //MBAP header (7 bytes) plus Modbus PDU (253 bytes maximum).

    //MBAP header field offsets
    const uint8_t    MBAP_HDR_TRANSACTION_ID             =   0;
    const uint8_t    MBAP_HDR_PROTOCOL_ID                =   2;
    const uint8_t    MBAP_HDR_LENGTH                     =   4;
    const uint8_t    MABAP_HDR_UNIT_ID                   =   6;

    //Field offsets (minus MBAP header)
    const uint8_t    MB_FUNC_CODE_OFFSET                 =   0;
    const uint8_t    MB_START_ADDR_PDU_OFFSET            =   1;
    const uint8_t    MB_QTY_PDU_OFFSET                   =   3;

    //Minimum and maximum read request quantities.
    const uint16_t   MB_MIN_READ_QTY_COILS               =   0x0001; //At least 1 coil must be requested.
    const uint16_t   MB_MAX_READ_QTY_COILS               =   0x07D0; //A maximum of 2000 coils can be requested at a time.
    const uint16_t   MB_MIN_READ_QTY_DISCRETES           =   0x0001; //At least 1 discrete input must be requested.
    const uint16_t   MB_MAX_READ_QTY_DISCRETES           =   0x07D0; //A maximum of 2000 discrete inputs can be requested at a time.
    const uint16_t   MB_MIN_READ_QTY_HOLDING_REGS        =   0x0001; //At least 1 holding register must be requested.
    const uint16_t   MB_MAX_READ_QTY_HOLDING_REGS        =   0x007D; //A maximum of 125 holding registers can be requested at a time.
    const uint16_t   MB_MIN_READ_QTY_INPUT_REGS          =   0x0001; //At least 1 input register must be requested.
    const uint16_t   MB_MAX_READ_QTY_INPUT_REGS          =   0x007D; //A maximum of 125 input registers can be requested at a time.

    //Minimum and maximum multiple write request quantities.
    const uint16_t   MB_MIN_MULT_WRITE_QTY_COILS         =   0x0001; //At least 1 coil must be packed in a multi-write request.
    const uint16_t   MB_MAX_MULT_WRITE_QTY_COILS         =   0x07B0; //A maximum of 1968 coils can be packed in a multi-write request.
    const uint16_t   MB_MIN_MULT_WRITE_QTY_HOLDING_REGS  =   0x0001; //At least 1 holding register must be packed in a multi-write request.
    const uint16_t   MB_MAX_MULT_WRITE_QTY_HOLDING_REGS  =   0x007B; //A maximum of 123 holding registers can be packed in a multi-write request.

    //Minimum and maximum stored coil values.
    const uint16_t   MB_MIN_COIL_VALUE                   =   0x0000; //This value indicates that the coil is OFF.
    const uint16_t   MB_MAX_COIL_VALUE                   =   0xFF00; //This value indicates that the coil is ON.

    //Minimum and maximum address values supported by Modbus.
    const uint16_t   MB_MIN_ADDRESS                      =   0x0000;
    const uint16_t   MB_MAX_ADDRESS                      =   0xFFFF;

    //Protocol-defined exception codes.
    const uint8_t    MB_ILLEGAL_FUNCTION                 =   0x01;
    const uint8_t    MB_ILLEGAL_DATA_ADDRESS             =   0x02;
    const uint8_t    MB_ILLEGAL_DATA_VALUE               =   0x03;
    const uint8_t    MB_SLAVE_DEVICE_FAILURE             =   0x04;
    const uint8_t    MB_ACKNOWLEDGE                      =   0x05;
    const uint8_t    MB_SLAVE_DEVICE_BUSY                =   0x06;
    const uint8_t    MB_MEMORY_PARITY_ERROR              =   0x08;
    const uint8_t    MB_GATEWAY_PATCH_UNAVAILABLE        =   0x0A;
    const uint8_t    MB_GATEWAY_TARGET_DEVICE_FAILED     =   0x0B;

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_CONSTANTS_HPP__ */
