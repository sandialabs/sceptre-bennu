#include "doctest.h"

#include <memory>
#include <string>

#include "opendnp3/gen/EventAnalogOutputStatusVariation.h"
#include "opendnp3/gen/EventAnalogVariation.h"
#include "opendnp3/gen/EventBinaryOutputStatusVariation.h"
#include "opendnp3/gen/EventBinaryVariation.h"
#include "opendnp3/gen/PointClass.h"
#include "opendnp3/gen/StaticAnalogOutputStatusVariation.h"
#include "opendnp3/gen/StaticAnalogVariation.h"
#include "opendnp3/gen/StaticBinaryOutputStatusVariation.h"
#include "opendnp3/gen/StaticBinaryVariation.h"
#include "opendnp3/outstation/DatabaseConfig.h"

#include "bennu/devices/field-device/DataManager.hpp"
#include "Server.hpp"

using bennu::comms::dnp3::Server;
using bennu::field_device::DataManager;
using opendnp3::PointClass;

TEST_CASE("DNP3 regression a17b391: control points map to output status, not inputs")
{
    auto dm = std::make_shared<DataManager>();
    dm->addInternalData<bool>("brkr_in", false);
    dm->addBinaryTag("brkr_in");
    dm->addInternalData<bool>("brkr_ctrl", false);
    dm->addBinaryTag("brkr_ctrl");
    dm->addInternalData<double>("load_mw_in", 0.0);
    dm->addAnalogTag("load_mw_in");
    dm->addInternalData<double>("load_mw_sp", 0.0);
    dm->addAnalogTag("load_mw_sp");

    Server server(dm);

    // Register 1 binary input and 1 binary output
    REQUIRE(server.addBinaryInput(0, "brkr_in", "", "", ""));
    REQUIRE(server.addBinaryOutput(1, "brkr_ctrl", true, ""));

    // Register 1 analog input and 1 analog output
    REQUIRE(server.addAnalogInput(0, "load_mw_in", "", "", "", 0.0));
    REQUIRE(server.addAnalogOutput(1, "load_mw_sp", false, ""));

    // Verify database configuration separates inputs from output status tables
    opendnp3::DatabaseConfig db;
    // Pre-populate database map slots as Server::init would
    db.binary_input[0] = {};
    db.binary_output_status[1] = {};
    db.analog_input[0] = {};
    db.analog_output_status[1] = {};

    server.configurePoints(db);

    // Binary control point must be in binary_output_status (group 10/40), not binary_input
    CHECK(db.binary_input.find(0) != db.binary_input.end());
    CHECK(db.binary_input.find(1) == db.binary_input.end());
    CHECK(db.binary_output_status.find(1) != db.binary_output_status.end());
    CHECK(db.binary_output_status.find(0) == db.binary_output_status.end());

    // Analog control point must be in analog_output_status (group 40), not analog_input
    CHECK(db.analog_input.find(0) != db.analog_input.end());
    CHECK(db.analog_input.find(1) == db.analog_input.end());
    CHECK(db.analog_output_status.find(1) != db.analog_output_status.end());
    CHECK(db.analog_output_status.find(0) == db.analog_output_status.end());

    // Check getBinaryPoint and getAnalogPoint lookups
    const auto* binOut = server.getBinaryPoint(1);
    REQUIRE(binOut != nullptr);
    CHECK(binOut->tag == "brkr_ctrl");
    CHECK(binOut->sbo == true);

    CHECK(server.getBinaryPoint(0) == nullptr); // Address 0 is an input, not an output

    const auto* analogOut = server.getAnalogPoint(1);
    REQUIRE(analogOut != nullptr);
    CHECK(analogOut->tag == "load_mw_sp");
    CHECK(analogOut->sbo == false);

    CHECK(server.getAnalogPoint(0) == nullptr); // Address 0 is an input, not an output
}

