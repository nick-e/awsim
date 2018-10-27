#ifndef AWSIM_HTTPREQUEST_H
#define AWSIM_HTTPREQUEST_H

#include <vector>

namespace awsim
{
    struct HttpRequest
    {
        static const char *fieldStrings[];

        enum class Field
        {
            Host = 0,
            UserAgent = 1,
            Accept = 2,
            AcceptLanguage = 3,
            AcceptEncoding = 4,
            Connection = 5,
            UpgradeInsecureRequests = 6,
            Unknown = 7
        };

        enum class Method
        {
            DELETE = 0,
            GET = 1,
            HEAD = 2,
            POST = 3,
            PUT = 4,
            CONNECT = 5,
            OPTIONS = 6,
            TRACE = 7,
            COPY = 8,
            LOCK =  9,
            MKCOL = 10,
            MOVE = 11,
            PROPFIND = 12,
            PROPPATCH = 13,
            UNLOCK = 14,
            REPORT = 15,
            MKACTIVITY = 16,
            CHECKOUT = 17,
            MERGE = 18,
            MSEARCH = 19,
            NOTIFY = 20,
            SUBSCRIBE = 21,
            UNSUBSCRIBE = 22,
            PATCH = 23
        };

        struct Value
        {
            const char *buffer = nullptr;
            size_t length = 0;
            bool set = false;
        };

        Method method;
        Value url;
        Value host;
        Value userAgent;
        Value accept;
        Value acceptLanguage;
        Value acceptEncoding;
        Value connection;
        Value upgradeInsecureRequests;
    };
}

#endif
