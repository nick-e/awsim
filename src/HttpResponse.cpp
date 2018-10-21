#include "HttpResponse.h"

template<class T>
bool awsim::HttpResponse::Header<T>::is_set() const
{
    return isSet;
}

template<class T>
T awsim::HttpResponse::Header<T>::get() const
{
    return value;
}

template<class T>
void awsim::HttpResponse::Header<T>::set(T value)
{
    isSet = true;
    this->value = value;
}

awsim::HttpResponse::HttpResponse(uint8_t httpMajorVersion,
    uint8_t httpMinorVersion, StatusCode statusCode) :
    httpMajorVersion(httpMajorVersion),
    httpMinorVersion(httpMinorVersion),
    statusCode(statusCode)
{

}

#define COPY_STR_AND_MOVE_OFFSET(str) \
    memcpy(buf + offset, str, sizeof(str) - 1); \
    offset += sizeof(str) - 1;

#define ADD_NEW_LINE() \
    buf[offset] = '\r'; \
    buf[offset + 1] = '\n'; \
    offset += 2;

void awsim::HttpResponse::send_to(int sock) const
{
    char buf[4096];
    int offset = 0;

    memcpy(buf, "HTTP/", sizeof("HTTP/") - 1);
    buf[sizeof("HTTP/") - 1] = httpMajorVersion + '0';
    buf[sizeof("HTTP/0.") - 1] = httpMinorVersion + '0';
    offset = sizeof("HTTP/0.0\n") - 1;
    if (contentEncoding.is_set())
    {
        COPY_STR_AND_MOVE_OFFSET("Content-Encoding: ")
        ContentEncoding contentEncoding = this->contentEncoding.get();
        switch (contentEncoding)
        {
            case ContentEncoding::Gzip:
                COPY_STR_AND_MOVE_OFFSET("gzip")
                break;
            case ContentEncoding::Deflate:
                COPY_STR_AND_MOVE_OFFSET("deflate")
                break;
        }
        ADD_NEW_LINE()
    }

    if (contentLength.is_set())
    {
        COPY_STR_AND_MOVE_OFFSET("Content-Length: ")
        offset += sprintf(buf + offset, "%" PRIu64, this->contentLength.get());
        ADD_NEW_LINE()
    }

    if (contentType.is_set())
    {
        COPY_STR_AND_MOVE_OFFSET("Content-Type: ")
        MimeType contentType = this->contentType.get();
        switch (contentType)
        {
            case MimeType::Application_JSON:
                COPY_STR_AND_MOVE_OFFSET("application/json")
                break;
            case MimeType::Application_Javascript:
                COPY_STR_AND_MOVE_OFFSET("application/javascript")
                break;
            case MimeType::Application_Octet_Stream:
                COPY_STR_AND_MOVE_OFFSET("application/stream")
                break;
            case MimeType::Image_GIF:
                COPY_STR_AND_MOVE_OFFSET("image/gif")
                break;
            case MimeType::Image_JPEG:
                COPY_STR_AND_MOVE_OFFSET("image/jpeg")
                break;
            case MimeType::Image_PNG:
                COPY_STR_AND_MOVE_OFFSET("image/png")
                break;
            case MimeType::Image_SVG_Plus_XML:
                COPY_STR_AND_MOVE_OFFSET("image/svg+xml")
                break;
            case MimeType::Text_CSS:
                COPY_STR_AND_MOVE_OFFSET("text/css")
                break;
            case MimeType::Text_HTML:
                COPY_STR_AND_MOVE_OFFSET("text/html")
                break;
            case MimeType::Text_Plain:
                COPY_STR_AND_MOVE_OFFSET("text/plain")
                break;
        }
        ADD_NEW_LINE()
    }

    ADD_NEW_LINE()
    if (send(sock, buf, offset, 0) == -1)
    {
        throw std::runtime_error("send(" + std::to_string(sock) + ", buf, "
            + std::to_string(offset) + ", 0) failed -> " + strerror(errno));
    }
}

void awsim::HttpResponse::set_content_encoding(ContentEncoding contentEncoding)
{
    this->contentEncoding.set(contentEncoding);
}

void awsim::HttpResponse::set_content_length(uint64_t contentLength)
{
    this->contentLength.set(contentLength);
}

void awsim::HttpResponse::set_content_type(MimeType contentType)
{
    this->contentType.set(contentType);
}