TEST_CASE("DNP3 regression a17b391: input and output points safely share the same index")
{
    auto dm = std::make_shared<DataManager>();
    dm->addInternalData<bool>("breaker_state", false);
    dm->addBinaryTag("breaker_state");
    dm->addInternalData<bool>("breaker_trip_cmd", false);
    dm->addBinaryTag("breaker_trip_cmd");
    dm->addInternalData<double>("tap_position", 0.0);
    dm->addAnalogTag("tap_position");
    dm->addInternalData<double>("tap_setpoint", 0.0);
    dm->addAnalogTag("tap_setpoint");

    Server server(dm);

    // Share index 5 for binary input and binary output
    REQUIRE(server.addBinaryInput(5, "breaker_state", "", "", ""));
    REQUIRE(server.addBinaryOutput(5, "breaker_trip_cmd", true, ""));

    // Share index 10 for analog input and analog output
    REQUIRE(server.addAnalogInput(10, "tap_position", "", "", "", 0.1));
    REQUIRE(server.addAnalogOutput(10, "tap_setpoint", false, ""));

    opendnp3::DatabaseConfig db;
    db.binary_input[5] = {};
    db.binary_output_status[5] = {};
    db.analog_input[10] = {};
    db.analog_output_status[10] = {};

    server.configurePoints(db);

    // Both tables hold the same index without conflict
    CHECK(db.binary_input.find(5) != db.binary_input.end());
    CHECK(db.binary_output_status.find(5) != db.binary_output_status.end());

    CHECK(db.analog_input.find(10) != db.analog_input.end());
    CHECK(db.analog_output_status.find(10) != db.analog_output_status.end());

    // Lookup output points by shared index
    const auto* binPt = server.getBinaryPoint(5);
    REQUIRE(binPt != nullptr);
    CHECK(binPt->tag == "breaker_trip_cmd");

    const auto* anPt = server.getAnalogPoint(10);
    REQUIRE(anPt != nullptr);
    CHECK(anPt->tag == "tap_setpoint");
}

TEST_CASE("DNP3 regression a17b391: Class0 default prevents buffer overflow vs explicit event classes")
{
    auto dm = std::make_shared<DataManager>();
    dm->addInternalData<bool>("b_static", false);
    dm->addBinaryTag("b_static");
    dm->addInternalData<bool>("b_event", false);
    dm->addBinaryTag("b_event");
    dm->addInternalData<bool>("bo_static", false);
    dm->addBinaryTag("bo_static");
    dm->addInternalData<bool>("bo_event", false);
    dm->addBinaryTag("bo_event");
    dm->addInternalData<double>("a_static", 0.0);
    dm->addAnalogTag("a_static");
    dm->addInternalData<double>("a_event", 0.0);
    dm->addAnalogTag("a_event");
    dm->addInternalData<double>("ao_static", 0.0);
    dm->addAnalogTag("ao_static");
    dm->addInternalData<double>("ao_event", 0.0);
    dm->addAnalogTag("ao_event");

    Server server(dm);

    // Default class (Class0) - prevents 100-event buffer fill on unpolled masters
    REQUIRE(server.addBinaryInput(0, "b_static", "", "", ""));
    REQUIRE(server.addBinaryOutput(0, "bo_static", false, ""));
    REQUIRE(server.addAnalogInput(0, "a_static", "", "", "", 0.0));
    REQUIRE(server.addAnalogOutput(0, "ao_static", false, ""));

    // Explicit event classes (Class1, Class2, Class3)
    REQUIRE(server.addBinaryInput(1, "b_event", "", "", "Class1"));
    REQUIRE(server.addBinaryOutput(1, "bo_event", true, "Class2"));
    REQUIRE(server.addAnalogInput(1, "a_event", "", "", "Class3", 0.5));
    REQUIRE(server.addAnalogOutput(1, "ao_event", false, "Class1"));

    opendnp3::DatabaseConfig db;
    db.binary_input[0] = {};
    db.binary_input[1] = {};
    db.binary_output_status[0] = {};
    db.binary_output_status[1] = {};
    db.analog_input[0] = {};
    db.analog_input[1] = {};
    db.analog_output_status[0] = {};
    db.analog_output_status[1] = {};

    server.configurePoints(db);

    // Verify Class0 defaults
    CHECK(db.binary_input[0].clazz == PointClass::Class0);
    CHECK(db.binary_output_status[0].clazz == PointClass::Class0);
    CHECK(db.analog_input[0].clazz == PointClass::Class0);
    CHECK(db.analog_output_status[0].clazz == PointClass::Class0);

    // Verify explicit classes and deadband
    CHECK(db.binary_input[1].clazz == PointClass::Class1);
    CHECK(db.binary_output_status[1].clazz == PointClass::Class2);
    CHECK(db.analog_input[1].clazz == PointClass::Class3);
    CHECK(doctest::Approx(db.analog_input[1].deadband) == 0.5);
    CHECK(db.analog_output_status[1].clazz == PointClass::Class1);

    // Verify event buffer size configuration method
    server.setEventBufferSize(200);

    // Invalid class names fail gracefully
    CHECK_FALSE(server.addBinaryInput(99, "b_static", "", "", "InvalidClass"));
    CHECK_FALSE(server.addBinaryOutput(99, "bo_static", false, "BadClass"));
    CHECK_FALSE(server.addAnalogInput(99, "a_static", "", "", "NoClass", 0.0));
    CHECK_FALSE(server.addAnalogOutput(99, "ao_static", false, "Unknown"));
}
