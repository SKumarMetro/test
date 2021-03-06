#ifndef SIMPLESOCKET_H
#define SIMPLESOCKET_H

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
#include <sys/ioctl.h>
#include <thread>
#include <queue>

#include <core/core.h>

class SimpleSocket {
    public:
        SimpleSocket(const uint16_t nSendBufferSize, const uint16_t nReceiveBufferSize);
        ~SimpleSocket();
        int Open(const uint32_t waitTime = 0);
        int Close(const uint32_t waitTime = 0);
        int Reopen();
        int Bind();
        int Connect();
        int SetNonBlocking();
        bool IsOpen();
        bool IsClosed() { return !IsOpen(); }

    protected:
        int Send(char *msg, int msg_len);
        int Receive(char *reply, int reply_len);

        void Worker();
        virtual uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) = 0;
        virtual uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) = 0;
        virtual void StateChange() {}
        void Trigger();

    private:
        uint16_t m_ReceiveBufferSize;
        uint16_t m_SendBufferSize;
        std::thread _thread;
        bool _running;
        int fd;
        struct sockaddr_un remote;
        struct sockaddr_un local;
        const char* _socket_path;
        std::queue<std::string> _responses;
        bool _sendPending;
};

#endif
