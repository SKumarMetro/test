#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <errno.h>

#include <string>
#include <sys/time.h>

#include <cstdint>
#include <queue>
#include <map>
#include <sstream>
#include <thread>
#include <iostream>
#include <sys/ioctl.h>

using namespace std;
#define DEFAULT_SOCK_PATH "/var/run/wpa_supplicant/wlan0";

#define WPA_EVENT_BSS_ADDED     "CTRL-EVENT-BSS-ADDED"
#define WPA_EVENT_BSS_REMOVED   "CTRL-EVENT-BSS-REMOVED"
#define WPA_EVENT_SCAN_STARTED  "CTRL-EVENT-SCAN-STARTED"
#define WPA_EVENT_SCAN_RESULTS  "CTRL-EVENT-SCAN-RESULTS"
#define WPA_EVENT_SCAN_FAILED   "CTRL-EVENT-SCAN-FAILED"


#define RECV_BUFFER_SIZE 4096
#define SEND_BUFFER_SIZE 256
#define PRINTF_L1 printf
#define PRINTF_L2 //printf

const char *socket_path = DEFAULT_SOCK_PATH;
int scanInterval = 30;

double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t usec = (uint64_t)tv.tv_sec * 1000000L + tv.tv_usec;
    return ( static_cast<double>(usec)/1000000L );
}

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

class SureshCli
{
    public:
        SureshCli();
        ~SureshCli();
        int Init();
        int Send(const string& msg, bool bControl = false)
        {
            Socket &socket = (bControl) ? _CtrlSocket : _socket;
            if (!bControl) {
                bPending = true;
                lastRequest = msg;
            }
            int rc = socket.Send(msg.c_str());
            if (rc == -1) {
            //    Send failed possibly supplicant reopend the socket and we are fucked
            //    socket.Reopen();
            //    rc = Init();
            //    if (rc != -1)
            //        rc = socket.Send(msg);
            }
            return rc;
        }
        int Receive(char *reply, int reply_len, bool bControl = false)
        {
            Socket &socket = (bControl) ? _CtrlSocket : _socket;
            return socket.Receive(reply, reply_len) ;
        }
        void Push(const string &msg)
        {
            messages.push(msg);
        }
        const string Pop()
        {
            string msg;
            if (!messages.empty()) {
                msg = messages.front();
                messages.pop();
            }
            return msg;
        }
        Networks& getNetworks()
        {
            return _networks;
        }
        void ControlThread();
        void DataThread();

    private:
        void parseContrlMsg(const string &msg, string &cmd, string &bss);
        void processDetailResponse(const string &response, Network& network);

    class Socket {
    public:
        Socket();
        ~Socket();
        int Open();
        int Close();
        int Reopen();
        int Bind();
        int Connect();
        int SetNonBlocking();

        int Send(const string& msg);
        int Receive(char *reply, int reply_len);

    private:
        int fd;
        struct sockaddr_un remote;
        struct sockaddr_un local;
    };

    private:
        Socket _CtrlSocket;
        Socket _socket;
        std::queue<string> messages;
        Networks _networks;
        bool bPending;
        string lastRequest;
};

SureshCli::SureshCli()
{
    Init();
}

int SureshCli::Init()
{
    int rc = 0;
    printf(  "%s: Initializing Ctrl Socket\n", __FUNCTION__);
   _CtrlSocket.SetNonBlocking();
    if (_CtrlSocket.Bind() == 0) {
        if (_CtrlSocket.Connect() != 0) {
            printf(  "%s: Connect Failed\n", __FUNCTION__);
            rc = -1;
        }
    } else {
        rc = -1;
    }

    Send("ATTACH", true);

    printf(  "%s: Initializing Data Socket\n", __FUNCTION__);
   _socket.SetNonBlocking();
    if (_socket.Bind() == 0) {
        if (_socket.Connect() != 0) {
            printf(  "%s: Connect Failed\n", __FUNCTION__);
            rc = -1;
        }
    } else {
        rc = -1;
    }

    Send("SET bss_max_count 1024");

    return rc;
}

SureshCli::~SureshCli()
{
}

SureshCli::Socket::Socket()
{
    Open();
}

SureshCli::Socket::~Socket()
{
    Close();
}

