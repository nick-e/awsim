#ifndef AWSIM_HTTPRESPONSE_H
#define AWSIM_HTTPRESPONSE_H

#include <inttypes.h>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/socket.h>

namespace awsim
{
    class HttpResponse
    {
    public:
        enum class StatusCode
        {
            OK_200
        };

        enum class ContentEncoding
        {
            Gzip,
            Deflate
        };

        enum class MimeType
        {
            Application_JSON,
            Application_Javascript,
            Application_Octet_Stream,
            Image_GIF,
            Image_JPEG,
            Image_PNG,
            Image_SVG_Plus_XML,
            Text_CSS,
            Text_HTML,
            Text_Plain,
        };

        HttpResponse(uint8_t httpMajorVersion, uint8_t httpMinorVersion,
            StatusCode statusCode);

        void send_to(int sock) const;
        void set_content_encoding(ContentEncoding contentEncoding);
        void set_content_length(uint64_t contentLength);
        void set_content_type(MimeType contentType);
    private:
        template<class T>
        class Header
        {
        public:
            bool is_set() const;
            T get() const;
            void set(T value);

        private:
            T value;
            bool isSet = false;
        };

        Header<ContentEncoding> contentEncoding;
        Header<uint64_t> contentLength;
        Header<MimeType> contentType;
        uint8_t httpMajorVersion;
        uint8_t httpMinorVersion;
        StatusCode statusCode;
    };
}

#endif
