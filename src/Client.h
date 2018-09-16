#ifndef AWSIM_CLIENT_H
#define AWSIM_CLIENT_H

#include <unistd.h>

namespace awsim
{
    class Client
    {
    public:
        bool allocated;
        bool https;
        int sock;

        Client *next;
        Client *prev;
    };
};

#endif
