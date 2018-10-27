#ifndef AWSIM_RESOURCE_H
#define AWSIM_RESOURCE_H

#include <errno.h>
#include <fcntl.h>
#include <string>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "DynamicPage.h"
#include "DynamicPages.h"
#include "HttpResponse.h"

namespace awsim
{
    class Resource
    {
    public:
        struct FileForbiddenException : public std::exception {};

        struct FileNotFoundException : public std::exception {};

        Resource(const std::string &url, int rootDirectoryFd,
            const DynamicPages &dynamicPages);
        Resource();
        ~Resource();

        void respond(HttpRequest *request, Client *client) const;
        void init(const std::string &url, int rootDirectoryFd,
            const DynamicPages &dynamicPages);
        bool is_static();
    private:
        bool isStatic;
        DynamicPage dynamicPage;
        int staticFileFd;
        size_t staticFileSize;

        void get_static_file(const std::string &url, int rootDirectoryFd);
        void send_static_file(int sock) const;
    };
}

#endif
