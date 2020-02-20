#ifndef SURESHCLI_H
#define SURESHCLI_H

#include <map>

#include "SimpleSocket.h"

using namespace std;

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


class SureshCli : public SimpleSocket
{
    public:
        class ControlSocket : public SimpleSocket
        {
            public:
                ControlSocket(SureshCli& parent, const char *socket_path)
                    : _parent(parent)
                    , SimpleSocket(socket_path)
                {
                }
                uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize);
                uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize);
                void StateChange();
                int Send(const string& msg);

            private:
                SureshCli &_parent;
                std::queue<std::string> _requests;
                bool _attached = false;
        };

        SureshCli(const char* socket_path);
        ~SureshCli();
        int Send(const string& msg, bool bControl = false);
        Networks& getNetworks();
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize);
        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize);
        void StateChange();
        void processControlMessage( const string& msg );

    private:
        void processResponse( const string& response);
        void parseContrlMsg(const string &msg, string &cmd, string &bss);
        void processDetailResponse(const string &response, Network& network);

    private:
        ControlSocket _CtrlSocket;
        Networks _networks;
        string lastRequest;
        std::queue<std::string> _requests;
        int added = 0, removed = 0;
        int pending = 0;
};

#endif
