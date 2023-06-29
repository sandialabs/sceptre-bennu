#include "bennu/devices/modules/comms/bacnet/module/DataHandler.hpp"
#include "bennu/devices/modules/comms/bacnet/module/Wrapper.h"

/*
 * Wrapper for Server::writeBinary() so it can be called from BACnet -> bi.c, bo.c
 */
void call_cpp_writeBinary(int instance, int address, bool status)
{
    return instanceToServerMap[instance]->writeBinary(address, status);
}

/*
 * Wrapper for Server::writeAnalog() so it can be called from BACnet -> ai.c, ao.c
 */
void call_cpp_writeAnalog(int instance, int address, float value)
{
    return instanceToServerMap[instance]->writeAnalog(address, value);
}

/*
 * C wrapper for ClientConnection::updateBinary() so it can be called from BACnet -> bacnet.c
 */
void call_cpp_updateBinary(int instance, int address, bool status)
{
    return instanceToClientConnectionMap[instance]->updateBinary(address, status);
}

/*
 * C wrapper for ClientConnection::updateAnalog() so it can be called from BACnet -> bacnet.c
 */
void call_cpp_updateAnalog(int instance, int address, float value)
{
    return instanceToClientConnectionMap[instance]->updateAnalog(address, value);
}
