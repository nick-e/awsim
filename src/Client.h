#ifndef AWSIM_CLIENT_H
#define AWSIM_CLIENT_H

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

        void init(int clientSocket, bool https);
    };
}

#endif
