#ifndef HTTP_SERVER_SERVER
#define HTTP_SERVER_SERVER

#include <string>
#include <sys/socket.h>
#include <fstream>
#include <netdb.h>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>

#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

#define HTTP_SERVER_DEFAULT_NUMBER_OF_STATIC_WORKERS 4
#define HTTP_SERVER_DEFAULT_DYNAMIC_WORKERS false
#define MAX(a,b) ((a) > (b) ? (a) : (b))

namespace httpserver
{
    class Server
    {
    public:
        Server(const std::string &configFileName);
        ~Server();

    private:
        bool dynamicWorkers;
        sockaddr_storage address;
        int listenSocket;
    };
};

#endif