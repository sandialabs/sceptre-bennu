#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_SERVERCOMMANDHANDLER_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_SERVERCOMMANDHANDLER_HPP

#include <cstdint>
#include <memory>

#include "Server.hpp"

#include "opendnp3/app/AnalogOutput.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"
#include "opendnp3/outstation/ICommandHandler.h"
#include "opendnp3/outstation/IOutstation.h"
#include "opendnp3/outstation/IUpdateHandler.h"

namespace bennu {
namespace comms {
namespace dnp3 {

using namespace opendnp3;

class ServerCommandHandler : public opendnp3::ICommandHandler
{
public:
    ServerCommandHandler(CommandStatus status);
    CommandStatus Select(const ControlRelayOutputBlock& arCommand, std::uint16_t aIndex) override final;
    CommandStatus Operate(const ControlRelayOutputBlock& arCommand, std::uint16_t aIndex, IUpdateHandler& handler, OperateType opType) override final;
    CommandStatus Select(const AnalogOutputInt16& arCommand, std::uint16_t aIndex) override final {return mStatus;}
    CommandStatus Operate(const AnalogOutputInt16& arCommand, std::uint16_t aIndex, IUpdateHandler& handler, OperateType opType) override final {return mStatus;}
    CommandStatus Select(const AnalogOutputInt32& arCommand, std::uint16_t aIndex) override final {return mStatus;}
    CommandStatus Operate(const AnalogOutputInt32& arCommand, std::uint16_t aIndex, IUpdateHandler& handler, OperateType opType) override final {return mStatus;}
    CommandStatus Select(const AnalogOutputFloat32& arCommand, std::uint16_t aIndex) override final;
    CommandStatus Operate(const AnalogOutputFloat32& arCommand, std::uint16_t aIndex, IUpdateHandler& handler, OperateType opType) override final;
    CommandStatus Select(const AnalogOutputDouble64& arCommand, std::uint16_t aIndex) override final {return mStatus;}
    CommandStatus Operate(const AnalogOutputDouble64& arCommand, std::uint16_t aIndex, IUpdateHandler& handler, OperateType opType) override final {return mStatus;}

    void setOutstation(std::shared_ptr<opendnp3::IOutstation> outstation)
    {
        pOutstation = outstation;
    }

    void setRtu(std::weak_ptr<Server> rtu)
    {
        pRtu = rtu;
    }

protected:
    CommandStatus mStatus;
    virtual void DoSelect(const ControlRelayOutputBlock& command, uint16_t index) {}
    virtual void DoOperate(const ControlRelayOutputBlock& command, uint16_t index, OperateType opType) {}
    virtual void DoSelect(const AnalogOutputInt16& command, uint16_t index) {}
    virtual void DoOperate(const AnalogOutputInt16& command, uint16_t index, OperateType opType) {}
    virtual void DoSelect(const AnalogOutputInt32& command, uint16_t index) {}
    virtual void DoOperate(const AnalogOutputInt32& command, uint16_t index, OperateType opType) {}
    virtual void DoSelect(const AnalogOutputFloat32& command, uint16_t index) {}
    virtual void DoOperate(const AnalogOutputFloat32& command, uint16_t index, OperateType opType) {}
    virtual void DoSelect(const AnalogOutputDouble64& command, uint16_t index) {}
    virtual void DoOperate(const AnalogOutputDouble64& command, uint16_t index, OperateType opType) {}
    virtual void Begin() override {}
    virtual void End() override {}

private:
    std::weak_ptr<Server> pRtu;
    std::shared_ptr<opendnp3::IOutstation> pOutstation;
};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_SERVERCOMMANDHANDLER_HPP
