#include "WPAClient.h"

int scanInterval = 30;
int delay = 0;

namespace WPEFramework {
namespace WPASupplicant {

double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t usec = (uint64_t)tv.tv_sec * 1000000L + tv.tv_usec;
    return ( static_cast<double>(usec)/1000000L );
}

WPAClient::WPAClient()
#if USE_SINGLE_SOCK
    : _socket(_CtrlSocket)        // Single sock for both ctrl & messages'
#else
    : _socket()
    , _CtrlSocket()
#endif
{
    printf("%s: Entering\n", __FUNCTION__);
    Run();
}

WPAClient::~WPAClient()
{
    printf("%s: Entering\n", __FUNCTION__);
}

uint32_t WPAClient::Worker()
{
    printf("%s: Entering\n", __FUNCTION__);
    _CtrlSocket.Send("ATTACH");
    _socket.Send("SET bss_max_count 1024");

    int waitTimeMS = 10;
    int elapsedTime = 0;

    while (true) {
        string ctrlMsg = _CtrlSocket.Receive();

        if (ctrlMsg.length()) {
            printf("%lf: '%s'\n", now(), ctrlMsg.c_str() );
            continue;
        }

        string response = _socket.Receive();
        if (response.length()) {
            printf("%lf: '%s'\n", now(), response.c_str() );
            continue;
        }

        if ((elapsedTime == 0) && scanInterval)
            _socket.Send("SCAN");

        elapsedTime = (elapsedTime > scanInterval*1000) ? 0 : elapsedTime+waitTimeMS;
        usleep(waitTimeMS * 1000);
    }
    return 0;
}

void WPAClient::Dispose()
{
    printf("WPAClient thread::%s: Done!!!\n", __FUNCTION__);
}


WPAClient::Socket::Socket()
    #ifdef USE_SIMPLESOCKET
    : BaseClass("/var/run/wpa_supplicant/wlan0")
    #else
    : BaseClass(false, Core::NodeId(), Core::NodeId(), 4096, 4096)
    #endif
    , supplicantBase(_T("/var/run/wpa_supplicant"))
    , interfaceName(_T("wlan0"))
    , attached(false)
{
    static int count = 0;
    printf("%s: Entering\n", __FUNCTION__);
    string remoteName(Core::Directory::Normalize(supplicantBase) + interfaceName);

    if (Core::File(remoteName).Exists() == true) {
        string data(Core::Directory::Normalize(supplicantBase) + _T("wpa_ctrl_") + interfaceName + '-' + Core::NumberType<uint32_t>(::getpid()).Text()
            + "-" + Core::NumberType<uint32_t>(count++).Text() );
        #ifndef USE_SIMPLESOCKET
        LocalNode(Core::NodeId(data.c_str()));
        RemoteNode(Core::NodeId(remoteName.c_str()));
        _error = BaseClass::Open(MaxConnectionTime);
        #endif
        printf("open return %d\n", _error);
    }
}

void WPAClient::Socket::Send(const string &msg)
{
    _requests.push(msg);
    Trigger();
}

string WPAClient::Socket::Receive()
{
    string response;
    if (!_responses.empty()) {
        response = _responses.front();
        _responses.pop();
    }
    return response;
}

uint16_t WPAClient::Socket::SendData(uint8_t* dataFrame, const uint16_t maxSendSize)
{
    uint16_t result = 0;

    //printf("%lf: %s\n", now(), __FUNCTION__);
    if (!_requests.empty()) {
        string cmd = _requests.front();
        _requests.pop();
        result = cmd.length();
        memcpy(dataFrame, cmd.c_str(), result);
        printf("%s: Sending '%s'\n", __FUNCTION__, cmd.c_str());
    }

    return result;
}

uint16_t WPAClient::Socket::ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize)
{
    if (receivedSize) {
        string response = string(reinterpret_cast<const char*>(dataFrame), (dataFrame[receivedSize - 1] == '\n' ? receivedSize - 1 : receivedSize));
        _responses.push(response);
        usleep(delay * 1000);                 // slow it down
    }
    return receivedSize;
}

void WPAClient::Socket::StateChange()
{
    printf("%s: Entering\n", __FUNCTION__);
}

}
}


int main(int argc, char *argv[])
{
    if (argc > 1)
        scanInterval = atoi (argv[1]);
    if (argc > 2)
        delay = atoi (argv[2]);

    WPEFramework::WPASupplicant::WPAClient wpaClient;

    while ( true ) {
        string cmd;
        std::getline (std::cin, cmd);
        if (cmd.compare("exit") == 0)
            break;
    }
}

