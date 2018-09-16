#include <stdlib.h>
#include <iostream>

#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "Server.h"

int main()
{
    pid_t pid;

    pid = fork();
    if (pid == -1)
    {
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        pid_t sid;

        umask(0);
        openlog("awsimd", LOG_CONS | LOG_PID, LOG_DAEMON);
        sid = setsid();
        if (sid < 0)
        {
            syslog(LOG_EMERG, "setsid() failed. %s\n", strerror(errno));
            closelog();
            return EXIT_FAILURE;
        }
        try
        {
            awsim::Server::init();
        }
        catch (const std::exception &ex)
        {
            syslog(LOG_EMERG, "Crashed. %s", ex.what());
            closelog();
            return EXIT_FAILURE;
        }

        closelog();
    }
    return EXIT_SUCCESS;
}
