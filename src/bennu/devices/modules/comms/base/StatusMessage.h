#ifndef BENNU_FIELDDEVICE_COMMS_STATUSMESSAGE_HPP
#define BENNU_FIELDDEVICE_COMMS_STATUSMESSAGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {STATUS_FAIL, STATUS_SUCCESS} STATUS;

/*
 * Simple struct that holds a success/fail status variable and
 * char* status message. This was originally created to be used with
 * the BACnet protocol which is C, so that's why it's on it's own
 * in a .h file. It is set as the return value for read/write tags
 * in the comms protocols. Since they inherit from a common parent
 * (CommsClient), it was necessary for all of them to use this struct
 * as the return value, even though it's primarily used by BACnet.
 * It's usage in BACnet is so useful error messages can be sent back to
 * a Client in the event of an error when reading/writing a tag in the
 * protocol.
 *
 * When using this in C++, if you want to append the char* message to a
 * std::string, create a string copy of the message and use that:
 *      std::string msgCopy(message);
 */
struct StatusMessage
{
    STATUS status;
    char* message;
};

/*
 * Provides an initial value when declaring a StatusMessage
 *      eg. StatusMessage sm = STATUS_INIT;
 */
const struct StatusMessage STATUS_INIT = {STATUS_SUCCESS, (char*)"Success"};

#ifdef __cplusplus
}
#endif

#endif // BENNU_FIELDDEVICE_COMMS_STATUSMESSAGE_HPP
