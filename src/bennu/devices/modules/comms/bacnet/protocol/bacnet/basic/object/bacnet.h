/**************************************************************************
*
* Copyright (C) 2010 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/
#ifndef BACNET_H
#define BACNET_H

#include <stdint.h>

#include "../module/Wrapper.h" /* Wrapper API for C++ */
#include "../../base/StatusMessage.h" /* For StatusMessage */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /****************************************************/
    /* This is the most fundamental setup needed to start communication  */
    /****************************************************/
    void BacnetPrepareComm(int instance);
    /****************************************************/
    /* This is the most fundamental setup needed to start communication.  */
    /* Used for clients; Adds the target endpoint address to the cache */
    /****************************************************/
    void BacnetPrepareClientComm(int instance, int rtuInstance, char* addr);
    /****************************************************/
    /* Try to bind to a device. If successful, return zero. If failure, return */
    /* non-zero and log the error details */
    /****************************************************/
    int BacnetBindToDevice(int deviceInstanceNumber);
    /****************************************************/
    /* This is the interface to ReadProperty */
    /****************************************************/
    struct StatusMessage
            BacnetReadProperty(int deviceInstanceNumber,
                               int objectType,
                               int objectInstanceNumber,
                               int objectProperty,
                               int objectIndex);
    /****************************************************/
    /* This is the interface to WriteProperty */
    /****************************************************/
    struct StatusMessage
            BacnetWriteProperty(int deviceInstanceNumber,
                                int objectType,
                                int objectInstanceNumber,
                                int objectProperty,
                                int objectPriority,
                                int objectIndex,
                                const char *tag,
                                const char *value);
    /****************************************************/
    /* Server initialization and listening task */
    /****************************************************/
    void BacnetServerInit(void);
    void BacnetServerTask(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
