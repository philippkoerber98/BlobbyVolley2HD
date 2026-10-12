#include "UITranslations.h"
#include "FileRead.h"
#include "tinyxml2.h"
#include <map>
#include <stdexcept>
namespace {
using Catalog=std::map<std::string,std::map<std::string,std::string>>;
Catalog loadCatalog()
{
    Catalog result;
    auto doc=FileRead::readXMLDocument("ui-translations.xml");
    auto* root=doc->FirstChildElement("translations");
    if(!root)throw std::runtime_error("Invalid UI translation catalog");
    for(auto* row=root->FirstChildElement("string");row;row=row->NextSiblingElement("string")){
        const char* key=row->Attribute("english");if(!key)continue;
        for(const char* lang:{"de","en","cs","es","fr","it"}){
            const char* value=row->Attribute(lang);
            if(!value)throw std::runtime_error(std::string("Missing UI translation: ")+lang+" / "+key);
            result[lang][key]=value;
        }
    }
    return result;
}
}
std::string translateUI(const std::string& language,const std::string& english)
{
    static const auto catalog=loadCatalog();
    auto lang=catalog.find(language);if(lang==catalog.end())lang=catalog.find("en");
    auto text=lang->second.find(english);
    return text==lang->second.end()?english:text->second;
}
