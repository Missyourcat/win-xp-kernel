#include "tof_r1_api.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <openssl/sha.h>
#include <jwt-cpp/jwt.h>
#include <sys/utsname.h>

#include "models/WinUser.h"
#include "models/DesktopIcon.h"
#include "models/Category.h"
#include "models/FileResource.h"
#include "models/ArticleCategory.h"
#include "models/ArticleMd.h"

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
using namespace drogon_model::win_xp_db;

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
        resp->addHeader("Content-Disposition", "attachment; filename=" + filename);
        callback(resp);
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::getSystemInfo(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        Json::Value data;

        struct utsname uts;
        if (uname(&uts) == 0)
        {
            data["os_name"] = uts.sysname;
            data["os_release"] = uts.release;
            data["os_version"] = uts.version;
            data["machine"] = uts.machine;
            data["hostname"] = uts.nodename;
        }

        std::ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open())
        {
            std::string line;
            int processorCount = 0;
            std::string modelName;
            while (std::getline(cpuinfo, line))
            {
                if (line.rfind("processor", 0) == 0) processorCount++;
                if (line.rfind("model name", 0) == 0)
                {
                    auto colon = line.find(':');
                    if (colon != std::string::npos)
                        modelName = line.substr(colon + 2);
                }
            }
            data["cpu_count"] = processorCount;
            data["cpu_model"] = modelName;
        }

        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open())
        {
            std::string line;
            while (std::getline(meminfo, line))
            {
                if (line.rfind("MemTotal:", 0) == 0)
                {
                    auto colon = line.find(':');
                    auto valStr = line.substr(colon + 1);
                    data["mem_total_kb"] = (Json::Int64)std::stoll(valStr);
                }
                if (line.rfind("MemAvailable:", 0) == 0)
                {
                    auto colon = line.find(':');
                    auto valStr = line.substr(colon + 1);
                    data["mem_available_kb"] = (Json::Int64)std::stoll(valStr);
                }
            }
        }

        std::string clientIp = req->getPeerAddr().toIp();
        auto forwarded = req->getHeader("X-Forwarded-For");
        if (!forwarded.empty())
        {
            auto comma = forwarded.find(',');
            clientIp = forwarded.substr(0, comma);
        }
        data["client_ip"] = clientIp;

        Json::Value extra;
        extra["data"] = data;
        json_response(callback, 200, "ok", extra);
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

// ============ 公开文章浏览接口 ============