int SureshCli::Socket::Close()
{
    printf("Shutting down %d\n", fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

int SureshCli::Socket::Open()
{
    int rc = 0;
    static int counter = 0;

    if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket error");
        exit(-1);
    }

    int rb = 0, sb = 0;
    socklen_t sz = sizeof(int);

    rb = RECV_BUFFER_SIZE;
    rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rb, sz);
    if (rc)
        printf("%s: Failed to SO_RCVBUF=%d errno=%d\n", __FUNCTION__, fd, rb, errno);
    sb = SEND_BUFFER_SIZE;
    rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sb, sz);
    if (rc)
        printf("%s: Failed to SO_SNDBUF=%d errno=%d\n", __FUNCTION__, fd, sb, errno);

    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rb, &sz);
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sb, &sz);


    /*
    root@spectrum110h:~# cat /proc/sys/net/ipv4/udp_rmem_min
    4096
    root@spectrum110h:~# cat /proc/sys/net/ipv4/udp_wmem_min
    4096
    root@spectrum110h:~# cat /proc/sys/net/ipv4/udp_mem
    11961   15949   23922

    Open: Created socket 4 SO_RCVBUF=163840 SO_SNDBUF=163840
    Open: Created socket 5 SO_RCVBUF=163840 SO_SNDBUF=163840
     */
    printf(  "%s: Created socket %d SO_RCVBUF=%d SO_SNDBUF=%d\n", __FUNCTION__, fd, rb, sb);

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, socket_path, sizeof(remote.sun_path)-1);

    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    sprintf(local.sun_path, "/tmp/wpa_ctrl-%d-%d", (int) getpid(), counter++ );

    return rc;
}

int SureshCli::Socket::Reopen()
{
    Close();
    Open();
}

int SureshCli::Socket::Bind()
{
    int rc = 0;
    printf("binding local to %s\n", local.sun_path);
    if (bind(fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
        printf("bind error!!! errno=%d\n", errno);
        if (errno == EADDRINUSE) {
            printf("local address (%s) is in use\n", local.sun_path);
        }
        rc = -1;
    }

    return rc;
}

int SureshCli::Socket::SetNonBlocking()
{
    int rc = 0;

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        printf(  "%s: fcntl(O_NONBLOCK) FAILED. errno=%d\n", __FUNCTION__, errno);
        rc = -1;
    } else {
        printf(  "%s: Using NON-BLOCKING socket\n", __FUNCTION__);
    }

    return rc;
}

int SureshCli::Socket::Connect()
{
    int rc = 0;

    if (connect(fd, (struct sockaddr*)&remote, sizeof(remote)) == -1) {
        rc = -1;
        perror("connect error");
        exit(-1);
    } else {
        printf("Connected\n");
    }

    return rc;
}

int SureshCli::Socket::Send(const string& msg)
{
    int rc = 0;
    int len = msg.length();
    PRINTF_L2 ("%lf: Sending '%s'\n", now(), msg.c_str());
    if (write(fd, msg.c_str(), len) != len) {
        perror("write error");
        rc = -1;
        //exit(-1);
    }
    return rc;
}

int SureshCli::Socket::Receive(char *reply, int reply_len)
{
    int rc = 0;
    fd_set rfds;
    struct timeval  tv;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec  = 0;
    tv.tv_usec = 100;

    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    //printf("select return %d\n", ret);
    if (ret > 0) {
        if (FD_ISSET(fd, &rfds)) {
            rc = recv(fd, reply, reply_len, 0);
            //printf("recv return %d\n", rc);


            int rcvbuf = 0;
            socklen_t optlen = sizeof(rcvbuf);
            if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) < 0)
                rcvbuf = -1;

            int inq2 = 0;
            if (ioctl(fd, TIOCINQ, &inq2) < 0)
                inq2 = -1;
            printf("%s: socket=%d inq1=%d inq2=%d rcvbuf=%d\n", __FUNCTION__, fd, 0, inq2, rcvbuf);

            if (rc > 0) {
            } else if (rc == 0) {   // DGRAM can be 0 byes NOT an error
                printf ("%s: errno=%d\n", __FUNCTION__, errno );
                //usleep(5000);
            } else if (rc == -1) {
                perror("recv error");
            }
        }
    } else if (ret == 0) {
        //perror("select timedout");
        usleep(500);
    } else if (ret == -1) {
        perror("select error");
    }

    return rc;
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
        printf ("%lf: Ignoring Non-ctrl msg '%s'\n", now(), msg.c_str() );
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

