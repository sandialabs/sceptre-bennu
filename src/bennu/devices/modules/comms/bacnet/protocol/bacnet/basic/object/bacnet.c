#include "bacnet.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/datalink/dlenv.h"

#include "bacnet/basic/tsm/tsm.h"

/* global variables used in this file */
static uint32_t Target_Device_Object_Instance = 4194303;
static unsigned Target_Max_APDU = 0;
static bool Error_Detected = false;
static BACNET_ADDRESS Target_Address;
static uint8_t Request_Invoke_ID = 0;
static bool isReadPropertyHandlerRegistered = false;
static bool isWritePropertyHandlerRegistered = false;

static void __LogAnswer(const char *msg, unsigned append)
{
    printf("\n%s\n", msg);
}

/****************************************/
/* Logging Support */
/****************************************/
#define MAX_ERROR_STRING 128
#define NO_ERROR "No Error"
static char Last_Error[MAX_ERROR_STRING] = NO_ERROR;
static void LogError(const char *msg)
{
    strcpy(Last_Error, msg);
    Error_Detected = true;
}

/**************************************/
/* error handlers */
/*************************************/
static void MyAbortHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server)
{
    (void)server;
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID))
    {
        char msg[MAX_ERROR_STRING];
        sprintf(msg, "BACnet Abort: %s",
                bactext_abort_reason_name((int)abort_reason));
        LogError(msg);
    }
}

static void MyRejectHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID))
    {
        char msg[MAX_ERROR_STRING];
        sprintf(msg, "BACnet Reject: %s",
                bactext_reject_reason_name((int)reject_reason));
        LogError(msg);
    }
}

static void My_Error_Handler(BACNET_ADDRESS *src,
                             uint8_t invoke_id,
                             BACNET_ERROR_CLASS error_class,
                             BACNET_ERROR_CODE error_code)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID))
    {
        char msg[MAX_ERROR_STRING];
        sprintf(msg, "BACnet Error: %s: %s",
                bactext_error_class_name((int)error_class),
                bactext_error_code_name((int)error_code));
        LogError(msg);
    }
}

/**********************************/
/*        ACK handlers            */
/**********************************/

/*****************************************/
/* Decode the ReadProperty Ack and pass to perl */
/****************************************/
#define MAX_ACK_STRING 512
void rp_ack_extract_data(BACNET_READ_PROPERTY_DATA *data)
{
    char ackString[MAX_ACK_STRING] = "";
    char *pAckString = &ackString[0];
    BACNET_OBJECT_PROPERTY_VALUE object_value; /* for bacapp printing */
    BACNET_APPLICATION_DATA_VALUE value;       /* for decode value data */
    int len = 0;
    uint8_t *application_data;
    int application_data_len;
    bool first_value = true;
    bool print_brace = true;
    bool DEBUG = false; //TODO loop this into debug printing

    if (data)
    {
        application_data = data->application_data;
        application_data_len = data->application_data_len;
        /* FIXME: what if application_data_len is bigger than 255? */
        /* value? need to loop until all of the len is gone... */
        if (DEBUG)
        {
            for (;;)
            {
                len = bacapp_decode_application_data(
                    application_data, (uint8_t)application_data_len, &value);
                if (first_value && (len < application_data_len))
                {
                    first_value = false;
                    strncat(pAckString, "{", 1);
                    pAckString += 1;
                    print_brace = true;
                }
                object_value.object_type = data->object_type;
                object_value.object_instance = data->object_instance;
                object_value.object_property = data->object_property;
                object_value.array_index = data->array_index;
                object_value.value = &value;
                bacapp_snprintf_value(pAckString,
                                      MAX_ACK_STRING - (pAckString - ackString), &object_value);
                if (len > 0)
                {
                    if (len < application_data_len)
                    {
                        application_data += len;
                        application_data_len -= len;
                        /* there's more! */
                        strncat(pAckString, ",", 1);
                        pAckString += 1;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            len = bacapp_decode_application_data(
                data->application_data, (uint8_t)data->application_data_len, &value);
        }
        if (data->object_type == OBJECT_BINARY_INPUT || data->object_type == OBJECT_BINARY_OUTPUT)
        {
            call_cpp_updateBinary(Target_Device_Object_Instance,
                                  data->object_instance,
                                  value.type.Boolean);
        }
        else if (data->object_type == OBJECT_ANALOG_INPUT || data->object_type == OBJECT_ANALOG_OUTPUT)
        {
            call_cpp_updateAnalog(Target_Device_Object_Instance,
                                  data->object_instance,
                                  value.type.Real);
        }
        if (DEBUG)
        {
            if (print_brace)
            {
                strncat(pAckString, "}", 1);
                pAckString += 1;
            }
            /* Now let's call a Perl function to display the data */
            __LogAnswer(ackString, 0);
        }
    }
}

/** Handler for a ReadProperty ACK.
 * @ingroup DSRP
 * Doesn't actually do anything, except, for debugging, to
 * print out the ACK data of a matching request.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */
static void My_Read_Property_Ack_Handler(uint8_t *service_request,
                                         uint16_t service_len,
                                         BACNET_ADDRESS *src,
                                         BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data)
{
    int len = 0;
    BACNET_READ_PROPERTY_DATA data;

    if (address_match(&Target_Address, src) &&
        (service_data->invoke_id == Request_Invoke_ID))
    {
        len = rp_ack_decode_service_request(service_request, service_len, &data);
        if (len > 0)
        {
            rp_ack_extract_data(&data);
        }
    }
}

void My_Write_Property_SimpleAck_Handler(BACNET_ADDRESS *src, uint8_t invoke_id)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID))
    {
        __LogAnswer("WriteProperty Acknowledged!", 0);
    }
}

