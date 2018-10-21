#include "ParserDetails.h"

awsim::ParserDetails::ParserDetails(
  const std::unordered_map<std::string, Domain> &domains) :
  domains(domains)
{

}

bool awsim::ParserDetails::domain_proven_missing() const
{
  return domainProvenMissing;
}

bool awsim::ParserDetails::resource_proven_missing() const
{
  return resourceProvenMissing;
}

bool awsim::ParserDetails::resource_proven_static() const
{
  return resourceProvenStatic;
}

awsim::HttpRequest::Field awsim::ParserDetails::get_current_header_field() const
{
  return currentHeaderField;
}

awsim::DynamicPage awsim::ParserDetails::get_resource_dynamic_page() const
{
  return resourceDynamicPage;
}

void awsim::ParserDetails::set_current_header_field(
    HttpRequest::Field headerField)
{
  currentHeaderField = headerField;
}

void awsim::ParserDetails::set_domain_proven_missing()
{
  domainProvenMissing = true;
}

void awsim::ParserDetails::set_resource_dynamic_page(DynamicPage dynamicPage)
{
  resourceDynamicPage = dynamicPage;
}

void awsim::ParserDetails::set_resource_proven_missing()
{
  resourceProvenMissing = true;
}

void awsim::ParserDetails::set_resource_proven_static()
{
  resourceProvenStatic = true;
}
