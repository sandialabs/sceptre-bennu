#include "ClientScanBlock.hpp"

#include <thread>

namespace bennu {
namespace comms {
namespace modbus {

ClientScanBlock::ClientScanBlock() :
    mIsRunning(false),
    mScanInterval(5)
{
}

ClientScanBlock::~ClientScanBlock()
{
    mReadRequests.clear();
}

void ClientScanBlock::addReadRequest(std::shared_ptr<ClientConnection> connection, const std::set<comms::RegisterDescriptor, comms::RegDescComp>& registers)
{
    ConnectionMessage rm;
    rm.mRegisters = registers;
    rm.mRegisterType = (*registers.begin()).mRegisterType;

    ReadRequests::iterator iter = mReadRequests.find(connection);
    if (iter != mReadRequests.end())
    {
        iter->second.push_back(rm);
    }
    else
    {
        std::vector<ConnectionMessage> messages;
        messages.push_back(rm);
        mReadRequests[connection] = messages;
    }
}

void ClientScanBlock::run()
{
    mIsRunning = true;
    while (mIsRunning)
    {
        scan();
        std::this_thread::sleep_for(std::chrono::seconds(mScanInterval));
    }
}

void ClientScanBlock::scan()
{
    std::set<std::string> tags;
    mRegisterResponses.clear();
    std::vector<std::thread> threads;
    for (ReadRequests::iterator iter = mReadRequests.begin(); iter != mReadRequests.end(); ++iter)
    {
        threads.push_back(std::thread(std::bind(&ClientScanBlock::scanConnectionRegisters, this, iter->first, iter->second)));

        for (size_t i = 0; i < iter->second.size(); ++i)
        {
            for (std::set<comms::RegisterDescriptor, comms::RegDescComp>::iterator regIter = iter->second[i].mRegisters.begin();
                 regIter != iter->second[i].mRegisters.end(); ++regIter)
            {
                tags.insert((*regIter).mTag);
            }
        }
    }

    std::for_each(threads.begin(),threads.end(), std::mem_fn(&std::thread::join));

    if (!mClient.expired())
    {
        if (mRegisterResponses.size() != tags.size())
        {
            std::vector<comms::RegisterDescriptor> toAdd;
            for (auto iter = tags.begin(); iter != tags.end(); ++iter)
            {
                bool found = false;
                for (size_t i = 0; i < mRegisterResponses.size(); ++i)
                {
                    if (mRegisterResponses[i].mTag == *iter)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    comms::RegisterDescriptor rd;
                    rd.mTag = *iter;
                    rd.mRegisterType = comms::eNone;
                    toAdd.push_back(rd);
                }
            }

            mRegisterResponses.insert(mRegisterResponses.begin(), toAdd.begin(), toAdd.end());
        }

        //mClient.lock()->report("modbus", mRegisterResponses);
    }

}

std::string ClientScanBlock::specificScan(std::set<std::string>& tagsToRead)
{
    mRegisterResponses.clear();
    std::vector<std::thread> threads;
    for (ReadRequests::iterator iter = mReadRequests.begin(); iter != mReadRequests.end(); ++iter)
    {
        threads.push_back(std::thread(std::bind(&ClientScanBlock::scanConnectionRegisters, this, iter->first, iter->second)));

        for (size_t i = 0; i < iter->second.size(); ++i)
        {
            for (std::set<comms::RegisterDescriptor, comms::RegDescComp>::iterator regIter = iter->second[i].mRegisters.begin();
                 regIter != iter->second[i].mRegisters.end(); ++regIter)
            {
                tagsToRead.insert((*regIter).mTag);
            }
        }
    }

    std::for_each(threads.begin(),threads.end(), std::mem_fn(&std::thread::join));

    return "modbus";
}

void ClientScanBlock::scanConnectionRegisters(std::shared_ptr<ClientConnection> connection, const std::vector<ConnectionMessage>& messages)
{
    std::vector<comms::RegisterDescriptor> responses;
    std::vector<comms::LogMessage> logMessages;
    auto result = connection->readRegisters(messages, responses, logMessages);

    mLock.lock();
    mRegisterResponses.insert(mRegisterResponses.end(), responses.begin(), responses.end());
    mLock.unlock();

    if (!mClient.expired())
    {
        for (auto lm : logMessages)
        {
            mClient.lock()->logEvent(lm.mEvent, lm.mLevel, lm.mMessage);
        }

        if (!result.status)
        {
            mClient.lock()->logDebug("error", "A scan block for " + connection->getName() + " failed to read a bank of registers");
        }
    }
}

} // namespace modbus
} // namespace comms
} // namespace bennu
