/**
 *
 *  AdminAuth.cc
 *
 */

#include "AdminAuth.h"

using namespace drogon;

void AdminAuth::doFilter(const HttpRequestPtr &req,
                         FilterCallback &&fcb,
                         FilterChainCallback &&fccb)
{
    //Edit your logic here
    auto token = req->getCookie("admin_token");
    if(token.empty())
    {
        auto resp = drogon::HttpResponse::newRedirectionResponse("/super/r1/LoginP/admin_in");
        fcb(resp);
        return;
    }
    try
    {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{getSecret()})
            .with_issuer("admin");
        verifier.verify(decoded);
        fccb();
        

    }catch (const std::exception& e) {
        // token无效，重定向到登录页
        std::cerr << "JWT验证失败: " << e.what() << std::endl;
        auto resp = drogon::HttpResponse::newRedirectionResponse("/super/r1/LoginP/admin_in");
        fcb(resp);
    }
}
