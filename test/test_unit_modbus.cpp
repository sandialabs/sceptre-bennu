#include "doctest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/modbus/module/Server.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/application-layer.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/error-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/function-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/protocol-stack.hpp"

using namespace bennu::comms::modbus;
using bennu::field_device::DataManager;

namespace {

inline std::vector<uint8_t> makeAdu(uint16_t tid, uint8_t uid, uint8_t fc, const std::vector<uint8_t>& pduData)
{
    std::vector<uint8_t> adu;
    adu.push_back(static_cast<uint8_t>(tid >> 8));
    adu.push_back(static_cast<uint8_t>(tid & 0xFF));
    adu.push_back(0x00);                                          // PID HI
    adu.push_back(0x00);                                          // PID LO
    uint16_t len = static_cast<uint16_t>(1 + 1 + pduData.size()); // UID + FC + data
    adu.push_back(static_cast<uint8_t>(len >> 8));
    adu.push_back(static_cast<uint8_t>(len & 0xFF));
    adu.push_back(uid);
    adu.push_back(fc);
    adu.insert(adu.end(), pduData.begin(), pduData.end());
    return adu;
}

} // namespace

TEST_CASE("Modbus Application Layer: FC01-FC04 read functions and response framing")
{
    application::callback_map_t callbacks;

    // Register handlers for reading
    boost::fusion::at_key<READ_COILS>(callbacks) =
        [](uint16_t start_addr, uint16_t quantity, std::vector<bool>& values) -> error_code_t::type {
        if (start_addr > 1000)
        {
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        for (uint16_t i = 0; i < quantity; ++i)
        {
            values.push_back(i % 2 == 0);
        }
        return error_code_t::NO_ERROR;
    };

    boost::fusion::at_key<READ_DISCRETE_INPUTS>(callbacks) =
        [](uint16_t start_addr, uint16_t quantity, std::vector<bool>& values) -> error_code_t::type {
        if (start_addr > 1000)
        {
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        for (uint16_t i = 0; i < quantity; ++i)
        {
            values.push_back(i % 2 != 0);
        }
        return error_code_t::NO_ERROR;
    };

    boost::fusion::at_key<READ_HOLDING_REGS>(callbacks) =
        [](uint16_t start_addr, uint16_t quantity, std::vector<uint16_t>& values) -> error_code_t::type {
        if (start_addr > 1000)
        {
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        for (uint16_t i = 0; i < quantity; ++i)
        {
            values.push_back(static_cast<uint16_t>(start_addr + i * 10));
        }
        return error_code_t::NO_ERROR;
    };

    boost::fusion::at_key<READ_INPUT_REGS>(callbacks) =
        [](uint16_t start_addr, uint16_t quantity, std::vector<uint16_t>& values) -> error_code_t::type {
        if (start_addr > 1000)
        {
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        for (uint16_t i = 0; i < quantity; ++i)
        {
            values.push_back(static_cast<uint16_t>(500 + i));
        }
        return error_code_t::NO_ERROR;
    };

    application::layer app(callbacks);
    std::vector<uint8_t> lastResponse;
    app.data_send_signal.connect([&](uint8_t* data, size_t size) {
        lastResponse.assign(data, data + size);
    });

    // 1. FC01: Read Coils (start: 0, qty: 8)
    {
        lastResponse.clear();
        auto adu = makeAdu(1, 1, function_code_t::READ_COILS, {0x00, 0x00, 0x00, 0x08});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == function_code_t::READ_COILS);
        CHECK(lastResponse[8] == 1);    // 1 byte to pack 8 coils
        CHECK(lastResponse[9] == 0x55); // Alternating true/false: 0b01010101 = 0x55
    }

    // 2. FC02: Read Discrete Inputs (start: 0, qty: 8)
    {
        lastResponse.clear();
        auto adu = makeAdu(2, 1, function_code_t::READ_DISCRETE_INPUTS, {0x00, 0x00, 0x00, 0x08});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == function_code_t::READ_DISCRETE_INPUTS);
        CHECK(lastResponse[8] == 1);
        CHECK(lastResponse[9] == 0xAA); // 0b10101010 = 0xAA
    }

    // 3. FC03: Read Holding Registers (start: 10, qty: 2)
    {
        lastResponse.clear();
        auto adu = makeAdu(3, 1, function_code_t::READ_HOLDING_REGS, {0x00, 0x0A, 0x00, 0x02});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 13);
        CHECK(lastResponse[7] == function_code_t::READ_HOLDING_REGS);
        CHECK(lastResponse[8] == 4); // 2 registers * 2 bytes = 4 bytes
        uint16_t reg0 = (static_cast<uint16_t>(lastResponse[9]) << 8) | lastResponse[10];
        uint16_t reg1 = (static_cast<uint16_t>(lastResponse[11]) << 8) | lastResponse[12];
        CHECK(reg0 == 10);
        CHECK(reg1 == 20);
    }

    // 4. FC04: Read Input Registers (start: 0, qty: 2)
    {
        lastResponse.clear();
        auto adu = makeAdu(4, 1, function_code_t::READ_INPUT_REGS, {0x00, 0x00, 0x00, 0x02});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 13);
        CHECK(lastResponse[7] == function_code_t::READ_INPUT_REGS);
        CHECK(lastResponse[8] == 4);
        uint16_t reg0 = (static_cast<uint16_t>(lastResponse[9]) << 8) | lastResponse[10];
        uint16_t reg1 = (static_cast<uint16_t>(lastResponse[11]) << 8) | lastResponse[12];
        CHECK(reg0 == 500);
        CHECK(reg1 == 501);
    }
}

