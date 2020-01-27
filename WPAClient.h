#ifndef WPACLIENT_H
#define WPACLIENT_H

//#include "Module.h"
#include <core/core.h>

namespace WPEFramework {
namespace WPASupplicant {

    class WPAClient : public Core::Thread
    {
        private:
            static constexpr uint32_t MaxConnectionTime = 3000;

        public:
            class Socket : public Core::StreamType<Core::SocketDatagram>
            {
            public:
                Socket();
                virtual ~Socket() {}
                void Send(const string &msg);
                string Receive();

                uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize);
                uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize);
                void StateChange();

            private:
                typedef Core::StreamType<Core::SocketDatagram> BaseClass;
                string supplicantBase;
                string interfaceName;
                uint32_t _error;
                bool attached;
                std::queue<string> _requests;
                std::queue<string> _responses;
            };

            WPAClient();
            virtual ~WPAClient();

        private:
            uint32_t Worker() override;
            void Dispose();

            Socket _CtrlSocket;
            Socket _socket;
    };
}
} // namespace WPEFramework::WPASupplicant

#endif
