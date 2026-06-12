#include "super_r1_MainP.h"

using namespace super::r1;

// Add definition of your processing function here
void MainP::King(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr &)> &&callback
) const
{
    static std::unordered_set<std::string> valid_tokens;

    std::string token = req->getCookie("admin_token");

    std::cout << "t:" << token <<"\n";
    if (token.empty() || valid_tokens.find(token) == valid_tokens.end())
    {}
    else{
        auto resp = HttpResponse::newRedirectionResponse("/super/r1/LoginP/admin_in");
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse("../views/mainpage.html");
    callback(resp);
}