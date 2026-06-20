#include "tof_r1_api.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <jwt-cpp/jwt.h>
#include "models/WinUser.h"
#include "models/DesktopIcon.h"
#include "models/Category.h"
#include "models/FileResource.h"

static std::string url_decode(const std::string &src)
{
    std::string result;
    result.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i)
    {
        if (src[i] == '%' && i + 2 < src.size())
        {
            int hi = src[i + 1];
            int lo = src[i + 2];
            if (hi >= '0' && hi <= '9') hi -= '0';
            else if (hi >= 'a' && hi <= 'f') hi -= 'a' - 10;
            else if (hi >= 'A' && hi <= 'F') hi -= 'A' - 10;
            else { result += src[i]; continue; }
            if (lo >= '0' && lo <= '9') lo -= '0';
            else if (lo >= 'a' && lo <= 'f') lo -= 'a' - 10;
            else if (lo >= 'A' && lo <= 'F') lo -= 'A' - 10;
            else { result += src[i]; continue; }
            result += static_cast<char>((hi << 4) | lo);
            i += 2;
        }
        else
        {
            result += src[i];
        }
    }
    return result;
}

using namespace tof::r1;

static std::string get_jwt_secret()
{
    auto secret = getenv("JWT_SECRET");
    return secret ? secret : "your_secret_key_change_in_production";
}

static std::string sha256(const std::string &str)
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

static std::string generate_jwt(const std::string& email)
{
    return jwt::create()
        .set_issuer("tof")
        .set_type("JWS")
        .set_payload_claim("email", jwt::claim(email))
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
        .sign(jwt::algorithm::hs256{get_jwt_secret()});
}

