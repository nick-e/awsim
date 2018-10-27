#ifndef AWSIM_PARSERDETAILS_H
#define AWSIM_PARSERDETAILS_H

#include <unordered_map>

#include "Domain.h"
#include "DynamicPage.h"
#include "HttpRequest.h"
#include "Resource.h"

namespace awsim
{
    class ParserDetails {
    public:
        class DomainNotFound : public std::exception {};

        HttpRequest::Field currentHeaderField = HttpRequest::Field::Unknown;

        ParserDetails(const std::unordered_map<std::string, Domain> &domains,
            std::unordered_map<std::string, Domain>::iterator localhostDomain);

        void get_resource(const HttpRequest::Value &url,
            const HttpRequest::Value &host);
        void respond(HttpRequest *request, Client *client);

    private:
        bool resourceNotFound = false;
        bool resourceForbidden = false;
        Resource resource;
        const std::unordered_map<std::string, Domain> domains;
        const std::unordered_map<std::string, Domain>::iterator localhostDomain;
        const Domain *domain;
    };
}

#endif
