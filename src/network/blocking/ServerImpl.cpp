#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <algorithm>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/execute/Command.h>
#include <afina/Storage.h>
//#include <protocol/Parser.h>
//#include <../../protocol/Parser.h>

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorProxy(void *p) {
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try {
        srv->RunAcceptor();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

void *ServerImpl::RunConnectionProxy(void* arguments) {
    struct st_args *args = (struct st_args *)arguments;
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(args->this_ptr);
    try {
        srv->RunConnection(args->client_socket);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    // Setup server parameters BEFORE thread created, that will guarantee
    // variable value visibility
    max_workers = n_workers;
    listen_port = port;

    // The pthread_create function creates a new thread.
    //
    // The first parameter is a pointer to a pthread_t variable, which we can use
    // in the remainder of the program to manage this thread.
    //
    // The second parameter is used to specify the attributes of this new thread
    // (e.g., its stack size). We can leave it NULL here.
    //
    // The third parameter is the function this thread will run. This function *must*
    // have the following prototype:
    //    void *f(void *args);
    //
    // Note how the function expects a single parameter of type void*. We are using it to
    // pass this pointer in order to proxy call to the class member function. The fourth
    // parameter to pthread_create is used to specify this parameter value.
    //
    // The thread we are creating here is the "server thread", which will be
    // responsible for listening on port 23300 for incoming connections. This thread,
    // in turn, will spawn threads to service each incoming connection, allowing
    // multiple clients to connect simultaneously.
    // Note that, in this particular example, creating a "server thread" is redundant,
    // since there will only be one server thread, and the program's main thread (the
    // one running main()) could fulfill this purpose.
    running.store(true);
    if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Server.h
void ServerImpl::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(accept_thread, 0);
}

// See Server.h
void ServerImpl::RunAcceptor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    // Arguments are:
    // - Family: IPv4
    // - Type: Full-duplex stream (reliable)
    // - Protocol: TCP
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    // when the server closes the socket,the connection must stay in the TIME_WAIT state to
    // make sure the client received the acknowledgement that the connection has been terminated.
    // During this time, this port is unavailable to other processes, unless we specify this option
    //
    // This option let kernel knows that we are OK that multiple threads/processes are listen on the
    // same port. In a such case kernel will balance input traffic between all listeners (except those who
    // are closed already)
    int opts = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    // Bind the socket to the address. In other words let kernel know data for what address we'd
    // like to see in the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    // Start listening. The second parameter is the "backlog", or the maximum number of
    // connections that we'll allow to queue up. Note that listen() doesn't block until
    // incoming connections arrive. It just makesthe OS aware that this process is willing
    // to accept connections on this socket (which is bound to a specific IP and port)
    if (listen(server_socket, 5) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    while (running.load()) {
        std::cout << "network debug: waiting for connection..." << std::endl;

        // When an incoming connection arrives, accept it. The call to accept() blocks until
        // the incoming connection arrives
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket accept() failed");
        }

        pthread_t worker;
        st_args args;
        args.this_ptr = this;
        args.client_socket = client_socket;

        if (this->connections.size() < this->max_workers) {
            if (pthread_create (&worker, NULL, ServerImpl::RunConnectionProxy, (void*)(&args)))
                throw std::runtime_error("can't create worker");
            else
                connections.push_back (worker);
        }
        else {
            std::cout << "network debug: max number of connections"<< std::endl;
            close (client_socket);
        }

        for (auto  it = connections.begin(); it != connections.end(); it++) {
            if (pthread_kill (*it,0)) {
                connections.erase (it);
                it--;
            }
        }

        //int status;
        //if (pthread_join (accept_thread, &status)) {
        //    //close (client_socket);
        //    throw std::runtime_error("can't join worker");
        //}
        //else {
        //    connections.erase (std::find (connections.begin(), connections.end(), worker));
        //    close (client_socket);
        //}

        //// TODO: Start new thread and process data from/to connection
        //{
        //    std::string msg = "TODO: start new thread and process memcached protocol instead";
        //    if (send(client_socket, msg.data(), msg.size(), 0) <= 0) {
        //        close(client_socket);
        //        close(server_socket);
        //        throw std::runtime_error("Socket send() failed");
        //    }
        //    close(client_socket);
        //}
    }

    // Cleanup on exit...
    close(server_socket);
}

// See Server.h
void ServerImpl::RunConnection (int client_socket)
{
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    //std::cout << "network debug: RUNCONNECTION" << std::endl;

    int buf_len;
    socklen_t optlen = sizeof(buf_len);
    if (getsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &buf_len, &optlen)) {
        throw std::runtime_error("Socket getsockopt() failed");
    }

    char buf[buf_len + 1];
    std::string unparsed_buf;
    size_t parsed;
    uint32_t body_size;
    //Protocol::Parser pars;
    Protocol::Parser pars;
    bool command = false;
    //std::cout <<  client_socket << std::endl;
    //std::string data;
    std::cout << "buf_len: " <<buf_len << " optlen: " << optlen << std::endl;

    while (running.load())
    {
        std::string out;
        //std::string args;
        int n = recv (client_socket, buf, buf_len, 0);
        if (n <= 0){
            close (client_socket);
            throw std::runtime_error("break connection");
        }
        buf[n] = '\0';//!!!!!!!!!!!!
        unparsed_buf += std::string(buf);
        //std::cout << "buf: "<< buf << " unparsed_buf: "<< unparsed_buf << " parsed: " << parsed << std::endl;
        command = pars.Parse (unparsed_buf.data(), n, parsed);

        unparsed_buf.erase (0, parsed);
        //unparsed_buf = unparsed_buf.substr (parsed, unparsed_buf.size() - parsed);

        //std::cout << "buf: "<< buf << " unparsed_buf: "<< unparsed_buf <<" unparsed_buf.size(): "<< unparsed_buf.size() << " parsed: " << parsed << std::endl;

        if (command){
            char arg_buffer[body_size + 1];
            int m;
            auto com = pars.Build(body_size);

            if (body_size > 0) {
                while (unparsed_buf.size() < body_size) {
                    m = recv (client_socket, buf, buf_len, 0);
                    if (m <= 0) {
                      close(client_socket);
                      throw std::runtime_error("socket recv failed: can't read args");
                    }
                    buf[m] = '\0';//!!!!!!!!!!!
                    unparsed_buf += std::string(buf);
                }
            }
            //std::cout << "buf: "<< buf << " unparsed_buf: "<< unparsed_buf <<" unparsed_buf.size(): "<< unparsed_buf.size() << " parsed: " << parsed << std::endl;

            //std::copy(unparsed_buf.begin(), unparsed_buf.begin() + body_size, args.begin());
            unparsed_buf.copy(arg_buffer, body_size, 0);
            arg_buffer[body_size] = '\0';
            unparsed_buf.erase (0, body_size);
            //std::cout << "args.data(): "<< args.data() << " args.size(): " << args.size() << std::endl;
            //std::cout << "arg_buffer: "<< arg_buffer << " std::string(arg_buffer): " << std::string(arg_buffer) << std::endl;

            com->Execute(*this->pStorage, std::string(arg_buffer), out);
            //com->Execute(*this->pStorage, args, out);
            //std::cout << "out.data(): "<< out.data() << " out.size(): " << out.size() << std::endl;
            if (send (client_socket, out.data(), out.size(), 0) <= 0) {
                close(client_socket);
                throw std::runtime_error("Socket send failed");
            }
            //std::cout<<"command"<<std::endl;
            command = false;
        }
    }
    close (client_socket);
    return;
}

} // namespace Blocking
} // namespace Network
} // namespace Afina
