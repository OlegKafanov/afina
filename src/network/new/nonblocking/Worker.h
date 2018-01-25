#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <memory>
#include <pthread.h>
#include <atomic>
#include <map>
#include <string>

#include <netinet/in.h>


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
    Worker(std::shared_ptr<Afina::Storage> ps);

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker& operator=(const Worker&) volatile = delete;

    ~Worker();

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(sockaddr_in &server_addr, int fifo = -1, int fifo_out = -1, std::string fifo_name = "");


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

    static void *RunProxy (void *p);

    std::string ApplyFunc(std::string buf_in, int sock);

protected:
    /**
     * Method executing by background thread
     */
    void OnRun(int sfd);

private:
    pthread_t thread;

//    bool running;
    std::atomic<bool> running;

    std::shared_ptr<Afina::Storage> storage;
    int m_server_socket;

    int m_fifo = -1;
    int m_fifo_out = -1;
    std::string m_fifo_name = "";

    std::map <int, std::string> bufers;
};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
