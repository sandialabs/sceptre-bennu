#include "InputModule.hpp"

#include <iostream>
#include <vector>

namespace bennu {
namespace io {

InputModule::InputModule() :
    IOModule()
{
}

void InputModule::start(const distributed::Endpoint &endpoint)
{
    mSubscriber.reset(new distributed::Subscriber(endpoint));
    mSubscriber->setHandler(std::bind(&InputModule::subscriptionHandler, this, std::placeholders::_1));
}

void InputModule::subscriptionHandler(std::string& data)
{
    try {
        // Ex: "load-1_bus-101.mw:999.000,load-1_bus-101.active:true,"
        std::string pointDelimiter{","};
        std::string valueDelimiter{":"};
        std::vector<std::string> points;
        try {
            points = distributed::split(data, pointDelimiter);
        } catch (std::exception& e) {
            return;
        }
        
        for (auto& t : points)
        {
            if (t.length() == 0) { continue; } //Checks if point is empty

            if (t.find(":")==std::string::npos && (
                t.find("bus")==std::string::npos ||
                t.find("branch")==std::string::npos ||
                t.find("load")==std::string::npos ||
                t.find("shunt")==std::string::npos ||
                t.find("gen")==std::string::npos))
            {
                continue;
            }
            std::vector<std::string> parts;
            try {
                parts = distributed::split(t, valueDelimiter);
            } catch (std::exception& e) {
                continue;
            }

	    //Checks if point was cutoff/malformed
	    if (parts.size() < 2) { continue; }

            std::string name, value;
            name = parts[0];
            value = parts[1];

            if (mDataManager->hasPoint(name))
            {
                if (value == "true" || value == "false")
                {
                    bool val = value == "true" ? true : false;
                    mDataManager->setDataByPoint<bool>(name, val);
                }
                else
                {
                    try {
                        double val = std::stod(value);
                        mDataManager->setDataByPoint<double>(name, val);
                    } catch (std::exception& e) {
                        std::cout << "E: InputModule::subscriptionHandler -- value=" << value << " -- " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

} // namespace io
} // namespace bennu