void SureshCli::ControlThread()
{
    char data[RECV_BUFFER_SIZE];

    printf ("Entering %s\n", __FUNCTION__);
    while ( true ) {

        int nbytes = Receive(data, sizeof(data), true);
        if (nbytes > 0) {
            string msg = string(reinterpret_cast<const char*>(data), (data[nbytes - 1] == '\n' ? nbytes - 1 : nbytes));
            Push(msg);

            //sleep(1);     // slow it down
            /*
            1581023279.324731: CTRL-DEBUG: ctrl_sock-sendmsg: sock=12 sndbuf=163840 outq=81536 send_len=42
            1581023279.324756: CTRL_IFACE monitor sent successfully to /tmp/wpa_ctrl-4550-0\x00
            1581023279.324767: CTRL: Had to throttle pending event message transmission for (sock 12 gsock -1)
            */
        }
    }
}

void SureshCli::DataThread()
{
    char data[RECV_BUFFER_SIZE];
    int waitTimeMS = 100;
    int elapsedTime = scanInterval*1000;
    int added = 0, removed = 0;

    printf ("Entering %s\n", __FUNCTION__);
    while ( true ) {
        // Check the data response
        int nbytes = Receive(data, sizeof(data));
        if (nbytes > 0) {
            string response = string(reinterpret_cast<const char*>(data), (data[nbytes - 1] == '\n' ? nbytes - 1 : nbytes));
            //printf ("%lf: Data '%.5s' Request=%s\n", now(), response.c_str(), lastRequest.c_str());
            //printf ("%lf: Data len=%d\n", now(), response.length() );
            if (data[0] == '<') {
                printf("Should never see CTRL message on this socket");
            } else if (lastRequest.compare(0, 3, "BSS") == 0) {
                Network network;
                processDetailResponse(response, network);
                _networks.Add(network);
            } else if (lastRequest.compare("SCAN_RESULTS") == 0) {
                printf ("SCAN_RESULTS response size=%d\n", response.length());
            } else {
            }
            bPending = false;
        }

        // check the control message
        string msg;
        if (!bPending)
            msg = Pop();

        if (msg.length()) {
            string cmd, bss;
            parseContrlMsg(msg, cmd, bss);

            if (cmd.compare(WPA_EVENT_SCAN_STARTED) == 0) {
                PRINTF_L1 ("%lf: %s\n", now(), cmd.c_str() );
                added = 0;
                removed = 0;
            } else if (cmd.compare(WPA_EVENT_SCAN_RESULTS) == 0) {
                PRINTF_L1 ("%lf: %s added=%d removed=%d\n", now(), cmd.c_str(), added, removed );
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
        } else {
            if (scanInterval && (elapsedTime >= scanInterval*1000)) {
                Send("SCAN");
                elapsedTime = 0;
            }
            elapsedTime += waitTimeMS;
        }

        // No data & No more control messages
        if ((nbytes == 0) && (msg.length() == 0))
            usleep(waitTimeMS * 1000);
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1)
        scanInterval = atoi (argv[1]);
    if (argc > 2)
        socket_path = argv[2];

    printf("socket_path=%s scanInterval=%d\n", socket_path, scanInterval);

    SureshCli sureshCli;

    thread control (&SureshCli::ControlThread, &sureshCli);
    thread data    (&SureshCli::DataThread,    &sureshCli);
    control.detach();
    data.detach();

    while ( true ) {
        string cmd;
        std::getline (cin, cmd);
        if (cmd.compare("exit") == 0)
            break;
        else if (cmd.compare("list") == 0) {
            Networks &networks = sureshCli.getNetworks();
            printf("%s: Networks.size=%d\n", __FUNCTION__, networks.size());
            Networks::iterator it = networks.begin();
            for (it = networks.begin(); it!=networks.end(); ++it) {
                Network& network = it->second;
                if (!network.ssid.empty())
                    printf("%s\t%d\t%d\t%d\t%s\n", network.bssid.c_str(), network.freq, network.level, network.snr, network.ssid.c_str());
            }
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
