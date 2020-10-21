#include <cstdio>
#include <iostream>
#include <vector>
#include <signal.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/un.h>
#include <stdio.h>
#include "Timer/CheckInputs.hpp"
#include "Server.hpp"

#define MAX_EVENTS 64
#define BUFFERUNIXSOCKET 8

#define UNIXSOCKET_PATH "/etc/zabbix/io.sock"

void signal_callback_handler(int signum) {
    printf("Caught signal SIGPIPE %d\n",signum);
}

ssize_t readWriteSocket(char *buffer,size_t size)
{
    sockaddr_un addr;
    int fd;

    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIXSOCKET_PATH, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      perror("connect error");
      exit(-1);
    }

    if(::write(fd, buffer, size) != (ssize_t)size)
        return -1;
    return read(fd, buffer, BUFFERUNIXSOCKET);
}

int main(int argc, char *argv[])
{
    /* Catch Signal Handler SIGPIPE */
    if(signal(SIGPIPE, signal_callback_handler)==SIG_ERR)
    {
        std::cerr << "signal(SIGPIPE, signal_callback_handler)==SIG_ERR, errno: " << std::to_string(errno) << std::endl;
        abort();
    }

    std::vector<std::string> inputsMapping;
    inputsMapping.push_back("Victron");
    inputsMapping.push_back("BlueSun");
    inputsMapping.push_back("CRE");
    inputsMapping.push_back("Stop");
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--list") {
            std::cout << "{\n"
                         "  \"data\":[\n";
            for (size_t i = 0; i < inputsMapping.size(); i++)
            {
                if(i>0)
                    std::cout << ",\n";
                std::cout << "    {\"{#INPUT}\":\""+inputsMapping.at(i)+"\"}";
            }
            std::cout << "\n  ]\n"
                         "}" << std::endl;
            exit(0);
        } else if (std::string(argv[i]) == "--value") {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                std::string name = argv[++i]; // Increment 'i' so we don't get the argument as the next argv[i].
                size_t index=0;
                while(index<inputsMapping.size())
                {
                    if(name==inputsMapping.at(index))
                    {
                        char buffer[BUFFERUNIXSOCKET];
                        buffer[0]=0x01;
                        buffer[1]=(uint8_t)index;
                        const size_t size=readWriteSocket(buffer,2);
                        if(size!=1)
                        {
                            std::cerr << "Wrong server output size (Internal error): " << std::to_string(size) << std::endl;
                            exit(1);
                        }
                        if(buffer[0])
                            std::cout << "1" << std::endl;
                        else
                            std::cout << "0" << std::endl;
                        exit(0);
                    }
                    index++;
                }
                std::cerr << "The input \"" << name << "\" is not into the supported list: ";
                for (size_t i = 0; i < inputsMapping.size(); i++)
                {
                    if(i>0)
                        std::cerr << ",";
                    std::cerr << inputsMapping.at(i);
                }
                std::cerr << std::endl;
                exit(1);
            } else { // Uh-oh, there was no argument to the destination option.
                std::cerr << "--value option requires one argument." << std::endl;
                return 1;
            }
        } else if (std::string(argv[i]) == "--downcount") {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                std::string name = argv[++i]; // Increment 'i' so we don't get the argument as the next argv[i].
                size_t index=0;
                while(index<inputsMapping.size())
                {
                    if(name==inputsMapping.at(index))
                    {
                        char buffer[BUFFERUNIXSOCKET];
                        buffer[0]=0x02;
                        buffer[1]=(uint8_t)index;
                        const size_t size=readWriteSocket(buffer,2);
                        if(size!=8)
                        {
                            std::cerr << "Wrong server output size (Internal error): " << std::to_string(size) << std::endl;
                            exit(1);
                        }
                        uint64_t value=*reinterpret_cast<uint64_t *>(buffer);
                        std::cout << std::to_string(value) << std::endl;
                        exit(0);
                    }
                    index++;
                }
                std::cerr << "The input \"" << name << "\" is not into the supported list: ";
                for (size_t i = 0; i < inputsMapping.size(); i++)
                {
                    if(i>0)
                        std::cerr << ",";
                    std::cerr << inputsMapping.at(i);
                }
                std::cerr << std::endl;
                exit(1);
            } else { // Uh-oh, there was no argument to the destination option.
                std::cerr << "--downcount option requires one argument." << std::endl;
                return 1;
            }
        } else if (std::string(argv[i]) == "--help") {
            std::cerr << "--list: discovery json list into zabbix format" << std::endl;
            std::cerr << "--value X: get a single value" << std::endl;
            std::cerr << "--downcount X: get the down count detected" << std::endl;
            return 1;
        }
    }

    (void)argc;
    (void)argv;

    //the event loop
    struct epoll_event ev, events[MAX_EVENTS];
    memset(&ev,0,sizeof(ev));
    int nfds, epollfd;

    ev.events = EPOLLIN|EPOLLET;

    if ((epollfd = epoll_create1(0)) == -1) {
        printf("epoll_create1: %s", strerror(errno));
        return -1;
    }
    EpollObject::epollfd=epollfd;

    Server s(UNIXSOCKET_PATH);
    CheckInputs::checkInputs=new CheckInputs();
    CheckInputs::checkInputs->start(1);

    (void)s;
    for (;;) {
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1)
            printf("epoll_wait error %s", strerror(errno));
        for (int n = 0; n < nfds; ++n)
        {
            epoll_event &e=events[n];
            switch(static_cast<EpollObject *>(e.data.ptr)->getKind())
            {
                case EpollObject::Kind::Kind_Server:
                {
                    Server * server=static_cast<Server *>(e.data.ptr);
                    server->parseEvent(e);
                }
                break;
                break;
                case EpollObject::Kind::Kind_Timer:
                {
                    static_cast<Timer *>(e.data.ptr)->exec();
                    static_cast<Timer *>(e.data.ptr)->validateTheTimer();
                }
                break;
                default:
                break;
            }
        }
    }

    return 0;
}
