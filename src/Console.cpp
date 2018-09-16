#include "Console.h"

awsim::Console::Console()
{
    sockaddr_un addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        throw std::runtime_error("Failed to create socket (" + std::string(strerror(errno)) + ")");
    }
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, AWSIM_DEFAULT_CONSOLE_SOCKET_PATH);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error("Failed to connect to \"" + std::string(AWSIM_DEFAULT_CONSOLE_SOCKET_PATH) + "\" (" + strerror(errno) + ")");
    }
}

awsim::Console::~Console()
{
    close(sock);
}
