#include "Domain.h"

awsim::Domain::Domain(const Config::Domain &domain) :
    name(domain.name),
    rootDirectory(domain.rootDirectory)
{

    directoryFd = open(domain.rootDirectory.c_str(),
        O_RDONLY | O_NOATIME | O_DIRECTORY);
    if (directoryFd == -1)
    {
        throw std::runtime_error("Failed to open root directory \""
            + domain.rootDirectory + "\" -> open(\"" + domain.rootDirectory
            + "\", O_RDONLY | O_NOATIME | O_DIRECTORY) failed -> "
            + strerror(errno));
    }

    for (const auto &entry : domain.dynamicPages)
    {
        try
        {
            dynamicPages.insert_dynamic_page(entry.first, entry.second);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to insert dynamic page \""
                + entry.second + "\" -> " + ex.what());
        }
    }

    try
    {
        statusCode403Resource.init(domain.statusCode403Url,
            directoryFd, dynamicPages);
    }
    catch (const Resource::FileForbiddenException &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode403Url + "\" -> File is forbidden");
    }
    catch (const Resource::FileNotFoundException &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode403Url + "\" -> File not found");
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode403Url + "\" -> " + ex.what());
    }

    try
    {
        statusCode404Resource.init(domain.statusCode404Url,
            directoryFd, dynamicPages);
    }
    catch (const Resource::FileForbiddenException &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode404Url + "\" -> File is forbidden");
    }
    catch (const Resource::FileNotFoundException &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode404Url + "\" -> File not found");
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to open resource \""
            + domain.statusCode404Url + "\" -> " + ex.what());
    }
}

awsim::Domain::~Domain()
{
    close(directoryFd);
}

awsim::DynamicPage awsim::Domain::get_dynamic_page(const std::string &url) const
{
    return dynamicPages.get_dynamic_page(url);
}

void awsim::Domain::get_resource(std::string url, Resource &destination) const
{
    destination.init(url, directoryFd, dynamicPages);
}

void awsim::Domain::send_403(HttpRequest *request, Client *client) const
{
    statusCode403Resource.respond(request, client);
}

void awsim::Domain::send_404(HttpRequest *request, Client *client) const
{
    statusCode404Resource.respond(request, client);
}
