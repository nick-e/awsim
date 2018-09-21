#include "Server.h"

void awsim::Server::Worker::Client::init(int clientSocket, bool https)
{
   this->https = https;
   this->sock = clientSocket;
   allocated = true;
}
