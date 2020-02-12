#ifndef WPACLIENT_H
#define WPACLIENT_H

//#include "Module.h"
#include <core/core.h>

namespace WPEFramework {
namespace WPASupplicant {

    struct Network {
        string ssid;
        string bssid;
        int freq;
        int level;
        int snr;
    };

    class Networks : public std::map<string,Network>
    {
        public:
        void Add(Network& network)
        {
            insert( std::pair<string,Network>(network.bssid, network) );
        }
        void Remove(const string& bssid)
        {
            Networks::iterator it = find(bssid);
            if (it != end())
                erase (it);
        }
        void dump()
        {
            Networks::iterator it = begin();
        }
    };

    class WPAClient : public Core::Thread
    {
        private:
            static constexpr uint32_t MaxConnectionTime = 3000;
            typedef Core::StreamType<Core::SocketDatagram> BaseClass;

        public:
            class Socket : public BaseClass
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
#if USE_SINGLE_SOCK
            Socket &_socket;
#else
            Socket _socket;
#endif
            Networks _networks;
    };
}
} // namespace WPEFramework::WPASupplicant

#endif