static void Init_Service_Handlers()
{
    Device_Init(NULL);
    /* we need to handle who-is to support dynamic device binding to us */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);
    /* set the handler for all the services we don't implement
       It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    /* we must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    /* handle generic errors coming back */
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);
}

typedef enum
{
    waitAnswer,
    waitBind,
} waitAction;

static void Wait_For_Answer_Or_Timeout(unsigned timeout_ms, waitAction action)
{
    /* Wait for timeout, failure, or success */
    time_t last_seconds = time(NULL);
    time_t timeout_seconds = (apdu_timeout() / 1000) * apdu_retries();
    time_t elapsed_seconds = 0;
    uint16_t pdu_len = 0;
    BACNET_ADDRESS src = {0}; /* address where message came from */
    uint8_t Rx_Buf[MAX_MPDU] = {0};

    while (true)
    {
        time_t current_seconds = time(NULL);

        /* If error was detected then bail out */
        if (Error_Detected)
        {
            //            LogError("Some other error occurred");
            break;
        }

        if (elapsed_seconds > timeout_seconds)
        {
            LogError("APDU Timeout");
            break;
        }

        /* Process PDU if one comes in */
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout_ms);
        if (pdu_len)
        {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }

        /* at least one second has passed */
        if (current_seconds != last_seconds)
        {
            tsm_timer_milliseconds(((current_seconds - last_seconds) * 1000));
        }

        if (action == waitAnswer)
        {
            /* Response was received. Exit. */
            if (tsm_invoke_id_free(Request_Invoke_ID))
            {
                break;
            }
            else if (tsm_invoke_id_failed(Request_Invoke_ID))
            {
                LogError("TSM Timeout!");
                tsm_free_invoke_id(Request_Invoke_ID);
                break;
            }
        }
        else if (action == waitBind)
        {
            if (address_bind_request(Target_Device_Object_Instance,
                                     &Target_Max_APDU, &Target_Address))
            {
                break;
            }
        }
        else
        {
            LogError("Invalid waitAction requested");
            break;
        }

        /* Keep track of time */
        elapsed_seconds += (current_seconds - last_seconds);
        last_seconds = current_seconds;
    }
}

