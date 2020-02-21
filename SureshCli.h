#ifndef SURESHCLI_H
#define SURESHCLI_H

#include <map>

#include "SimpleSocket.h"

#define DEFAULT_SOCK_PATH "/var/run/wpa_supplicant/wlan0";
#define WIFI_SENDBUF_SIZE 512
#define WIFI_RECVBUF_SIZE 32767

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
                ControlSocket(SureshCli& parent);
                /* virtual */ void StateChange();
                /* virtual */ uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize);
                /* virtual */ uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize);
                int Send(const string& msg);

            private:
                SureshCli &_parent;
                std::queue<std::string> _requests;
                bool _attached = false;
        };

        SureshCli();
        ~SureshCli();

        /* virtual */ void StateChange();
        /* virtual */ uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize);
        /* virtual */ uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize);

        int Send(const string& msg, bool bControl = false);
        Networks& getNetworks();

    private:
        void processResponse( const string& response);
        void parseContrlMsg(const string &msg, string &cmd, string &bss);
        void processDetailResponse(const string &response, Network& network);
        void processControlMessage( const string& msg );

    private:
        ControlSocket _CtrlSocket;
        Networks _networks;
        string lastRequest;
        std::queue<std::string> _requests;
        int added = 0, removed = 0;
        int pending = 0;
};

#endif
