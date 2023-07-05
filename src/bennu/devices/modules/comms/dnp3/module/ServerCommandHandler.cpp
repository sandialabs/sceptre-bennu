#include "ServerCommandHandler.hpp"

#include "opendnp3/app/ControlRelayOutputBlock.h"
#include "opendnp3/outstation/IUpdateHandler.h"
#include "opendnp3/outstation/UpdateBuilder.h"

namespace bennu {
namespace comms {
namespace dnp3 {

using opendnp3::CommandStatus;

ServerCommandHandler::ServerCommandHandler(CommandStatus status) :
    mStatus(status)
{
}

CommandStatus ServerCommandHandler::Select(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex)
{
    if (!pRtu.lock()->getBinaryPoint(aIndex))
    {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return CommandStatus::OUT_OF_RANGE;
    }

    return CommandStatus::SUCCESS;
}

CommandStatus ServerCommandHandler::Operate(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType)
{
    auto point = pRtu.lock()->getBinaryPoint(aIndex);

    if (!point)
    {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return CommandStatus::OUT_OF_RANGE;
    }

    if (point->sbo && opType != opendnp3::OperateType::SelectBeforeOperate)
    {
        return CommandStatus::NO_SELECT;
    }


    bool val = arCommand.opType == opendnp3::OperationType::LATCH_ON ? true : false;

    // write binary to data manager
    pRtu.lock()->writeBinary(aIndex, val);

    return mStatus;
}

CommandStatus ServerCommandHandler::Select(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex)
{
    if (!pRtu.lock()->getAnalogPoint(aIndex))
    {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return CommandStatus::OUT_OF_RANGE;
    }

    return CommandStatus::SUCCESS;
}

CommandStatus ServerCommandHandler::Operate(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType)
{
    auto point = pRtu.lock()->getAnalogPoint(aIndex);

    if (!point)
    {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return CommandStatus::OUT_OF_RANGE;
    }

    if (point->sbo && opType != opendnp3::OperateType::SelectBeforeOperate)
    {
        return CommandStatus::NO_SELECT;
    }


    // write analog to data manager
    pRtu.lock()->writeAnalog(aIndex, arCommand.value);

    return mStatus;
}

} // namespace dnp3
} // namespace comms
} // namespace bennu