void api::getArticleCategories(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<ArticleCategory> mapper(db);
        mapper.findBy(
            drogon::orm::Criteria(
                ArticleCategory::Cols::_id,
                drogon::orm::CompareOperator::GT,
                0
            ),
            [callback](const std::vector<ArticleCategory> &cats)
            {
                Json::Value list(Json::arrayValue);
                for (const auto &c : cats)
                {
                    Json::Value item;
                    item["id"] = c.getValueOfId();
                    item["name"] = c.getValueOfName();
                    if (c.getDescription())
                        item["description"] = c.getValueOfDescription();
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

void api::getArticles(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    try
    {
        auto db = drogon::app().getDbClient();
        auto query = req->getParameters();
        auto catIt = query.find("category_id");

        drogon::orm::Mapper<ArticleMd> mapper(db);
        auto criteria = drogon::orm::Criteria(
            ArticleMd::Cols::_status,
            drogon::orm::CompareOperator::EQ,
            1
        );
        if (catIt != query.end())
        {
            int64_t catId = std::stoll(catIt->second);
            criteria = criteria && drogon::orm::Criteria(
                ArticleMd::Cols::_category_id,
                drogon::orm::CompareOperator::EQ,
                catId
            );
        }

        mapper.findBy(
            criteria,
            [callback](const std::vector<ArticleMd> &articles)
            {
                Json::Value list(Json::arrayValue);
                for (const auto &a : articles)
                {
                    Json::Value item;
                    item["id"] = a.getValueOfId();
                    item["category_id"] = a.getValueOfCategoryId();
                    item["title"] = a.getValueOfTitle();
                    if (a.getSummary())
                        item["summary"] = a.getValueOfSummary();
                    item["view_count"] = (Json::Int64)a.getValueOfViewCount();
                    if (a.getCreatedAt())
                        item["created_at"] = a.getValueOfCreatedAt().toDbStringLocal();
                    if (a.getUpdatedAt())
                        item["updated_at"] = a.getValueOfUpdatedAt().toDbStringLocal();

                    auto kw = a.getKeywords();
                    if (kw)
                    {
                        Json::Value karr(Json::arrayValue);
                        Json::Reader reader;
                        Json::Value parsed;
                        if (reader.parse(*kw, parsed) && parsed.isArray())
                            item["keywords"] = parsed;
                        else
                            item["keywords"] = Json::arrayValue;
                    }
                    else
                    {
                        item["keywords"] = Json::arrayValue;
                    }

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

void api::getArticle(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string id
) const
{
    try
    {
        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<ArticleMd> mapper(db);
        mapper.findOne(
            drogon::orm::Criteria(
                ArticleMd::Cols::_id,
                drogon::orm::CompareOperator::EQ,
                (int64_t)std::stoll(id)
            ),
            [callback](const ArticleMd &a)
            {
                Json::Value item;
                item["id"] = a.getValueOfId();
                item["category_id"] = a.getValueOfCategoryId();
                item["title"] = a.getValueOfTitle();
                if (a.getSummary())
                    item["summary"] = a.getValueOfSummary();
                if (a.getContentMd())
                    item["content_md"] = a.getValueOfContentMd();
                item["view_count"] = (Json::Int64)a.getValueOfViewCount();
                if (a.getCreatedAt())
                    item["created_at"] = a.getValueOfCreatedAt().toDbStringLocal();
                if (a.getUpdatedAt())
                    item["updated_at"] = a.getValueOfUpdatedAt().toDbStringLocal();

                auto kw = a.getKeywords();
                if (kw)
                {
                    Json::Value karr(Json::arrayValue);
                    Json::Reader reader;
                    Json::Value parsed;
                    if (reader.parse(*kw, parsed) && parsed.isArray())
                        item["keywords"] = parsed;
                    else
                        item["keywords"] = Json::arrayValue;
                }
                else
                {
                    item["keywords"] = Json::arrayValue;
                }

                Json::Value extra;
                extra["data"] = item;
                json_response(callback, 200, "ok", extra);
            },
            [callback](const drogon::orm::DrogonDbException &e)
            {
                json_response(callback, 500, "未找到文章");
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::incrementView(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string id
) const
{
    try
    {
        auto cb = callback;
        auto db = drogon::app().getDbClient();
        drogon::orm::Mapper<ArticleMd> mapper(db);
        mapper.findOne(
            drogon::orm::Criteria(
                ArticleMd::Cols::_id,
                drogon::orm::CompareOperator::EQ,
                (int64_t)std::stoll(id)
            ),
            [cb, db](const ArticleMd &article)
            {
                drogon::orm::Mapper<ArticleMd> mapper(db);
                auto updated = article;
                updated.setViewCount(article.getValueOfViewCount() + 1);
                mapper.update(updated,
                    [cb]([[maybe_unused]] const size_t count)
                    {
                        json_response(cb, 200, "ok");
                    },
                    [cb](const drogon::orm::DrogonDbException &e)
                    {
                        json_response(cb, 500, "更新失败: " + std::string(e.base().what()));
                    }
                );
            },
            [cb](const drogon::orm::DrogonDbException &e)
            {
                json_response(cb, 500, "未找到文章");
            }
        );
    }
    catch (const std::exception &e)
    {
        json_response(callback, 500, e.what());
    }
}

void api::getArticleImage(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string imageId
) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT image_data, mime_type FROM article_md_image WHERE id = ?",
        [callback](const drogon::orm::Result &r)
        {
            if (r.empty())
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k404NotFound);
                resp->setBody("图片不存在");
                callback(resp);
                return;
            }
            const auto &row = r[0];
            auto mime = row["mime_type"].as<std::string>();
            auto blob = row["image_data"].as<std::string>();
            auto resp = HttpResponse::newHttpResponse();
            resp->setContentTypeString(mime);
            resp->setBody(blob);
            callback(resp);
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setBody(e.base().what());
            callback(resp);
        },
        imageId);
}

// ============ 后台文章管理接口 ============

void api::getAdminArticles(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    auto db = drogon::app().getDbClient();
    std::string sql =
        "SELECT a.id, a.title, a.summary, a.status, a.view_count, "
        "a.created_at, a.updated_at, a.category_id, c.name AS category_name "
        "FROM article_md a LEFT JOIN article_category c ON a.category_id = c.id "
        "ORDER BY a.id DESC LIMIT 200";

    db->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            Json::Value data(Json::arrayValue);
            for (const auto &row : r)
            {
                Json::Value item;
                item["id"] = row["id"].as<int>();
                item["category_id"] = row["category_id"].isNull() ? 0 : row["category_id"].as<int>();
                item["title"] = row["title"].as<std::string>();
                item["summary"] = row["summary"].isNull() ? "" : row["summary"].as<std::string>();
                item["status"] = row["status"].as<int>();
                item["view_count"] = row["view_count"].as<int>();
                item["category_name"] = row["category_name"].isNull() ? "" : row["category_name"].as<std::string>();
                item["created_at"] = row["created_at"].as<std::string>();
                item["updated_at"] = row["updated_at"].as<std::string>();
                data.append(item);
            }
            ret["data"] = data;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        });
}

void api::getAdminArticle(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string id
) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT a.*, c.name AS category_name FROM article_md a "
        "LEFT JOIN article_category c ON a.category_id = c.id WHERE a.id = ?",
        [callback](const drogon::orm::Result &r)
        {
            if (r.empty())
            {
                Json::Value ret;
                ret["code"] = 404;
                ret["msg"] = "文章不存在";
                callback(HttpResponse::newHttpJsonResponse(ret));
                return;
            }
            const auto &row = r[0];
            Json::Value ret;
            ret["code"] = 200;
            ret["data"]["id"] = row["id"].as<int>();
            ret["data"]["category_id"] = row["category_id"].isNull() ? 0 : row["category_id"].as<int>();
            ret["data"]["title"] = row["title"].as<std::string>();
            ret["data"]["summary"] = row["summary"].isNull() ? "" : row["summary"].as<std::string>();
            ret["data"]["content_md"] = row["content_md"].as<std::string>();
            ret["data"]["status"] = row["status"].as<int>();
            ret["data"]["view_count"] = row["view_count"].as<int>();
            ret["data"]["category_name"] = row["category_name"].isNull() ? "" : row["category_name"].as<std::string>();
            ret["data"]["created_at"] = row["created_at"].as<std::string>();
            ret["data"]["updated_at"] = row["updated_at"].as<std::string>();
            auto keywordsStr = row["keywords"].isNull()
                ? "" : row["keywords"].as<std::string>();
            if (!keywordsStr.empty())
            {
                Json::Value karr(Json::arrayValue);
                Json::Reader reader;
                Json::Value parsed;
                if (reader.parse(keywordsStr, parsed) && parsed.isArray())
                    ret["data"]["keywords"] = parsed;
                else
                    ret["data"]["keywords"] = Json::arrayValue;
            }
            else
            {
                ret["data"]["keywords"] = Json::arrayValue;
            }
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        id);
}

void api::createArticle(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback
) const
{
    auto json = req->getJsonObject();
    if (!json)
    {
        json_response(callback, 400, "请求体不能为空");
        return;
    }
    if ((*json)["title"].asString().empty())
    {
        json_response(callback, 400, "标题不能为空");
        return;
    }
    int categoryId = (*json)["category_id"].asInt();
    std::string title = (*json)["title"].asString();
    std::string summary = (*json)["summary"].asString();
    std::string keywords = Json::FastWriter().write((*json)["keywords"]);
    std::string contentMd = (*json)["content_md"].asString();
    int status = (*json)["status"].asInt();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO article_md (category_id, title, summary, keywords, "
        "content_md, status) VALUES (?, ?, ?, ?, ?, ?)",
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            ret["msg"] = "创建成功";
            ret["insertId"] = static_cast<Json::Int64>(r.insertId());
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        categoryId, title, summary, keywords, contentMd, status);
}

void api::updateArticle(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string id
) const
{
    auto json = req->getJsonObject();
    if (!json)
    {
        json_response(callback, 400, "请求体不能为空");
        return;
    }
    int categoryId = (*json)["category_id"].asInt();
    std::string title = (*json)["title"].asString();
    std::string summary = (*json)["summary"].asString();
    std::string keywords = Json::FastWriter().write((*json)["keywords"]);
    std::string contentMd = (*json)["content_md"].asString();
    int status = (*json)["status"].asInt();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE article_md SET category_id=?, title=?, summary=?, keywords=?, "
        "content_md=?, status=? WHERE id=?",
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            ret["msg"] = "更新成功";
            ret["affectedRows"] = static_cast<Json::Int64>(r.affectedRows());
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        categoryId, title, summary, keywords, contentMd, status, id);
}

void api::deleteArticle(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string id
) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM article_md WHERE id=?",
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            ret["msg"] = "删除成功";
            ret["affectedRows"] = static_cast<Json::Int64>(r.affectedRows());
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        id);
}

void api::uploadArticleImage(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string articleId
) const
{
    MultiPartParser parser;
    if (parser.parse(req) != 0)
    {
        json_response(callback, 400, "解析上传数据失败");
        return;
    }
    const auto &files = parser.getFiles();
    if (files.empty())
    {
        json_response(callback, 400, "未找到上传文件");
        return;
    }
    const auto &file = files[0];
    std::string fileName = file.getFileName();
    auto content = file.fileContent();
    std::string fileData(content.data(), content.size());
    auto contentType = file.getContentType();
    std::string mimeStr;
    if (contentType == drogon::CT_IMAGE_PNG)
        mimeStr = "image/png";
    else if (contentType == drogon::CT_IMAGE_JPG)
        mimeStr = "image/jpeg";
    else if (contentType == drogon::CT_IMAGE_GIF)
        mimeStr = "image/gif";
    else if (contentType == drogon::CT_IMAGE_WEBP)
        mimeStr = "image/webp";
    else if (contentType == drogon::CT_IMAGE_BMP)
        mimeStr = "image/bmp";
    else
        mimeStr = "application/octet-stream";
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO article_md_image (article_id, image_name, mime_type, image_data) "
        "VALUES (?, ?, ?, ?)",
        [callback, fileName](const drogon::orm::Result &r)
        {
            auto insertId = r.insertId();
            std::string url = "/tof/r1/api/article_images/" +
                              std::to_string(insertId);
            Json::Value ret;
            ret["code"] = 200;
            ret["msg"] = "上传成功";
            ret["data"]["id"] = static_cast<Json::Int64>(insertId);
            ret["data"]["image_name"] = fileName;
            ret["data"]["url"] = url;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        articleId, fileName, mimeStr, fileData);
}

void api::getArticleImages(
    const HttpRequestPtr& req,
    std::function<void (const HttpResponsePtr &)> &&callback,
    std::string articleId
) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, image_name, mime_type FROM article_md_image WHERE article_id = ? ORDER BY id",
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            Json::Value data(Json::arrayValue);
            for (const auto &row : r)
            {
                Json::Value item;
                auto id = row["id"].as<int>();
                item["id"] = id;
                item["image_name"] = row["image_name"].as<std::string>();
                item["mime_type"] = row["mime_type"].as<std::string>();
                item["url"] = "/tof/r1/api/article_images/" + std::to_string(id);
                data.append(item);
            }
            ret["data"] = data;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            Json::Value ret;
            ret["code"] = 500;
            ret["msg"] = e.base().what();
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        articleId);
}
