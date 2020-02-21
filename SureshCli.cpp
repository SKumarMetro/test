#include <stdio.h>

#include <cstdint>
#include <sstream>
#include <iostream>

#include "SureshCli.h"

#define PRINTF_L1 printf
#define PRINTF_L2 //printf
#define PRINTF_L3 //printf

#define WPA_EVENT_BSS_ADDED     "CTRL-EVENT-BSS-ADDED"
#define WPA_EVENT_BSS_REMOVED   "CTRL-EVENT-BSS-REMOVED"
#define WPA_EVENT_SCAN_STARTED  "CTRL-EVENT-SCAN-STARTED"
#define WPA_EVENT_SCAN_RESULTS  "CTRL-EVENT-SCAN-RESULTS"
#define WPA_EVENT_SCAN_FAILED   "CTRL-EVENT-SCAN-FAILED"
//"CTRL-EVENT-TERMINATING"

int scanInterval = 30;

static double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t usec = (uint64_t)tv.tv_sec * 1000000L + tv.tv_usec;
    return ( static_cast<double>(usec)/1000000L );
}

SureshCli::SureshCli()
    : SimpleSocket(WIFI_SENDBUF_SIZE, WIFI_RECVBUF_SIZE)
    , _CtrlSocket(*this)
{
}

SureshCli::~SureshCli()
{
}

void SureshCli::StateChange()
{
    PRINTF_L1(  "%s: \n", __PRETTY_FUNCTION__);
    if (IsOpen()) {
        Send("SET bss_max_count 1024");
    }
}

uint16_t SureshCli::SendData(uint8_t* dataFrame, const uint16_t maxSendSize)
{
    uint16_t result = 0;
    string msg;
    if (!_requests.empty() && pending == 0) {
        ++pending;
        msg = _requests.front();
        lastRequest = msg;
        _requests.pop();
        result = msg.length();
        memcpy(dataFrame, msg.c_str(), result);
        PRINTF_L2(  "%s: Sending %s pending=%d\n", __FUNCTION__, msg.c_str(), pending);
    }
    return result;
}

uint16_t SureshCli::ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize)
{
    if (receivedSize)
        --pending;

    PRINTF_L2(  "%s: Received %d bytes pending=%d\n", __FUNCTION__, receivedSize, pending);
    string response = string(reinterpret_cast<const char*>(dataFrame), (dataFrame[receivedSize - 1] == '\n' ? receivedSize - 1 : receivedSize));
    processResponse(response);
    if (!_requests.empty()) {
        Trigger();
    }
}

int SureshCli::Send(const string& msg, bool bControl)
{
    int rc = 0;
    if (!bControl) {
        //bPending = true;
        //lastRequest = msg;
    }

    if (bControl) {
        _CtrlSocket.Send(msg);
    } else {
        _requests.push(msg);
        Trigger();
    }

    return rc;
}

Networks& SureshCli::getNetworks()
{
    return _networks;
}


void SureshCli::parseContrlMsg(const string &msg, string &cmd, string &bss)
{
    if (msg[0] == '<') {
        PRINTF_L2 ("%lf: %s\n", now(), msg.c_str() );
        int pos = msg.find_first_of(' ');
        if (pos == string::npos) {
            cmd = msg.substr(3);
            bss = "";
        } else {
            cmd = msg.substr(3, pos-3);

            int pos = msg.find_last_of(' ');
            if (pos != string::npos)
                bss = msg.substr(pos+1);
        }
    } else {
        PRINTF_L2 ("%lf: Ignoring Non-ctrl msg '%s'\n", now(), msg.c_str() );
    }
}

void SureshCli::processDetailResponse(const string &response, Network& network)
{
    stringstream ss(response);
    string line;

    while (getline(ss, line)) {
        int pos = line.find_first_of('=');
        if (pos != string::npos) {
            string name = line.substr(0, pos);
            string value = line.substr(pos+1);
            if (name.compare("ssid") == 0 )
                network.ssid = value;
            else if (name.compare("bssid") == 0 )
                network.bssid = value;
            else if (name.compare("freq") == 0 )
                network.freq = atoi(value.c_str());
            else if (name.compare("level") == 0 )
                network.level = atoi(value.c_str());
            else if (name.compare("snr") == 0 )
                network.snr = atoi(value.c_str());
        }
    }
}

void SureshCli::processResponse( const string& response)
{
    PRINTF_L2 ("%lf: Data '%.5s' Request=%s\n", now(), response.c_str(), lastRequest.c_str());
    if (lastRequest.compare(0, 3, "BSS") == 0) {
        Network network;
        processDetailResponse(response, network);
        _networks.Add(network);
    } else if (lastRequest.compare("SCAN_RESULTS") == 0) {
        PRINTF_L3 ("SCAN_RESULTS response size=%d\n", response.length());
    } else {
    }
}

