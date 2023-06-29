#ifndef BENNU_UTILITY_ABSTRACTCLIENT_HPP
#define BENNU_UTILITY_ABSTRACTCLIENT_HPP

namespace bennu{
namespace utility{

class AbstractClient
{
    public:
        AbstractClient() = default;

        ~AbstractClient() = default;

        virtual bool connect() = 0;

        virtual void disconnect() = 0;

        virtual void send(uint8_t* buffer, size_t) = 0;

        virtual void receive(uint8_t* buffer, size_t) = 0;

};

}
}

#endif //BENNU_UTILITY_ABSTRACTCLIENT_HPP
