#ifndef AWSIM_WORKER_H
#define AWSIM_WORKER_H

#include <atomic>
#include <fcntl.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <sys/sendfile.h>

#include "Client.h"
#include "CriticalException.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ParserDetails.h"
#include "ServerInfo.h"
#include "WorkerAndServerFlags.h"

namespace awsim
{
    class Worker
    {
    public:
        static const uint64_t HTTP_HEADER_BUFFER_SIZE = 8192;
        static const uint64_t INITIAL_MAX_NUMBER_OF_CLIENTS = 128;
        static const uint64_t MAX_NUMBER_OF_EPOLL_EVENTS = 8192;
        static const uint64_t SERVER_PIPE_BUFFER_SIZE = 512;

        static HttpParserSettings httpParserSettings;

        enum class State
        {
            WeakStopPending,
            Running,
            Crashed,
            WeakStopped,
            Stopped
        };

        // Used by both threads
        std::atomic<State> state;

        // Used by server thread
        uint64_t numberOfClients;

        void request_weak_stop();
        uint64_t get_number_of_clients();

        Worker(uint64_t id, ServerInfo &serverInfo);
        ~Worker();

    private:
        // Used by worker thread
        Client *allocatedList;
        int epollfd;
        Client *clientHeap;
        HttpParser httpParser;
        uint64_t id;
        uint64_t maxNumberOfClients;
        uint64_t _numberOfClients;
        ServerInfo &serverInfo;
        int serverReadfd;
        Client *unallocatedList;

        // Used by server thread
        std::thread thread;

        // Used by both threads
        int serverWritefd;

        void accept_http_client();
        void add_http_client(int clientSocket);
        void increase_max_number_of_clients(
            uint64_t maxNumberOfClients);
        void handle_client(Client *client);
        void handle_server_pipe();
        void remove_client(Client *client);
        void remove_remote_host_sockets();
        void request_stop();
        static void routine_start(Worker &worker);
        void routine_loop();
        void stop();
        void weak_stop();
        void write_to_pipe(void *buffer, size_t length);
        void write_to_server(void *buffer, size_t length);
    };
}

#endif
