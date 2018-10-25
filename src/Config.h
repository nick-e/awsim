#ifndef AWSIM_CONFIG_H
#define AWSIM_CONFIG_H

#include <fstream>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"

#include "defaults.h"

namespace awsim
{
    struct Config
    {
        struct Domain
        {
            std::unordered_map<std::string, std::string> dynamicPages;
            std::string name;
            std::string rootDirectory;
            std::string statusCode403Url;
            std::string statusCode404Url;

            Domain(const std::unordered_map<std::string, std::string>
                &dynamicPages,
                const std::string &name,
                const std::string &rootDirectory,
                const std::string &statusCode403Url,
                const std::string &statusCode404Url);
        };

        std::vector<Domain> domains;
        bool dynamicNumberOfWorkers;
        std::string consoleSocketPath;
        uint16_t httpPort;
        uint16_t httpsPort;
        std::string localhostDomainName;
        uint64_t minimumSizeOfLargeFiles;
        uint64_t numberOfWorkers;
        double percentOfCoresForWorkers;
        uint64_t staticNumberOfWorkers;

        Config(const std::string &fileName);
    };
}

#endif