/*
 * Write address_cache file to disk (cache file gets read inside "address_init()")
 *
 * File format:
 * DeviceID MAC SNET SADR MAX-APDU
 * 2222 ac:10:d2:98:ba:c0 0 0 1476
 *
 * The example above corresponds to:
 *      Target Instance: 2222
 *      Target IP/Port: 172.16.210.152:47808
*/
static void set_file_address(const char *pFilename,
                             uint32_t device_id,
                             BACNET_ADDRESS *dest,
                             uint16_t max_apdu)
{
    unsigned i;
    FILE *pFile = NULL;

    pFile = fopen(pFilename, "a");

    if (pFile)
    {
        fprintf(pFile, "%lu ", (long unsigned int)device_id);
        for (i = 0; i < dest->mac_len; i++)
        {
            fprintf(pFile, "%02x", dest->mac[i]);
            if ((i + 1) < dest->mac_len)
            {
                fprintf(pFile, ":");
            }
        }
        fprintf(pFile, " %hu ", dest->net);
        if (dest->net)
        {
            for (i = 0; i < dest->len; i++)
            {
                fprintf(pFile, "%02x", dest->adr[i]);
                if ((i + 1) < dest->len)
                {
                    fprintf(pFile, ":");
                }
            }
        }
        else
        {
            fprintf(pFile, "0");
        }
        fprintf(pFile, " %hu\n", max_apdu);
        fclose(pFile);
    }
}

/****************************************************/
/*             Interface API                        */
/****************************************************/

/****************************************************/
/* This is the most fundamental setup needed to start communication  */
/****************************************************/
void BacnetPrepareComm(int instance)
{
    if (instance < 0)
    {
        instance = BACNET_MAX_INSTANCE;
    }
    /* setup my info */
    Device_Set_Object_Instance_Number(instance);
    address_init();
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);
}

void BacnetPrepareClientComm(int instance, int rtuInstance, char *addr)
{
    if (instance < 0)
    {
        instance = BACNET_MAX_INSTANCE;
    }
    /* setup my info */
    Device_Set_Object_Instance_Number(instance);
    /* add address to cache                    */
    /*  - This provides support for            */
    /*    connections to remote BACnet servers */
    BACNET_ADDRESS src = {0};
    BACNET_MAC_ADDRESS mac = {0};
    int index = 0;
    address_mac_from_ascii(&mac, addr);
    memcpy(&src.mac[0], &mac.adr[0], mac.len);
    src.mac_len = mac.len;
    src.len = 0;
    src.net = 0;
    set_file_address("address_cache", rtuInstance, &src, MAX_APDU);
    /* finish initialization */
    address_init();
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);
}

void BacnetServerInit(void)
{
    /* Set the handlers for any confirmed services that we support. */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE, handler_write_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_RANGE, handler_read_range);
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL, handler_device_communication_control);
    /* Hello World! */
    Send_I_Am(&Handler_Transmit_Buffer[0]);
}

void BacnetServerTask(void)
{
    BACNET_ADDRESS src; /* source address */
    uint16_t pdu_len = 0;
    unsigned timeout = 1; /* milliseconds */
    uint8_t Rx_Buf[MAX_MPDU] = {0};
    /* handle the messaging -- returns 0 on timeout */
    pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
    if (pdu_len)
    {
        npdu_handler(&src, &Rx_Buf[0], pdu_len);
    }
    //TODO
    //    handler_cov_task();
}

/****************************************************/
/* Try to bind to a device. If successful, return zero. If failure, return */
/* non-zero and log the error details */
/****************************************************/
int BacnetBindToDevice(int deviceInstanceNumber)
{
    int isFailure = 0;

    /* Store the requested device instance number in the global variable for */
    /* reference in other communication routines */
    Target_Device_Object_Instance = deviceInstanceNumber;

    /* try to bind with the device */
    bool bound = address_bind_request(deviceInstanceNumber, &Target_Max_APDU, &Target_Address);
    if (bound)
    {
        printf("Found address in cache: %u.%u.%u.%u\n",
               Target_Address.mac[0], Target_Address.mac[1], Target_Address.mac[2], Target_Address.mac[3]);
    }
    else
    {
        printf("Did not find address in cache...sending WhoIs for device: %u\n", deviceInstanceNumber);
        Send_WhoIs(
            Target_Device_Object_Instance, Target_Device_Object_Instance);

        /* Wait for timeout, failure, or success */
        Wait_For_Answer_Or_Timeout(100, waitBind);
    }
    /* Clean up after ourselves */
    isFailure = Error_Detected;
    if (isFailure)
    {
        printf("\nFailed binding to BACnet device: %u -- %s\n", deviceInstanceNumber, Last_Error);
    }
    Error_Detected = false;
    return isFailure;
}

