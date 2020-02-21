#include "SimpleSocket.h"

#define PRINTF_L1 printf
#define PRINTF_L2 //printf
#define PRINTF_L3 //printf

#define SOCKET_PATH "/var/run/wpa_supplicant/wlan0"

static double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t usec = (uint64_t)tv.tv_sec * 1000000L + tv.tv_usec;
    return ( static_cast<double>(usec)/1000000L );
}

SimpleSocket::SimpleSocket(const uint16_t nSendBufferSize, const uint16_t nReceiveBufferSize)
    : m_ReceiveBufferSize(nReceiveBufferSize)
    , m_SendBufferSize(nSendBufferSize)
    , _thread (&SimpleSocket::Worker, this )
    , _socket_path(SOCKET_PATH)
    , _sendPending(false)
{
    PRINTF_L1("%s: Inside\n", __FUNCTION__);
}

SimpleSocket::~SimpleSocket()
{
    PRINTF_L1("%s: Inside\n", __FUNCTION__);
}

void SimpleSocket::Worker()
{
    int rc = 0;
    PRINTF_L2("%s: Entering\n", __FUNCTION__);
    char data[m_ReceiveBufferSize];
    Open();

    PRINTF_L2("%s: Initializing\n", __FUNCTION__);
    SetNonBlocking();
    if (Bind() == 0) {
        if (Connect() != 0) {
            printf("%s: Connect Failed\n", __FUNCTION__);
            rc = -1;
        }
    } else {
        rc = -1;
    }
    StateChange();

    _running = true;
    while ( _running ) {
        int nbytes = Receive(data, sizeof(data) );
        //printf(  "%s: Received %d bytes\n", __FUNCTION__, nbytes);
        if (nbytes > 0) {
            ReceiveData((uint8_t *)data, nbytes);
        } else if (_sendPending) {
            char msg[m_SendBufferSize];
            int nbytes = SendData((uint8_t*)msg, m_SendBufferSize);
            if (nbytes) {
                Send(msg, nbytes);
            }
            _sendPending = false;
        }
    }

    PRINTF_L2("%s: Exiting\n", __FUNCTION__);
}

int SimpleSocket::Open(const uint32_t waitTime)
{
    int rc = 0;
    static int counter = 0;

    if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        PRINTF_L1("socket error errno=%d\n", errno);
        exit(-1);
    }

    int rb = 0, sb = 0;
    socklen_t sz = sizeof(int);

    rb = m_ReceiveBufferSize;
    rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rb, sz);
    if (rc)
        PRINTF_L1("%s: Socket=%d Failed to set SO_RCVBUF=%d  errno=%d\n", __FUNCTION__, fd, rb, errno);
    sb = m_SendBufferSize;
    rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sb, sz);
    if (rc)
        PRINTF_L1("%s: socket=%d Failed to set SO_SNDBUF=%d errno=%d\n", __FUNCTION__, fd, sb, errno);

    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rb, &sz);
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sb, &sz);

    PRINTF_L1("%s: Created socket %d SO_RCVBUF=%d SO_SNDBUF=%d\n", __FUNCTION__, fd, rb, sb);

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, _socket_path, sizeof(remote.sun_path)-1);

    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    sprintf(local.sun_path, "/tmp/wpa_ctrl-%d-%d", (int) getpid(), counter++ );

    return rc;
}

int SimpleSocket::Close(const uint32_t waitTime)
{
    _running = false;
    _thread.join();
    PRINTF_L1("Shutting down %d\n", fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);

    return 0;
}

bool SimpleSocket::IsOpen()
{
    bool result = true;
    int val;
    socklen_t sz = sizeof(val);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &sz);
    if (val) {
        result = false;
        PRINTF_L2("%s: SO_ERROR code %d errno=%d\n", __FUNCTION__, val, errno);
    }

    return result;
}

int SimpleSocket::Reopen()
{
    Close();
    return (Open());
}

int SimpleSocket::Bind()
{
    int rc = 0;
    PRINTF_L1("binding local to %s\n", local.sun_path);
    if (bind(fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
        PRINTF_L1("bind error!!! errno=%d\n", errno);
        if (errno == EADDRINUSE) {
            PRINTF_L1("local address (%s) is in use\n", local.sun_path);
        }
        rc = -1;
    }

    return rc;
}

int SimpleSocket::SetNonBlocking()
{
    int rc = 0;

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        PRINTF_L1("%s: fcntl(O_NONBLOCK) FAILED. errno=%d\n", __FUNCTION__, errno);
        rc = -1;
    } else {
        PRINTF_L1("%s: Using NON-BLOCKING socket\n", __FUNCTION__);
    }

    return rc;
}

int SimpleSocket::Connect()
{
    int rc = 0, retry = 0;
    bool done = false;

    while (!done) {
        if (connect(fd, (struct sockaddr*)&remote, sizeof(remote)) == -1) {
            rc = -1;
            PRINTF_L1 ("%lf: socket %d connect failed errno=%d\n", now(), fd, errno);
            usleep(100000);
            if (++retry > 3)
                done = true;
        } else {
            PRINTF_L1("%d Connected\n", fd);
            rc = 0;
            done = true;
        }
    }

    return rc;
}

int SimpleSocket::Send(char *msg, int len)
{
    int rc = 0;
    PRINTF_L2 ("%lf: Sending '%s'\n", now(), msg);
    if (write(fd, msg, len) != len) {
        perror("write error");
        rc = -1;
    }
    #if 0
        int outq = 0;
        if (ioctl(fd, TIOCOUTQ, &outq) < 0)
            outq = -1;

        PRINTF_L1 ("%lf: socket=%d Send '%s' errno=%d TIOCOUTQ=%d\n", now(), fd, msg, errno, outq);
        //PRINTF_L1 ("%lf: socket=%d Failed to send '%s' errno=%d TIOCOUTQ=%d\n", now(), fd, msg, errno, outq);
    #endif
    return rc;
}

int SimpleSocket::Receive(char *reply, int reply_len)
{
    int rc = 0;
    fd_set rfds;
    struct timeval  tv;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec  = 0;
    tv.tv_usec = 100;

    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    PRINTF_L3("select return %d\n", ret);
    if (ret > 0) {
        if (FD_ISSET(fd, &rfds)) {
            rc = recv(fd, reply, reply_len, 0);
            PRINTF_L3("recv return %d\n", rc);

            int rcvbuf = 0;
            socklen_t optlen = sizeof(rcvbuf);
            if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) < 0)
                rcvbuf = -1;

            int inq2 = 0;
            if (ioctl(fd, TIOCINQ, &inq2) < 0)
                inq2 = -1;
            PRINTF_L2("%s: socket=%d inq1=%d inq2=%d rcvbuf=%d\n", __FUNCTION__, fd, 0, inq2, rcvbuf);

            if (rc > 0) {
            } else if (rc == 0) {   // DGRAM can be 0 byes NOT an error
                printf ("%s: errno=%d\n", __FUNCTION__, errno );
            } else if (rc == -1) {
                perror("recv error");
            }
        }
    } else if (ret == 0) {
        usleep(1000);
    } else if (ret == -1) {
        perror("select error");
    }

    return rc;
}

void SimpleSocket::Trigger()
{
    _sendPending = true;
}