void SureshCli::processControlMessage( const string& msg )
{
    if (msg.length()) {
        string cmd, bss;
        parseContrlMsg(msg, cmd, bss);

        if (cmd.compare(WPA_EVENT_SCAN_STARTED) == 0) {
            PRINTF_L1 ("%lf: %s\n", now(), cmd.c_str() );
            added = 0;
            removed = 0;
        } else if (cmd.compare(WPA_EVENT_SCAN_RESULTS) == 0) {
            PRINTF_L1 ("%lf: %s added=%d removed=%d _networks.size=%d\n", now(), cmd.c_str(), added, removed, _networks.size() );
            // NEVER need SCAN_RESULTS as long as we process ADD/REMOVE just after the 1st scan since sup started
            //Send("SCAN_RESULTS");
        } else if ((cmd.compare(WPA_EVENT_BSS_ADDED) == 0) && !bss.empty()) {
            ++added;
            stringstream ss;
            ss << "BSS " << bss;
            Send (ss.str());
        } else if ((cmd.compare(WPA_EVENT_BSS_REMOVED) == 0) && !bss.empty()) {
            ++removed;
            _networks.Remove(bss);
        }
        //PRINTF_L1 ("%lf: %s added=%d removed=%d _networks.size=%d\n", now(), cmd.c_str(), added, removed, _networks.size() );
    }
}


SureshCli::ControlSocket::ControlSocket(SureshCli& parent)
    : _parent(parent)
    , SimpleSocket(WIFI_SENDBUF_SIZE, WIFI_RECVBUF_SIZE)
{
}

uint16_t SureshCli::ControlSocket::SendData(uint8_t* dataFrame, const uint16_t maxSendSize)
{
    uint16_t result = 0;
    string msg;
    if (!_requests.empty()) {
        msg = _requests.front();
        _requests.pop();
        result = msg.length();
        memcpy(dataFrame, msg.c_str(), result);
    }
    return result;
}

uint16_t SureshCli::ControlSocket::ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize)
{
    PRINTF_L2(  "%s: Received %d bytes\n", __PRETTY_FUNCTION__, receivedSize);
    string message = string(reinterpret_cast<const char*>(dataFrame), (dataFrame[receivedSize - 1] == '\n' ? receivedSize - 1 : receivedSize));
    _parent.processControlMessage(message);
}

void SureshCli::ControlSocket::StateChange()
{
    PRINTF_L1(  "%s: \n", __PRETTY_FUNCTION__);
    if (!_attached) {
        _attached = true;
        Send("ATTACH");
    }
}

int SureshCli::ControlSocket::Send(const string& msg) {
    _requests.push(msg);
    Trigger();
    return 0;
}


int main(int argc, char *argv[])
{
    const char *socket_path = DEFAULT_SOCK_PATH;
    if (argc > 1)
        scanInterval = atoi (argv[1]);
    if (argc > 2)
        socket_path = argv[2];

    PRINTF_L1("socket_path=%s scanInterval=%d\n", socket_path, scanInterval);

    SureshCli sureshCli;

    while ( true ) {
        string cmd;
        std::getline (cin, cmd);
        if (cmd.compare("exit") == 0) {
            break;
        } else if (cmd.compare("scan") == 0) {
            sureshCli.Send("SCAN");
        } else if (cmd.compare("list") == 0) {
            Networks &networks = sureshCli.getNetworks();
            PRINTF_L1("%s: Networks.size=%d\n", __FUNCTION__, networks.size());
            Networks::iterator it = networks.begin();
            for (it = networks.begin(); it!=networks.end(); ++it) {
                Network& network = it->second;
                if (!network.ssid.empty())
                    PRINTF_L1("%s\t%d\t%d\t%d\t%s\n", network.bssid.c_str(), network.freq, network.level, network.snr, network.ssid.c_str());
            }
        } else {
            if (!cmd.empty())
            PRINTF_L1("Unknown command '%s'\n", cmd.c_str());
        }
    }
    return 0;
}



/*
Deatil Reqponse
id=17568
bssid=78:f2:9e:2e:98:48
freq=2412
beacon_int=100
capabilities=0x1431
qual=0
noise=-89
level=-66
tsf=0000156411594411
age=0
ie=000e414c424552545f414234415f3234010882848b968c1298240301010504000100000706555320010b1e2a01003204b048606c460571d000000c2d1aad011bffffff00000000000000000001000000000406e6470d003d16010001000000000000000000000000000000000000004a0e14000a002c01c8001400050019007f080100070200000040dd180050f2020101800003a4000027a4000042435e0062322f00dd0900037f01010000ff7fdd1d0050f204104a0001101044000102103c0001011049000600372a00012030140100000fac040100000fac040100000fac020000
flags=[WPA2-PSK-CCMP][WPS][ESS]
ssid=ALBERT_AB4A_24
wps_state=configured
snr=23
est_throughput=58500
*/