static void json_response(
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

void api::registerUser(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            json_response(callback, 400, "JSON错误");
            return;
        }
        if (!(*json).isMember("user_name") || !(*json)["user_name"].isString() ||
            !(*json).isMember("user_email") || !(*json)["user_email"].isString() ||
            !(*json).isMember("user_password") || !(*json)["user_password"].isString())
        {
            json_response(callback, 400, "缺少必填字段(user_name, user_email, user_password)");
            return;
        }

        std::string name = (*json)["user_name"].asString();
        std::string email = (*json)["user_email"].asString();
        std::string password = sha256((*json)["user_password"].asString());

        drogon_model::win_xp_db::WinUser user;
        user.setUserName(name);
        user.setUserEmail(email);
        user.setUserPassword(password);
        if ((*json).isMember("user_icon") && (*json)["user_icon"].isString())
        {
            user.setUserIcon((*json)["user_icon"].asString());
        }

        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::win_xp_db::WinUser> mapper(db);
        mapper.insert(user,
            [callback](const drogon_model::win_xp_db::WinUser &newUser)
            {
                Json::Value data;
                data["user_id"] = newUser.getValueOfUserId();
                data["user_name"] = newUser.getValueOfUserName();
                data["user_email"] = newUser.getValueOfUserEmail();
                if (newUser.getUserIcon())
                    data["user_icon"] = newUser.getValueOfUserIcon();
                json_response(callback, 200, "注册成功", data);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                std::string err = e.base().what();
                json_response(callback, 500, "注册失败: " + err);
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::login(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            json_response(callback, 400, "JSON错误");
            return;
        }
        if (!(*json).isMember("user_email") || !(*json)["user_email"].isString() ||
            !(*json).isMember("user_password") || !(*json)["user_password"].isString())
        {
            json_response(callback, 400, "缺少必填字段(user_email, user_password)");
            return;
        }

        std::string email = (*json)["user_email"].asString();
        std::string password = (*json)["user_password"].asString();

        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::win_xp_db::WinUser> mapper(db);
        mapper.findOne(
            drogon::orm::Criteria(
                drogon_model::win_xp_db::WinUser::Cols::_user_email,
                drogon::orm::CompareOperator::EQ,
                email
            ),
            [callback, password](const drogon_model::win_xp_db::WinUser &user)
            {
                std::string dbHash = user.getValueOfUserPassword();
                if (sha256(password) != dbHash)
                {
                    json_response(callback, 403, "密码错误");
                    return;
                }
                std::string token = generate_jwt(user.getValueOfUserEmail());

                Json::Value data;
                data["token"] = token;
                data["user_id"] = user.getValueOfUserId();
                data["user_name"] = user.getValueOfUserName();
                data["user_email"] = user.getValueOfUserEmail();
                if (user.getUserIcon())
                    data["user_icon"] = user.getValueOfUserIcon();

                json_response(callback, 200, "登录成功", data);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                std::string errMsg = e.base().what();
                if (errMsg.find("not found") != std::string::npos ||
                    errMsg.find("empty") != std::string::npos)
                {
                    json_response(callback, 401, "用户不存在");
                }
                else
                {
                    json_response(callback, 500, "数据库错误: " + errMsg);
                }
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::getDesktopIcons(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::win_xp_db::DesktopIcon> mapper(db);
        mapper.findAll(
            [callback](const std::vector<drogon_model::win_xp_db::DesktopIcon> &icons)
            {
                Json::Value list(Json::arrayValue);
                for (const auto &icon : icons)
                {
                    Json::Value item;
                    item["desktop_id"] = icon.getValueOfDesktopId();
                    item["desktop_name"] = icon.getValueOfDesktopName();
                    if (icon.getDesktopImage())
                        item["desktop_image"] = icon.getValueOfDesktopImage();
                    if (icon.getDesktopUrl())
                        item["desktop_url"] = icon.getValueOfDesktopUrl();
                    list.append(item);
                }
                Json::Value extra;
                extra["data"] = list;
                json_response(callback, 200, "ok", extra);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                json_response(callback, 500, "数据库错误: " + std::string(e.base().what()));
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::getCategories(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::win_xp_db::Category> mapper(db);
        mapper.findAll(
            [callback](const std::vector<drogon_model::win_xp_db::Category> &categories)
            {
                Json::Value list(Json::arrayValue);
                for (const auto &cat : categories)
                {
                    Json::Value item;
                    item["id"] = cat.getValueOfId();
                    item["name"] = cat.getValueOfName();
                    if (cat.getParentId())
                        item["parent_id"] = cat.getValueOfParentId();
                    if (cat.getPath())
                        item["path"] = cat.getValueOfPath();
                    if (cat.getSort())
                        item["sort"] = cat.getValueOfSort();
                    list.append(item);
                }
                Json::Value extra;
                extra["data"] = list;
                json_response(callback, 200, "ok", extra);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                json_response(callback, 500, "数据库错误: " + std::string(e.base().what()));
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::getFiles(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto query = req->getParameters();
        auto it = query.find("category_id");
        if (it == query.end())
        {
            json_response(callback, 400, "缺少category_id参数");
            return;
        }
        int64_t categoryId = std::stoll(it->second);
        if (categoryId <= 0)
        {
            json_response(callback, 400, "无效的category_id");
            return;
        }

        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::win_xp_db::FileResource> mapper(db);
        mapper.findBy(
            drogon::orm::Criteria(
                drogon_model::win_xp_db::FileResource::Cols::_category_id,
                drogon::orm::CompareOperator::EQ,
                categoryId
            ),
            [callback](const std::vector<drogon_model::win_xp_db::FileResource> &files)
            {
                Json::Value list(Json::arrayValue);
                for (const auto &f : files)
                {
                    Json::Value item;
                    item["id"] = f.getValueOfId();
                    item["category_id"] = f.getValueOfCategoryId();
                    item["file_name"] = f.getValueOfFileName();
                    if (f.getFileExt())
                        item["file_ext"] = f.getValueOfFileExt();
                    item["relative_path"] = f.getValueOfRelativePath();
                    if (f.getAbsolutePath())
                        item["absolute_path"] = f.getValueOfAbsolutePath();
                    if (f.getFileSize())
                        item["file_size"] = f.getValueOfFileSize();
                    if (f.getCreatedAt())
                        item["created_at"] = f.getValueOfCreatedAt().toDbStringLocal();
                    list.append(item);
                }
                Json::Value extra;
                extra["data"] = list;
                json_response(callback, 200, "ok", extra);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                json_response(callback, 500, "数据库错误: " + std::string(e.base().what()));
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::downloadFile(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto query = req->getParameters();
        auto it = query.find("path");
        if (it == query.end())
        {
            json_response(callback, 400, "缺少path参数");
            return;
        }
        std::string relativePath = url_decode(it->second);
        if (relativePath.find("..") != std::string::npos)
        {
            json_response(callback, 400, "无效的路径");
            return;
        }

        const char *fd = getenv("FILES_DIR");
        std::string filesDir = fd ? fd : "../files";
        std::string fullPath = filesDir + "/" + relativePath;

        auto resp = HttpResponse::newFileResponse(fullPath);
        std::string filename = relativePath;
        auto pos = filename.rfind('/');
        if (pos != std::string::npos)
            filename = filename.substr(pos + 1);
        resp->addHeader("Content-Disposition", "attachment; filename=/" + filename + "/");
        callback(resp);
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}
