#include "super_r1_LoginP.h"
#include <iostream>      // std::cerr, std::cout 打印日志
#include <sstream>       // std::stringstream 字符串拼接
#include <iomanip>       // std::setw, std::setfill 格式化输出（补零）
#include <openssl/sha.h> // SHA256 加密算法
#include <jwt-cpp/jwt.h> // JWT 生成和验证
#include "models/AdminUser.h"

static std::string get_jwt_secret() {
    auto secret = getenv("JWT_SECRET");
    return secret ? secret : "your_secret_key_change_in_production";
}

static std::string generate_jwt(const std::string& username, const std::string& role) {
    return jwt::create()
        .set_issuer("admin")
        .set_type("JWS")
        .set_payload_claim("username", jwt::claim(username))
        .set_payload_claim("role", jwt::claim(role))
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(2))
        .sign(jwt::algorithm::hs256{get_jwt_secret()});
}

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
    // Json::Value  ret;
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
        
        drogon::orm::Mapper<drogon_model::win_xp_db::AdminUser> mapper(db);
        mapper.findOne(
            drogon::orm::Criteria(
                drogon_model::win_xp_db::AdminUser::Cols::_username,
                drogon::orm::CompareOperator::EQ,
                username
            ),
            [callback,password](drogon_model::win_xp_db::AdminUser user){
                Json::Value ret;
                std::string db_hash = user.getValueOfPasswordHash();
                std::string role = user.getValueOfRole();
                // 验证密码
                if (sha256(password) != db_hash) {
                    json_response(callback, 403, "密码错误");
                    return;
                }
                std::string token = generate_jwt(user.getValueOfUsername(), role);
                 // 构建响应
                ret["code"] = 200;
                ret["msg"] = "login success";
                ret["role"] = role;
                ret["token"] = token;
                
                auto resp = drogon::HttpResponse::newHttpJsonResponse(ret);
                drogon::Cookie cookie("admin_token", token);
                cookie.setHttpOnly(true);
                cookie.setPath("/");
                cookie.setMaxAge(7200);  // 2小时
                resp->addCookie(std::move(cookie));
                callback(resp);
            },
            [callback](const drogon::orm::DrogonDbException &e) {
            // 判断是否是用户不存在
            std::string errMsg = e.base().what();
            if (errMsg.find("not found") != std::string::npos || 
                errMsg.find("empty") != std::string::npos) {
                json_response(callback, 401, "用户不存在");
            } else {
                json_response(callback, 500, "数据库错误: " + errMsg);
            }
        }
        );
        // db->execSqlAsync(
        //     "SELECT id, username, password_hash, role FROM admin_user WHERE username=?",
        //     [callback, password, username](const drogon::orm::Result &r)
        //     {
        //         Json::Value ret;

        //         if (r.empty())
        //         {
        //             json_response(callback, 401 , "用户不存在");
        //             return;
        //         }

        //         auto row = r[0];

        //         std::string db_hash = row["password_hash"].as<std::string>();
        //         std::string role = row["role"].as<std::string>();

        //         if (sha256(password) != db_hash)
        //         {
        //             json_response(callback, 403, "密码错误");
        //             return;
        //         }

        //         // 登录成功
        //         std::string token = generate_jwt(username, role);

        //         ret["code"] = 200;
        //         ret["msg"] = "login success";
        //         ret["role"] = role;
        //         ret["token"] = token;
        //         // std::cout << "a t:" << token <<"\n";
        //         auto resp = drogon::HttpResponse::newHttpJsonResponse(ret);
        //         // 设置 Cookie（HttpOnly 更安全）
        //         drogon::Cookie cookie("admin_token", token);
        //         cookie.setHttpOnly(true);
        //         cookie.setPath("/");
        //         cookie.setMaxAge(7200);  // 2小时
        //         resp->addCookie(std::move(cookie));

        //         callback(resp);
        //     },
        //     [callback](const drogon::orm::DrogonDbException &e)
        //     {

        //         json_response(callback, 500, e.base().what());
        //     },
        //     username
        // );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }

}