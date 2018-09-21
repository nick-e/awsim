#ifndef AWSIM_SERVER_H
#define AWSIM_SERVER_H

#include <atomic>
#include <arpa/inet.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <list>
#include <math.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"

#include "flags.h"
#include "defaults.h"
#include "HttpParser.h"
#include "HttpRequest.h"

#define AF_FAMILY_TO_STR(a) ((a) == AF_INET ? "AF_INET" : (a) == AF_INET6 \
    ? "AF_INET6" : (a) == AF_UNIX ? "AF_UNIX" : "UNKNOWN")
#define SOCKTYPE_TO_STR(a) ((a) == SOCK_STREAM ? "SOCK_STREAM" \
    : (a) == SOCK_DGRAM ? "SOCK_DGRAM" : "UNKNOWN")
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define INSERT_FLAG(flag, buffer) (((uint16_t*)(buffer))[0] = (uint16_t)(flag))
#define NO_FLAGS 0

namespace awsim
{
    class Server
    {
    public:
        static const std::string CONFIG_FILE_PATH;
        static const uint64_t CONSOLE_BACKLOG = 32;
        static const uint64_t CONSOLE_BUFFER_SIZE = 1024;
        static const uint64_t MAX_ROOT_DIRECTORY_LENGTH = 1024;
        static const uint64_t NUMBER_OF_EPOLL_EVENTS = 64;
        static const uint64_t INTERNET_BACKLOG = 1024;

        static void init();

    private:
        struct Config
        {
            uint16_t httpPort;
            uint16_t httpsPort;
            uint64_t numberOfWorkers;
            bool dynamicNumberOfWorkers;
            uint64_t staticNumberOfWorkers;
            double percentOfCoresForWorkers;
            char rootDirectory[MAX_ROOT_DIRECTORY_LENGTH + 1];

            Config(const std::string &fileName);
        };

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

                Worker(uint64_t id);
                ~Worker();

            private:
                class CriticalException : public std::runtime_error
                {
                public:
                    CriticalException(const std::string &message);
                };

                class Client
                {
                public:
                    bool allocated;
                    bool https;
                    int sock;

                    void init(int clientSocket, bool https);

                    Client *next;
                    Client *prev;
                };

                // Used by worker thread
                Client *clientHeap;
                Client *allocatedList;
                Client *unallocatedList;
                uint64_t maxNumberOfClients;
                uint64_t _numberOfClients;
                int epollfd;
                int serverReadfd;
                uint64_t id;
                HttpParser httpParser;

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
        };

        struct WorkersPipeHeader
        {
            uint64_t workerID;
            WorkerToServerFlags flag;
        };

        static sockaddr_storage address;
        static int consoleSocket;
        static std::string consoleSocketPath;
        static bool dynamicNumberOfWorkers;
        static int epollfd;
        static int httpSocket;
        static uint64_t nextWorkerID;
        static uint64_t numberOfConnectedConsoles;
        static uint64_t numberOfWorkers;
        static double percentOfCoresForWorkers;
        static bool quit;
        static char rootDirectory[MAX_ROOT_DIRECTORY_LENGTH + 1];
        static int signalsfd;
        static uint64_t staticNumberOfWorkers;
        static std::unordered_map<uint64_t, Worker> workers;
        static int workersReadfd;
        static int workersWritefd;

        static void accept_console();
        static void add_worker();
        static void end();
        static void end_workers();
        static bool handle_console(int consoleSocket);
        static bool handle_signal();
        static void handle_workers();
        static void loop();
        static void restart();
        static void start();
        static void start_workers();
        static void stop();
        static void write_to_pipe(void *buffer, size_t length);
    };
};

#endif
