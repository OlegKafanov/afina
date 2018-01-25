#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Utils.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/epoll.h>
//#include <sys/socket.h>
//#include <sys/types.h>
#include <unistd.h>

#include <afina/execute/Command.h>
#include <protocol/Parser.h>

#include <afina/Storage.h>

#define MAXEVENTS 64

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) {
    // TODO: implementation here
    pStorage = ps;
}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}
// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    //this->OnRun(server_socket);
    //running.store(true);
    running = true;
    this->server_socket = server_socket;
    if (pthread_create(&thread, NULL, Worker::RunProxy, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    //running.store(false);
    running = false;
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    pthread_join(thread, 0);
}

void *Worker::RunProxy(void *p) {
    Worker *server = reinterpret_cast<Worker *>(p);
    try {
        server->OnRun(server->server_socket);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Worker.h
//void Worker::OnRun(void *args) {
void Worker::OnRun(int sfd) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    int s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;

    efd = epoll_create1(0);
    if (efd == -1) {
        throw std::runtime_error("Could not epoll_create");
        std::abort();
    }

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET; //| EPOLLEXCLUSIVE; // (was not declared in this scope)
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        throw std::runtime_error("Could not epoll_ctl");
        std::abort();
    }

    events = (epoll_event*)calloc(MAXEVENTS, sizeof event);

    while(running)
    {
        int n, i;

        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                throw std::runtime_error("epoll error");
                close(events[i].data.fd);
                continue;
            }

            else if (sfd == events[i].data.fd) {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof in_addr;
                    infd = accept(sfd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { //ресурс временно недоступен || блокирующая операция
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            throw std::runtime_error("Could not accept");
                            break;
                        }
                    }

                    s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0) {
                        std::cout<<"Accepted connection on descriptor "<<infd<<" (host=" <<hbuf<< ", port="<<sbuf<<")"<<std::endl;
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */

                    make_socket_non_blocking(infd);

                    //s = make_socket_non_blocking(infd);
                    //if (s == -1)
                    //    abort();

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        throw std::runtime_error("Could not epoll_ctl");
                        std::abort();
                    }
                }
                continue;
            } else {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read(events[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            throw std::runtime_error("Could not read");
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }

                    //FUNCTIONNNNNNNNNNNNNN
                    /* Write the buffer to standard output */

                    Work(buf, events[i].data.fd, count);

                    //s = write(1, buf, count);
                    //if (s == -1) {
                    //    perror("write");
                    //    abort();
                    //}
                    //FUNCTIONNNNNNNNNNNNNN
                }

                if (done) {
                    std::cout<<"Closed connection on descriptor "<<events[i].data.fd<<std::endl;

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close(events[i].data.fd);
                }
            }
        }
    }

    free(events);

    close(sfd);

    //return EXIT_SUCCESS;

    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.
}

void Worker::Work(char *buf_, size_t client_socket, int n)
{

    Protocol::Parser parser;
    parser.Reset();

    size_t parsed = 0;
    bool ready_to_command = false;
    bool stop = false;
    std::string buf;// = std::string(buf_);
    std::cout<<"buf_0:"<<buf<<std::endl;
    buf_[n] = '\0';
    std::string unparsed_buf = std::string(buf_);
    std::string unparsed_buf2 = std::string(buf_);
    while (!stop && unparsed_buf.size() ){

        try {
            ready_to_command = parser.Parse(unparsed_buf2, parsed);
        } catch (std::runtime_error &ex) {

            std::string error = std::string("SERVER_ERROR ") + ex.what() + "\n";
            std::cout << error<< "start: " << unparsed_buf<< " :end"<< std::endl;
            if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                close(client_socket);
                std::cout<<"not send2\n"<<std::endl;
            }
            stop = true;
        }

        unparsed_buf2.erase(0, parsed);

        if(ready_to_command)
        {
            uint32_t body_size = 0;
            auto command = parser.Build(body_size);
            std::cout<< "body_size: "<<body_size<<std::endl;

            if (body_size > 0)
            {
                auto found = unparsed_buf.find_first_of("\r\n");
                unparsed_buf.erase(0, found + 2);

                buf = unparsed_buf.substr(0, body_size);
                std::string out;

                if(!buf.size())
                    break;
                try {
                    command->Execute(*this->pStorage, buf, out);
                    //command->Execute();
                    out += "\r\n";
                    std::cout<<"out:"<<out.data()<<std::endl;
                    if (send(client_socket, out.data(), out.size(), 0) <= 0) {
                        close(client_socket);
                        std::cout<<"not send\n"<<std::endl;
                    }
                } catch (std::runtime_error &ex) {
                    std::string error = std::string("SERVER_ERROR 1 ") + ex.what() + "\n";
                    if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                        close(client_socket);
                        std::cout<<"not send2\n"<<std::endl;
                    }
                    close(client_socket);
                    unparsed_buf = "";

                }
                //stop = true;
                unparsed_buf.erase(0, body_size + 2);
                body_size = 0;
            }

            else //if(!body_size)
            {
                //cut_buf(buf);
                //buf = buf.substr(3, buf.size());
                //body_size = buf.size()-3;
                auto found = unparsed_buf.find_first_of("\r\n");
                buf = unparsed_buf.substr(0, found);
                buf.erase(0,4);

                if(!buf.size())
                    break;
                std::string out;
                try {
                    command->Execute(*this->pStorage, buf, out);
                    out += "\r\n";
                    std::cout<<"out:"<<out.data()<<std::endl;
                    if (send(client_socket, out.data(), out.size(), 0) <= 0) {
                        close(client_socket);
                        std::cout<<"not send\n"<<std::endl;
                    }
                } catch (std::runtime_error &ex) {
                    std::string error = std::string("SERVER_ERROR 2 ") + ex.what() + "\n";
                    if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                        close(client_socket);
                        std::cout<<"not send2\n"<<std::endl;
                    }
                    close(client_socket);
                }
                //stop = true;

                unparsed_buf.erase(0, found + 2);
        }
        }

    }

