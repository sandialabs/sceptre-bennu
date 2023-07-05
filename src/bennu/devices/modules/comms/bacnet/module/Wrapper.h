#ifndef BENNU_FIELDDEVICE_COMMS_BACNET_WRAPPER_H
#define BENNU_FIELDDEVICE_COMMS_BACNET_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/*
 * Wrapper for Server::writeBinary() so it can be called from BACnet -> bi.c, bo.c
 */
void call_cpp_writeBinary(int instance, int address, bool status);

/*
 * C wrapper for Server::writeAnalog() so it can be called from BACnet -> ai.c, ao.c
 */
void call_cpp_writeAnalog(int instance, int address, float value);

/*
 * C wrapper for Client::updateBinary() so it can be called from BACnet -> bacnet.c
 */
void call_cpp_updateBinary(int instance, int address, bool status);

/*
 * C wrapper for Client::updateAnalog() so it can be called from BACnet -> bacnet.c
 */
void call_cpp_updateAnalog(int instance, int address, float value);

#ifdef __cplusplus
}
#endif

#endif // BENNU_FIELDDEVICE_COMMS_BACNET_WRAPPER_H
