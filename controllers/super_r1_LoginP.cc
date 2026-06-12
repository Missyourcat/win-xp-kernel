#include "super_r1_LoginP.h"
#include <iostream>
// #include <memory>
#include <openssl/sha.h>
std::string sha256(const std::string &str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)str.c_str(), str.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << (int)hash[i];
    }
    return ss.str();
}


using namespace super::r1;

// Add definition of your processing function here
void LoginP::admin(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
)const
{
    Json::Value  ret;
    try
    {
        auto resp = HttpResponse::newFileResponse("../views/admin.html");
        callback(resp);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }    

}

inline void json_response(
        const std::function<void(const drogon::HttpResponsePtr &)> &callback,
        int code,
        const std::string &msg,
        const Json::Value &extra = Json::Value()
    )
    {
        Json::Value ret = extra;
        ret["code"] = code;
        ret["msg"] = msg;

        callback(drogon::HttpResponse::newHttpJsonResponse(ret));
    }
void LoginP::login(
  const HttpRequestPtr& req,
  std::function<void (const HttpResponsePtr &)> &&callback
) const
{ 
    try
    {
        auto json = req->getJsonObject();
        if(!json)
        {
            json_response(callback, 400 , "JSON错误");
            return;
        }
        if(!(*json).isMember("username") || !(*json)["username"].isString() || !(*json).isMember("password") || !(*json)["password"].isString()) 
        {
            json_response(callback, 400,"无效账号密码");
            return;
        }
        std::string username = (*json)["username"].asString();
        std::string password = (*json)["password"].asString();
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT id, username, password_hash, role FROM admin_user WHERE username=?",
            [callback, password](const drogon::orm::Result &r)
            {
                Json::Value ret;

                if (r.empty())
                {
                    json_response(callback, 401 , "用户不存在");
                    return;
                }

                auto row = r[0];

                std::string db_hash = row["password_hash"].as<std::string>();
                std::string role = row["role"].as<std::string>();

                if (sha256(password) != db_hash)
                {
                    json_response(callback, 403, "密码错误");
                    return;
                }

                // 登录成功
                std::string token = drogon::utils::getUuid();

                ret["code"] = 200;
                ret["msg"] = "login success";
                ret["role"] = role;
                ret["token"] = token;
                std::cout << "a t:" << token <<"\n";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(ret);
                drogon::Cookie cookie("admin_token", token);
                resp->addCookie(std::move(cookie));

                callback(resp);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                Json::Value ret;
                json_response(callback, 500, e.base().what());
            },
            username
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }

}