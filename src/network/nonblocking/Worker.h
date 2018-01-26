#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

//#include <atomic>

#include <memory>
#include <pthread.h>

#include <sys/uio.h>
#include <iostream>
#include <string.h>
#include <utility>
#include <string>
#include <sys/uio.h>
#include <unistd.h>

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {

/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */
class Worker {
public:
    static void *RunProxy(void *p);

    Worker(std::shared_ptr<Afina::Storage> ps);
    ~Worker();

    //static int Worker::make_socket_non_blocking(int sfd);
    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();


    void Work(char *buf, size_t, int n);

protected:
    /**
     * Method executing by background thread
     */
    //void OnRun(void *args);
    void OnRun(int sfd);

private:
    pthread_t thread;
    std::shared_ptr<Afina::Storage> pStorage;
    bool running;
    int server_socket;
    //    std::atomic<bool> running;

};


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