/*
    while (!stop && unparsed_buf.size() ) {
        //parser.Reset();
        try {
            ready_to_command = parser.Parse(unparsed_buf, parsed);
        } catch (std::runtime_error &ex) {

            std::string error = std::string("SERVER_ERROR 0") + ex.what() + "\n";
            std::cout << error<< "start: " << unparsed_buf<< " :end"<< std::endl;
            if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                close(client_socket);
            }
            close(client_socket);
            //unparsed_buf = "";
        }

        //std::cout<<"buf_1:"<<buf<<std::endl;
        //if (ready_to_command) {
            //return buf;
        //    std::cout<<"READY\n";
        //}

        //buf = unparsed_buf.substr(0, found);



        //buf = buf.substr(parsed, buf.size() - parsed);


        uint32_t body_size = 0;
        auto command = parser.Build(body_size);
        std::cout<<body_size<<std::endl;


        if (body_size)
        {
            auto found = unparsed_buf.find_first_of("\r\n");
            unparsed_buf.erase(0, found + 2);

            buf = unparsed_buf.substr(0, body_size);
            std::string out;

            if(!buf.size())
                break;
            try {
                command->Execute(*this->pStorage, buf.substr(0, body_size), out);
                out += "\r\n";
                std::cout<<"out:"<<out.data()<<std::endl;
                if (send(client_socket, out.data(), out.size(), 0) <= 0) {
                    close(client_socket);
                    std::cout<<"not send\n"<<std::endl;
                }
            } catch (std::runtime_error &ex) {
                std::string error = std::string("SERVER_ERROR 1 ") + ex.what() + "\n";
                if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                    close(client_socket);
                    std::cout<<"not send2\n"<<std::endl;
                }
                close(client_socket);               
                unparsed_buf = "";
                body_size = 0;
            }
            //stop = true;
            unparsed_buf.erase(0, body_size + 2);
        }

        //std::cout<<"parsed :"<<parsed<<std::endl;
        //
        //
        //std::cout<<"buf_1:"<<buf<<std::endl;
        //if (buf.size() < body_size) {
        //    //return prev;
        //}

        //GET
        else //if(!body_size)
        {
            //cut_buf(buf);
            //buf = buf.substr(3, buf.size());
            //body_size = buf.size()-3;
            auto found = unparsed_buf.find_first_of("\r\n");
            buf = unparsed_buf.substr(0, found);
            if(!buf.size())
                break;
            std::string out;
            try {
                command->Execute(*this->pStorage, buf, out);
                out += "\r\n";
                std::cout<<"out:"<<out.data()<<std::endl;
                if (send(client_socket, out.data(), out.size(), 0) <= 0) {
                    close(client_socket);
                    std::cout<<"not send\n"<<std::endl;
                }
            } catch (std::runtime_error &ex) {
                std::string error = std::string("SERVER_ERROR 2 ") + ex.what() + "\n";
                if (send(client_socket, error.data(), error.size(), 0) <= 0) {
                    close(client_socket);
                    std::cout<<"not send2\n"<<std::endl;
                }
                close(client_socket);
            }
            //stop = true;

            unparsed_buf.erase(0, found + 2);
        }

        std::cout<<"unparsed_buf.size(): "<<unparsed_buf.size()<<std::endl;

        if(unparsed_buf.size() < 3)
            unparsed_buf = "";

        std::cout<<"body_size: "<<body_size <<std::endl;

        //buf = buf.substr(body_size, buf.size() - body_size);
        //buf = "";

        std::cout<<"buf: "<<buf<<"  "<< buf.size() <<std::endl;
    }
*/

}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina


//"set foo 0 0 3\r\nwtf\r\nset bar 0 0 3\r\nzzz\r\nget foo bar\r\n"