TEST_CASE("Modbus Application Layer: FC05, FC06, FC15, FC16 write functions")
{
    application::callback_map_t callbacks;

    uint16_t lastCoilAddr = 0xFFFF;
    bool lastCoilVal = false;
    boost::fusion::at_key<WRITE_SINGLE_COIL>(callbacks) =
        [&](uint16_t start_addr, uint16_t qty, const std::vector<bool>& values) -> error_code_t::type {
        lastCoilAddr = start_addr;
        if (!values.empty())
            lastCoilVal = values[0];
        return error_code_t::NO_ERROR;
    };

    uint16_t lastRegAddr = 0xFFFF;
    uint16_t lastRegVal = 0;
    boost::fusion::at_key<WRITE_SINGLE_REG>(callbacks) =
        [&](uint16_t start_addr, uint16_t qty, const std::vector<uint16_t>& values) -> error_code_t::type {
        lastRegAddr = start_addr;
        if (!values.empty())
            lastRegVal = values[0];
        return error_code_t::NO_ERROR;
    };

    std::vector<bool> multiCoilsWritten;
    boost::fusion::at_key<WRITE_MULTI_COIL>(callbacks) =
        [&](uint16_t start_addr, uint16_t qty, const std::vector<bool>& values) -> error_code_t::type {
        multiCoilsWritten = values;
        return error_code_t::NO_ERROR;
    };

    std::vector<uint16_t> multiRegsWritten;
    boost::fusion::at_key<WRITE_MULTI_REG>(callbacks) =
        [&](uint16_t start_addr, uint16_t qty, const std::vector<uint16_t>& values) -> error_code_t::type {
        multiRegsWritten = values;
        return error_code_t::NO_ERROR;
    };

    application::layer app(callbacks);
    std::vector<uint8_t> lastResponse;
    app.data_send_signal.connect([&](uint8_t* data, size_t size) {
        lastResponse.assign(data, data + size);
    });

    // FC05: Write Single Coil (addr: 5, value: 0xFF00 -> ON)
    {
        lastResponse.clear();
        auto adu = makeAdu(5, 1, function_code_t::WRITE_SINGLE_COIL, {0x00, 0x05, 0xFF, 0x00});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 12);
        CHECK(lastResponse[7] == function_code_t::WRITE_SINGLE_COIL);
        CHECK(lastCoilAddr == 5);
        CHECK(lastCoilVal == true);
    }

    // FC06: Write Single Register (addr: 12, value: 1234 = 0x04D2)
    {
        lastResponse.clear();
        auto adu = makeAdu(6, 1, function_code_t::WRITE_SINGLE_REG, {0x00, 0x0C, 0x04, 0xD2});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 12);
        CHECK(lastResponse[7] == function_code_t::WRITE_SINGLE_REG);
        CHECK(lastRegAddr == 12);
        CHECK(lastRegVal == 1234);
    }

    // FC15: Write Multiple Coils (start: 0, qty: 3, byte count: 1, val: 0b101 = 0x05)
    {
        lastResponse.clear();
        auto adu = makeAdu(7, 1, function_code_t::WRITE_MULTI_COIL, {0x00, 0x00, 0x00, 0x03, 0x01, 0x05});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 12);
        CHECK(lastResponse[7] == function_code_t::WRITE_MULTI_COIL);
        REQUIRE(multiCoilsWritten.size() == 3);
        CHECK(multiCoilsWritten[0] == true);
        CHECK(multiCoilsWritten[1] == false);
        CHECK(multiCoilsWritten[2] == true);
    }

    // FC16: Write Multiple Registers (start: 2, qty: 2, byte count: 4, val: [100, 200])
    {
        lastResponse.clear();
        auto adu = makeAdu(8, 1, function_code_t::WRITE_MULTI_REG,
                           {0x00, 0x02, 0x00, 0x02, 0x04, 0x00, 0x64, 0x00, 0xC8});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 12);
        CHECK(lastResponse[7] == function_code_t::WRITE_MULTI_REG);
        REQUIRE(multiRegsWritten.size() == 2);
        CHECK(multiRegsWritten[0] == 100);
        CHECK(multiRegsWritten[1] == 200);
    }
}

