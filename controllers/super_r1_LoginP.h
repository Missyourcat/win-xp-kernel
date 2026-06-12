#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace super
{
namespace r1
{
class LoginP : public drogon::HttpController<LoginP>
{
  public:
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    // METHOD_ADD(LoginP::get, "/{2}/{1}", Get); // path is /super/r1/LoginP/{arg2}/{arg1}
    // METHOD_ADD(LoginP::your_method_name, "/{1}/{2}/list", Get); // path is /super/r1/LoginP/{arg1}/{arg2}/list
    // ADD_METHOD_TO(LoginP::your_method_name, "/absolute/path/{1}/{2}/list", Get); // path is /absolute/path/{arg1}/{arg2}/list
    
    METHOD_ADD(LoginP::admin, "/admin_in", Get);
    METHOD_ADD(LoginP::login, "/login_in", Post);
    

    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    // void get(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, int p1, std::string p2);
    // void your_method_name(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, double p1, int p2) const;
    void login(
      const HttpRequestPtr& req,
      std::function<void (const HttpResponsePtr &)> &&callback
    ) const;
    void admin(
      const HttpRequestPtr& req,
      std::function<void (const HttpResponsePtr &)> &&callback
    )const;

};
}
}
