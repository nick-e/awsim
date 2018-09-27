#include "Client.h"

void awsim::Client::init(int clientSocket, bool https)
{
   this->https = https;
   this->sock = clientSocket;
   allocated = true;
}
