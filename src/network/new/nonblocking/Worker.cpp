#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/epoll.h>

#include <afina/execute/Command.h>
#include <protocol/Parser.h>

#include "Utils.h"

#include <sys/types.h>
#include <sys/stat.h>

#define MAXEVENTS 10

namespace Afina {
namespace Network {
namespace NonBlocking {

void *Worker::RunProxy(void *p) {
    Worker *srv = reinterpret_cast<Worker *>(p);
    try {
        srv->OnRun(srv->m_server_socket);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) : running(false), storage(ps) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
}

// See Worker.h
Worker::~Worker() {}

// See Worker.h
void Worker::Start(sockaddr_in &server_addr, int fifo, int fifo_out, std::string fifo_name) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(true);

    m_fifo = fifo;
    m_fifo_out = fifo_out;
    m_fifo_name = fifo_name;

    m_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    int opts = 1;
    if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(m_server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEPORT, &opts, sizeof(opts)) == -1) {
        close(m_server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }
#endif

    if (bind(m_server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        std::cout << errno;
        close(m_server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    make_socket_non_blocking(m_server_socket);
    if (listen(m_server_socket, 5) == -1) {
        close(m_server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    if (pthread_create(&thread, NULL, Worker::RunProxy, this) < 0) {
        throw std::runtime_error("Could not create worker thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(thread, 0);
}

// See Worker.h
void Worker::OnRun(int sfd) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    int s;
    int efd;

    epoll_event event;
    epoll_event events[MAXEVENTS];

    efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create");
        abort();
    }

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        perror("epoll_ctl");
        abort();
    }

    if (m_fifo >= 0) {
        event.data.fd = m_fifo;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl(efd, EPOLL_CTL_ADD, m_fifo, &event);
        if (s == -1) {
            perror("epoll_ctl");
            abort();
        }
    }

    while (running.load()) {
        int n, i;

        n = epoll_wait(efd, events, MAXEVENTS, 10);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP && events[i].data.fd != m_fifo) || (!(events[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                         ready for reading (why were we notified then?) */
                fprintf(stderr, "epoll error\n");
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
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                                     connections. */
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                                    NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n",
                               infd, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                             list of fds to monitor. */
                    make_socket_non_blocking(infd);
                    if (s == -1)
                        abort();

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        perror("epoll_ctl");
                        abort();
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
                    char buf[4096];

                    count = read(events[i].data.fd, buf, sizeof buf);

                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                                 data. So go back to the main loop. */

                        if (events[i].data.fd == m_fifo)
                        {
                            close (m_fifo);

                            mkfifo(m_fifo_name.c_str(), 0666);
                            if ((m_fifo = open(m_fifo_name.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
                            {
                                done = 1;
                                break;
                            }

                            if (m_fifo >= 0) {
                                event.data.fd = m_fifo;
                                event.events = EPOLLIN | EPOLLET;
                                s = epoll_ctl(efd, EPOLL_CTL_ADD, m_fifo, &event);
                                if (s == -1) {
                                    perror("epoll_ctl");
                                    abort();
                                }
                            }
                            std::cout<<"reopend fifo"<<std::endl;

                        }
                        else if (errno != EAGAIN) {
                            perror("read");
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                                 connection. */
                        done = 1;
                        break;
                    }
                    bufers[events[i].data.fd] =
                        ApplyFunc(bufers[events[i].data.fd] + std::string(buf, count), events[i].data.fd);
                }

                if (done) {
                    printf("Closed connection on descriptor %d\n", events[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                             from the set of descriptors which are monitored. */
                    close(events[i].data.fd);
                    bufers[events[i].data.fd] = "";
                }
            }
        }
    }
}

std::string Worker::ApplyFunc(std::string buf_in, int sock) {
    std::string buf = buf_in;

    auto cut_buf = [](std::string &buf) {
        size_t first_symbol = buf.find_first_not_of("\n\r");
        if (first_symbol != std::string::npos)
            buf = buf.substr(first_symbol, buf.size() - first_symbol);
        else
            buf = "";
    };

    std::cout<<"lol here:"<<buf_in<<std::endl;

    while (buf.size()) {

        Protocol::Parser pr;
        pr.Reset();

        size_t parsed = 0;
        bool was_parsed = false;

        cut_buf(buf);

        try {
            was_parsed = pr.Parse(buf, parsed);
        } catch (std::runtime_error &ex) {
            std::cout<<"here_1"<<std::endl;

            std::string error = std::string("SERVER_ERROR ") + ex.what() + "\n";
            if (send(sock, error.data(), error.size(), 0) <= 0) {
                close(sock);
            }
            if (m_fifo_out >= 0) {
                if (send(m_fifo_out, error.data(), error.size(), 0) <= 0) {
                    return "";
                }
            }
            return "";
        }
//        std::cout<<"here_2"<<std::endl;


        if (!was_parsed) {
            return buf;
        }


        uint32_t body_size = 0;
        std::unique_ptr<Execute::Command> command = pr.Build(body_size);

        std::string prev = buf;
        buf = buf.substr(parsed, buf.size() - parsed);

        cut_buf(buf);

        if (buf.size() < body_size) {
            return prev;
        }



        std::string out;
        try {
            command->Execute(*storage, buf.substr(0, body_size), out);
            out += "\r\n";
            if (send(sock, out.data(), out.size(), 0) <= 0) {
                close(sock);
                return "";
            }
            std::cout<<"fifo:"<<m_fifo_out<<std::endl;
            if (m_fifo_out >= 0) {
                if (send(m_fifo_out, out.data(), out.size(), 0) <= 0) {
                    return "";
                }
            }
        } catch (std::runtime_error &ex) {
            std::cout<<"here_1"<<std::endl;
            std::string error = std::string("SERVER_ERROR ") + ex.what() + "\n";
            if (send(sock, error.data(), error.size(), 0) <= 0) {
                close(sock);
            }
            if (m_fifo_out >= 0) {
                if (send(m_fifo_out, error.data(), error.size(), 0) <= 0) {
                    return "";
                }
            }
            return "";
        }



        buf = buf.substr(body_size, buf.size() - body_size);

        cut_buf(buf);
        std::cout<<"buf:"<<buf<<std::endl;
    }

    return buf;
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
