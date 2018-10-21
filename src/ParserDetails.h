#ifndef AWSIM_PARSERDETAILS_H
#define AWSIM_PARSERDETAILS_H

#include <unordered_map>

#include "Domain.h"
#include "DynamicPage.h"
#include "HttpRequest.h"

namespace awsim
{
    class ParserDetails {
    public:
        const std::unordered_map<std::string, Domain> domains;

        ParserDetails(const std::unordered_map<std::string, Domain> &domains);

        bool domain_proven_missing() const;
        bool resource_proven_missing() const;
        bool resource_proven_static() const;
        HttpRequest::Field get_current_header_field() const;
        DynamicPage get_resource_dynamic_page() const;
        void set_current_header_field(HttpRequest::Field headerField);
        void set_domain_proven_missing();
        void set_resource_dynamic_page(DynamicPage dynamicPage);
        void set_resource_proven_missing();
        void set_resource_proven_static();

    private:
        HttpRequest::Field currentHeaderField = HttpRequest::Field::Unknown;

        bool domainProvenMissing = false;
        DynamicPage resourceDynamicPage = nullptr;
        bool resourceProvenMissing = false;
        bool resourceProvenStatic = false;
        int staticFileFd = -1;
    };
}

#endif
