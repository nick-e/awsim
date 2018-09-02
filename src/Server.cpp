#include "Server.h"

static std::string value_to_string(const rapidjson::Value &value)
{
    if (value.IsBool())
    {
        return std::to_string(value.GetBool());
    }
    if (value.IsDouble())
    {
        return std::to_string(value.GetDouble());
    }
    if (value.IsFloat())
    {
        return std::to_string(value.GetFloat());
    }
    if (value.IsInt())
    {
        return std::to_string(value.GetInt());
    }
    if (value.IsInt64())
    {
        return std::to_string(value.GetInt64());
    }
    if (value.IsNull())
    {
        return "NULL";
    }
    if (value.IsString())
    {
        return value.GetString();
    }
    if (value.IsUint())
    {
        return std::to_string(value.GetUint());
    }
    if (value.IsUint64())
    {
        return std::to_string(value.GetUint64());
    }
    return "empty";
}

static uint16_t config_get_port(rapidjson::Document &document)
{
    uint16_t port = 0;
    rapidjson::Value::ConstMemberIterator it = document.FindMember("port");
    if (it != document.MemberEnd())
    {
        if (it->value.IsUint())
        {
            uint64_t value = it->value.GetUint();
            if (value > 65535)
            {
                throw std::runtime_error("\"port\" must be an integer with a "
                    "value between 0 and 65535, currently set to \""
                    + std::to_string(it->value.GetUint()) + "\"");
            }
            port = (uint16_t)value;
        }
        else
        {
            throw std::runtime_error("\"port\" must be an integer with a value "
                "between 0 and 65535, currently set to \""
                + value_to_string(it->value) + "\"");
        }
    }
    return port;
}

static bool config_get_dynamic_workers(rapidjson::Document &document)
{
    bool dynamicWorkers = HTTP_SERVER_DEFAULT_NUMBER_OF_STATIC_WORKERS;
    rapidjson::Value::ConstMemberIterator it =
        document.FindMember("dynamic workers");
    if (it != document.MemberEnd())
    {
        if (it->value.IsBool())
        {
            dynamicWorkers = it->value.GetBool();
        }
        else
        {
            throw std::runtime_error("\"dynamic workers\" must be a boolean, "
                "currently set to \"" + value_to_string(it->value) + "\"");
        }
    }
    return dynamicWorkers;
}

static uint64_t config_get_number_of_static_workers(
    rapidjson::Document &document)
{
    uint64_t numberOfStaticWorkers =
        HTTP_SERVER_DEFAULT_NUMBER_OF_STATIC_WORKERS;
    rapidjson::Value::ConstMemberIterator it =
        document.FindMember("number of static workers");
    if (it != document.MemberEnd())
    {
        if (it->value.IsUint())
        {
            uint64_t value = it->value.GetUint();
            if (value == 0)
            {
                throw std::runtime_error("\"number of static workers\" must be "
                    "an integer with a value greater than 0, currently set to "
                    "\"" + std::to_string(it->value.GetUint()) + "\"");
            }
            numberOfStaticWorkers = value;
        }
        else
        {
            throw std::runtime_error("\"number of static workers\" must be an "
                "integer with a value greater than 0, currently set to \""
                + value_to_string(it->value) + "\"");
        }
    }
    return numberOfStaticWorkers;
}

static void get_config(const std::string &configFileName,
    uint16_t *port, bool *dynamicWorkers, uint64_t *numberOfStaticWorkers)
{
    FILE *fp = fopen(configFileName.c_str(), "r");
    char buf[2048];
    rapidjson::Document document;
    if (fp == nullptr)
    {
        throw std::runtime_error("Cannot open config file \"" + configFileName
            + "\" (" + strerror(errno) + ")");
    }
    rapidjson::FileReadStream stream(fp, buf, sizeof(buf));
    document.ParseStream(stream);
    *port = config_get_port(document);
    *dynamicWorkers = config_get_dynamic_workers(document);
    *numberOfStaticWorkers = config_get_number_of_static_workers(document);
    fclose(fp);
}

static void get_address(sockaddr_storage *address)
{
    addrinfo hints;
    addrinfo *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int result = getaddrinfo(nullptr, nullptr, &hints, &results);
    if (result)
    {
        throw std::runtime_error("getaddrinfo failed (" + std::string(gai_strerror(result)) + ")");
    }
    memcpy(address, results->ai_addr, results->ai_addrlen);
    freeaddrinfo(results);
}

httpserver::Server::Server(const std::string &configFileName) :
    dynamicWorkers(HTTP_SERVER_DEFAULT_DYNAMIC_WORKERS)
{
    uint16_t port;
    uint64_t numberOfStaticWorkers;
    char addrstr[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
    get_config(configFileName, &port, &dynamicWorkers, &numberOfStaticWorkers);
    get_address(&address);
    listenSocket = socket(address.ss_family, SOCK_STREAM, 0);
    if (listenSocket == -1)
    {
        throw std::runtime_error("Failed to create a remove host socket (" + std::string(strerror(errno)) + ")");
    }
    if (bind(listenSocket, (sockaddr*)&address, sizeof(address)) == -1)
    {
        throw std::runtime_error("Failed to bind socket (" + std::string(strerror(errno)) + ")");
    }

    inet_ntop(address.ss_family, &address, addrstr, sizeof(address));
    std::cout << "Listening on " << addrstr << std::endl;
}

httpserver::Server::~Server()
{
    close(listenSocket);
}