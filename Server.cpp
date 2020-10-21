#include "Server.hpp"
#include "Timer/CheckInputs.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>

Server::Server(const char *const path)
{
    this->kind=EpollObject::Kind::Kind_Server;

    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        std::cerr << "Can't create the unix socket: " << errno << std::endl;
        abort();
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path,path);
    unlink(local.sun_path);
    int len = strlen(local.sun_path) + sizeof(local.sun_family);
    if(bind(fd, (struct sockaddr *)&local, len)!=0)
    {
        std::cerr << "Can't bind the unix socket, error (errno): " << errno << std::endl;
        abort();
    }

    if(listen(fd, 4096) == -1)
    {
        std::cerr << "Unable to listen, error (errno): " << errno << std::endl;
        abort();
    }
    chmod(path,(mode_t)S_IRWXU | S_IRWXG | S_IRWXO);

    int flags, s;
    flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1)
        std::cerr << "fcntl get flags error" << std::endl;
    else
    {
        flags |= O_NONBLOCK;
        s = fcntl(fd, F_SETFL, flags);
        if(s == -1)
            std::cerr << "fcntl set flags error" << std::endl;
    }

    epoll_event event;
    memset(&event,0,sizeof(event));
    event.data.ptr = this;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    //std::cerr << "EPOLL_CTL_ADD: " << fd << std::endl;
    if(epoll_ctl(epollfd,EPOLL_CTL_ADD, fd, &event) == -1)
    {
        std::cerr << "epoll_ctl failed to add server: " << errno << std::endl;
        abort();
    }
}

void Server::parseEvent(const epoll_event &)
{
    while(1)
    {
        sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);
        const int &infd = ::accept(fd, &in_addr, &in_len);
        if(infd == -1)
        {
            if((errno != EAGAIN) &&
            (errno != EWOULDBLOCK))
                std::cout << "connexion accepted" << std::endl;
            return;
        }

        char buffer[4096];
        ssize_t s=::read(infd,buffer,sizeof(buffer));
        //data[0]==0x01 query UP or down (8Bits), data[0]==0x02 query down count (64Bits)
        //data[1]==index
        if(s==2)
        {
            const uint8_t &queryType=buffer[0];
            const uint8_t &index=buffer[1];
            /* do directly into CheckInputs if(index>=CheckInputs::checkInputs->inputcount)
            {
                const uint64_t downCount=0;
                ::write(infd,&downCount,sizeof(downCount));
            }*/
            switch(queryType)
            {
                case 0x01:
                {
                    const bool isUP=CheckInputs::checkInputs->valueIsUP(index);
                    ::write(infd,&isUP,sizeof(isUP));
                    ::close(infd);
                }
                break;
                case 0x02:
                {
                    const uint64_t lastDown=CheckInputs::checkInputs->getLastDown(index);
                    ::write(infd,&lastDown,sizeof(lastDown));
                    ::close(infd);
                }
                break;
                default:
                    ::close(infd);
                break;
            }
        }
        else
            ::close(infd);
    }
}

