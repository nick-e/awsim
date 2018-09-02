#include <stdlib.h>
#include <iostream>

#include "Server.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: httpserver <config_file>" << std::endl;
        return EXIT_FAILURE;
    }
    try
    {
        httpserver::Server server(argv[1]);
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}