/****************************************************/
/* This is the interface to ReadProperty */
/****************************************************/
struct StatusMessage BacnetReadProperty(int deviceInstanceNumber,
                                        int objectType,
                                        int objectInstanceNumber,
                                        int objectProperty,
                                        int objectIndex)
{
    if (!isReadPropertyHandlerRegistered)
    {
        /* handle the data coming back from confirmed requests */
        apdu_set_confirmed_ack_handler(
            SERVICE_CONFIRMED_READ_PROPERTY, My_Read_Property_Ack_Handler);

        /* handle any errors coming back */
        apdu_set_error_handler(
            SERVICE_CONFIRMED_READ_PROPERTY, My_Error_Handler);

        /* indicate that handlers are now registered */
        isReadPropertyHandlerRegistered = true;
    }
    /* Send the message out */
    Request_Invoke_ID = Send_Read_Property_Request(deviceInstanceNumber,
                                                   objectType, objectInstanceNumber, objectProperty, objectIndex);
    Wait_For_Answer_Or_Timeout(100, waitAnswer);

    int isFailure = Error_Detected;
    Error_Detected = 0;
    struct StatusMessage status;
    status.status = !isFailure;
    status.message = Last_Error;
    return status;
}

/****************************************************/
/* This is the interface to WriteProperty */
/****************************************************/
struct StatusMessage BacnetWriteProperty(int deviceInstanceNumber,
                                         int objectType,
                                         int objectInstanceNumber,
                                         int objectProperty,
                                         int objectPriority,
                                         int objectIndex,
                                         const char *tag,
                                         const char *value)
{
    char msg[MAX_ERROR_STRING];
    int isFailure = 1;

    if (!isWritePropertyHandlerRegistered)
    {
        /* handle the ack coming back */
        apdu_set_confirmed_simple_ack_handler(SERVICE_CONFIRMED_WRITE_PROPERTY,
                                              My_Write_Property_SimpleAck_Handler);

        /* handle any errors coming back */
        apdu_set_error_handler(
            SERVICE_CONFIRMED_WRITE_PROPERTY, My_Error_Handler);

        /* indicate that handlers are now registered */
        isWritePropertyHandlerRegistered = true;
    }

    if (objectIndex == -1)
    {
        objectIndex = BACNET_ARRAY_ALL;
    }
    /* Loop for eary exit; */
    do
    {
        /* Handle the tag/value pair */
        uint8_t context_tag = 0;
        BACNET_APPLICATION_TAG property_tag;
        BACNET_APPLICATION_DATA_VALUE propertyValue;

        if (toupper(tag[0]) == 'C')
        {
            context_tag = strtol(&tag[1], NULL, 0);
            propertyValue.context_tag = context_tag;
            propertyValue.context_specific = true;
        }
        else
        {
            propertyValue.context_specific = false;
        }
        property_tag = strtol(tag, NULL, 0);

        if (property_tag >= MAX_BACNET_APPLICATION_TAG)
        {
            sprintf(msg, "Error: tag=%u - it must be less than %u",
                    property_tag, MAX_BACNET_APPLICATION_TAG);
            LogError(msg);
            break;
        }
        if (!bacapp_parse_application_data(
                property_tag, value, &propertyValue))
        {
            sprintf(msg, "Error: unable to parse the tag value");
            LogError(msg);
            break;
        }
        propertyValue.next = NULL;

        /* Send out the message */
        Request_Invoke_ID = Send_Write_Property_Request(deviceInstanceNumber,
                                                        objectType, objectInstanceNumber, objectProperty, &propertyValue,
                                                        objectPriority, objectIndex);
        Wait_For_Answer_Or_Timeout(100, waitAnswer);

        /* If we get here, then there were no explicit failures. However, there
         */
        /* could have been implicit failures. Let's look at those also. */
        isFailure = Error_Detected;
    } while (false);

    /* Clean up after ourselves. */
    Error_Detected = false;
    struct StatusMessage status;
    status.status = !isFailure;
    status.message = Last_Error;
    return status;
}
