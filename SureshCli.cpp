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

using namespace std;

const char *socket_path = "/var/run/wpa_supplicant/wlan0";

class SureshCli
{
    public:
        SureshCli();
        ~SureshCli();
        int Send(const char *msg)
        {
            return _socket.Send(msg);
        }
        int Receive(char *reply, int reply_len)
        {
            return _socket.Receive(reply, reply_len) ;
        }


    class Socket {
    public:
        Socket();
        ~Socket();
        int Bind();
        int Connect();
        int SetNonBlocking();

        int Send(const char *msg);
        int Receive(char *reply, int reply_len);

    private:
        int fd;
        struct sockaddr_un remote;
        struct sockaddr_un local;
    };

    private:
        Socket _socket;
};

SureshCli::SureshCli()
{
    _socket.SetNonBlocking();
    if (_socket.Bind() == 0) {
        if (_socket.Connect() != 0) {
            printf(  "%s: Connect Failed\n", __FUNCTION__);
        }
    }
}

SureshCli::~SureshCli()
{
}

SureshCli::Socket::Socket()
{
    if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket error");
        exit(-1);
    }
    printf(  "%s: Created socket %d\n", __FUNCTION__, fd);

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, socket_path, sizeof(remote.sun_path)-1);

    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    sprintf(local.sun_path, "/tmp/wpa_ctrl-%d", (int) getpid() );
}

SureshCli::Socket::~Socket()
{
    close(fd);
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

int SureshCli::Socket::Send(const char *msg)
{
    int len = strlen(msg);
    printf ("%s\n", msg);
    if (write(fd, msg, len) != len) {
        if (len) {
            fprintf(stderr,"partial write");
        } else {
            perror("write error");
            exit(-1);
        }
    }
    return 0;
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

    if (ret > 0) {
        if (FD_ISSET(fd, &rfds)) {
            rc = recv(fd, reply, reply_len, 0);
            if (rc > 0) {
            } else if (rc == 0) {   // DGRAM can be 0 byes
                printf ("%s: errno=%d\n", __FUNCTION__, errno );
                //usleep(5000);
            } else if (rc == -1) {
                perror("recv error");
            }
        }
    }

    return rc;
}

int main(int argc, char *argv[])
{
    SureshCli sureshCli;
    char reply[4096];
    int count = 0;

    sureshCli.Send("ATTACH");

    while ( true ) {
        int nbytes = sureshCli.Receive(reply, sizeof(reply));
        if (nbytes > 0) {
            reply[nbytes] = '\0';
            printf ("%s\n", reply );
        } else {
            ++count;
            if (count == 5)
                sureshCli.Send("SCAN");
            if (count > 10) {
                sureshCli.Send("SCAN_RESULTS");
                count = 0;
            }
            sleep(1);
        }
    }

    return 0;
}
