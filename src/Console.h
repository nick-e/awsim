#ifndef AWSIM_CONSOLE
#define AWSIM_CONSOLE

#include <sys/socket.h>
#include <sys/un.h>
#include <stdexcept>
#include <string>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "Shared.h"

namespace awsim
{
    class Console
    {
    public:
        Console();
        ~Console();

    private:
        int sock;
    };
}

#endif
