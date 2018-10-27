#include "ParserDetails.h"

awsim::ParserDetails::ParserDetails(
   const std::unordered_map<std::string, Domain> &domains,
   std::unordered_map<std::string, Domain>::iterator localhostDomain) :
   domains(domains),
   localhostDomain(localhostDomain)
{

}

void awsim::ParserDetails::get_resource(const HttpRequest::Value &url,
   const HttpRequest::Value &host)
{
   std::string hostString = std::string(host.buffer, host.length);
   const auto &it = domains.find(hostString);
   if (it == domains.end())
   {
      if (hostString == "localhost")
      {
         domain = &(localhostDomain->second);
      }
      else
      {
         throw DomainNotFound();
      }
   }
   else
   {
      domain = &(it->second);
   }

   try
   {
      domain->get_resource(std::string(url.buffer, url.length), resource);
   }
   catch (const Resource::FileForbiddenException &ex)
   {
      resourceNotFound = true;
   }
   catch (const Resource::FileNotFoundException &ex)
   {
      resourceForbidden = true;
   }

}

void awsim::ParserDetails::respond(HttpRequest *request, Client *client)
{
   if (resourceNotFound)
   {
      domain->send_404(request, client);
   }
   else if (resourceForbidden)
   {
      domain->send_403(request, client);
   }
}
