#ifndef BENNU_FIELDDEVICE_COMMS_COMMON_HPP
#define BENNU_FIELDDEVICE_COMMS_COMMON_HPP

#include <cstdint>
#include <string>

#include "StatusMessage.h" // Not used in this file directly; for StatusMessage struct usage

namespace bennu {
namespace comms {

struct LogMessage
{
    std::string mEvent;
    std::string mLevel;
    std::string mMessage;
};

enum RegisterType
{
    eNone,
    eStatusReadOnly,
    eStatusReadWrite,
    eValueReadOnly,
    eValueReadWrite,
    eIntReadOnly,
    eIntReadWrite
};

struct RegisterDescriptor
{
    RegisterDescriptor() :
        mRegisterType(eNone),
        mRegisterAddress(0),
        mTag(""),
        mStatus(false),
        mFloatValue(0.0f),
        mIntValue(0)
    {
    }

    RegisterType mRegisterType;
    uint16_t mRegisterAddress;
    std::string mTag;
    bool mStatus;
    float mFloatValue;
    int mIntValue;
};

struct RegDescComp
{
    bool operator() (const RegisterDescriptor& lhs, const RegisterDescriptor& rhs) const
    {
        return lhs.mRegisterAddress < rhs.mRegisterAddress;
    }
};

} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_COMMON_HPP