TEST_CASE("Modbus Application Layer: Error code and exception handling")
{
    application::callback_map_t callbacks;

    boost::fusion::at_key<READ_HOLDING_REGS>(callbacks) =
        [](uint16_t start_addr, uint16_t quantity, std::vector<uint16_t>& values) -> error_code_t::type {
        if (start_addr >= 500)
        {
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        return error_code_t::NO_ERROR;
    };

    application::layer app(callbacks);
    std::vector<uint8_t> lastResponse;
    app.data_send_signal.connect([&](uint8_t* data, size_t size) {
        lastResponse.assign(data, data + size);
    });

    // 1. Illegal Data Address: start_addr >= 500
    {
        lastResponse.clear();
        auto adu = makeAdu(10, 1, function_code_t::READ_HOLDING_REGS, {0x02, 0x00, 0x00, 0x02});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == (function_code_t::READ_HOLDING_REGS | 0x80));
        CHECK(lastResponse[8] == error_code_t::ILLEGAL_DATA_ADDRESS);
    }

    // 2. Illegal Data Value: requesting 0 registers (quantity < 1)
    {
        lastResponse.clear();
        auto adu = makeAdu(11, 1, function_code_t::READ_HOLDING_REGS, {0x00, 0x01, 0x00, 0x00});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == (function_code_t::READ_HOLDING_REGS | 0x80));
        CHECK(lastResponse[8] == error_code_t::ILLEGAL_DATA_VALUE);
    }

    // 3. Illegal Data Value: requesting > 125 holding registers
    {
        lastResponse.clear();
        auto adu = makeAdu(12, 1, function_code_t::READ_HOLDING_REGS, {0x00, 0x01, 0x00, 0x7E});
        app.handle_data_receive(adu.data(), adu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == (function_code_t::READ_HOLDING_REGS | 0x80));
        CHECK(lastResponse[8] == error_code_t::ILLEGAL_DATA_VALUE);
    }

    // 4. Undersized ADU packet (< MB_MIN_REQUEST_ADU_LENGTH = 12 bytes)
    {
        lastResponse.clear();
        std::vector<uint8_t> shortAdu = {0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x01, 0x01};
        app.handle_data_receive(shortAdu.data(), shortAdu.size());

        REQUIRE(lastResponse.size() >= 9);
        CHECK(lastResponse[7] == 0x80);
        CHECK(lastResponse[8] == error_code_t::SLAVE_DEVICE_FAILURE);
    }
}

