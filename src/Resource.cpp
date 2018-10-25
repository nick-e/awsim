#include "Resource.h"

void awsim::Resource::get_static_file(const std::string &url,
    int rootDirectoryFd)
{
    struct stat s;

    staticFileFd = openat(rootDirectoryFd, url.c_str(), 0,
        O_RDONLY | O_NOATIME);
    if (staticFileFd == -1)
    {
        switch (errno)
        {
            case EACCES:
                throw FileForbiddenException();
            case ENOENT:
            case ENOTDIR:
            case EFAULT:
                // File is missing; it may be a dynamic page
                return;
            default:
                throw std::runtime_error("openat("
                    + std::to_string(rootDirectoryFd) + ", \"" + url
                    + "\", 0, O_RDONLY | O_NOATIME) failed -> "
                    + strerror(errno));
        }
    }

    if (fstat(staticFileFd, &s) == -1)
    {
        throw std::runtime_error("fstat(" + std::to_string(staticFileFd)
            + ", &s) failed -> " + strerror(errno));
    }

    staticFileSize = s.st_size;
    isStatic = true;
}

awsim::Resource::Resource(const std::string &url, int rootDirectoryFd,
    DynamicPages &dynamicPages)
{
    get_static_file(url, rootDirectoryFd);
    if (!isStatic)
    {
        dynamicPage = dynamicPages.get_dynamic_page(url);
        if (dynamicPage == nullptr)
        {
            throw FileNotFoundException();
        }
    }
}

awsim::Resource::~Resource()
{
    if (isStatic)
    {
        close(staticFileFd);
    }
}

void awsim::Resource::respond(HttpRequest *request, Client *client)
{
    if (isStatic)
    {
        send_static_file(client->sock);
    }
    else
    {
        dynamicPage(request, client);
    }
}

void awsim::Resource::send_static_file(int sock) {
    HttpResponse response(1, 1, HttpResponse::StatusCode::OK_200);
    response.set_content_length(staticFileSize);
    try
    {
        response.send_to(sock);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to send HTTP response -> ")
            + ex.what());
    }
    size_t sent = 0;
    while (sent < staticFileSize)
    {
        ssize_t tmp = sendfile(sock, staticFileFd, nullptr, staticFileSize);
        if (tmp == -1)
        {
            throw std::runtime_error("sendfile("
                + std::to_string(sock) + ", " + std::to_string(staticFileFd)
                + ", nullptr, " + std::to_string(staticFileSize)
                + ") failed -> " + strerror(errno));
        }
        sent += tmp;
    }
}