TEST_CASE("Modbus Server & DataManager integration: Linear scaling, reads, and writes")
{
    auto dm = std::make_shared<DataManager>();

    // Seed DataManager with points
    dm->addBinaryTag("breaker_state");
    dm->addInternalData<bool>("breaker_state", true);

    dm->addBinaryTag("alarm_input");
    dm->addInternalData<bool>("alarm_input", false);

    dm->addAnalogTag("temperature");
    dm->addInternalData<double>("temperature", 50.0);

    dm->addAnalogTag("line_voltage");
    dm->addInternalData<double>("line_voltage", 75.0);

    Server server(dm);

    // Register points with addresses and linear ranges
    CHECK(server.addCoil(1, "breaker_state"));
    CHECK(server.addDiscreteInput(2, "alarm_input"));
    CHECK(server.addHoldingRegister(10, "temperature", {100.0, 0.0}));
    CHECK(server.addInputRegister(20, "line_voltage", {100.0, 0.0}));

    // Registering unmapped tag returns false
    CHECK_FALSE(server.addCoil(99, "nonexistent_tag"));
    CHECK_FALSE(server.addHoldingRegister(99, "nonexistent_tag", {100.0, 0.0}));

    // Test Read Coils
    {
        std::vector<bool> values;
        CHECK(server.readCoils(1, 1, values) == error_code_t::NO_ERROR);
        REQUIRE(values.size() == 1);
        CHECK(values[0] == true);
    }

    // Test Read Discrete Inputs
    {
        std::vector<bool> values;
        CHECK(server.readDiscreteInputs(2, 1, values) == error_code_t::NO_ERROR);
        REQUIRE(values.size() == 1);
        CHECK(values[0] == false);
    }

    // Test Read Holding Registers with Linear Scaling
    // Range [100.0, 0.0]: slope = 65535.0 / 100.0 = 655.35; temp = 50.0 -> 32767
    {
        std::vector<uint16_t> values;
        CHECK(server.readHoldingRegisters(10, 1, values) == error_code_t::NO_ERROR);
        REQUIRE(values.size() == 1);
        CHECK(values[0] == 32767);
    }

    // Test Read Input Registers with Linear Scaling
    // line_voltage = 75.0 -> 75 * 655.35 = 49151
    {
        std::vector<uint16_t> values;
        CHECK(server.readInputRegisters(20, 1, values) == error_code_t::NO_ERROR);
        REQUIRE(values.size() == 1);
        CHECK(values[0] == 49151);
    }

    // Test Write Coils updating DataManager
    {
        CHECK(server.writeCoils(1, 1, {false}) == error_code_t::NO_ERROR);
        auto updated = dm->snapshotUpdatedBinaryTags();
        CHECK(updated.count("breaker_state") == 1);
        CHECK(updated["breaker_state"] == false);
        dm->updateInternalData();
        CHECK(dm->getDataByTag<bool>("breaker_state") == false);
    }

    // Test Write Holding Registers with inverse scaling back to engineering units
    // Write 65535 -> should map to 100.0
    {
        CHECK(server.writeHoldingRegisters(10, 1, {65535}) == error_code_t::NO_ERROR);
        auto updated = dm->snapshotUpdatedAnalogTags();
        CHECK(updated.count("temperature") == 1);
        CHECK(doctest::Approx(updated["temperature"]).epsilon(0.01) == 100.0);
        dm->updateInternalData();
        CHECK(doctest::Approx(dm->getDataByTag<double>("temperature")).epsilon(0.01) == 100.0);
    }

    // Test Invalid Addresses
    {
        std::vector<bool> bools;
        CHECK(server.readCoils(999, 1, bools) == error_code_t::ILLEGAL_DATA_VALUE);

        std::vector<uint16_t> regs;
        CHECK(server.readHoldingRegisters(999, 1, regs) == error_code_t::ILLEGAL_DATA_ADDRESS);
        CHECK(server.writeHoldingRegisters(999, 1, {1234}) == error_code_t::ILLEGAL_DATA_ADDRESS);
    }
}
