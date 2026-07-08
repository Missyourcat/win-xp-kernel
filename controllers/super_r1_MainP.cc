#include "super_r1_MainP.h"
#include "models/AdminUser.h"
#include "models/DesktopIcon.h"
#include "models/WinUser.h"
#include <drogon/orm/SqlBinder.h>
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include <jwt-cpp/jwt.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <sstream>
#include <vector>

using namespace super::r1;

static bool isSafeName(const std::string &name)
{
    if (name.empty())
        return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

static std::string joinStrings(const std::vector<std::string> &parts,
                                const std::string &sep)
{
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
            oss << sep;
        oss << parts[i];
    }
    return oss.str();
}

static std::string toStringValue(const Json::Value &value)
{
    if (value.isNull())
        return std::string();
    if (value.isBool())
        return value.asBool() ? "1" : "0";
    return value.asString();
}

static void makeError(
    std::function<void(const HttpResponsePtr &)> callback,
    int code, const std::string &msg)
{
    Json::Value ret;
    ret["code"] = code;
    ret["msg"] = msg;
    callback(HttpResponse::newHttpJsonResponse(ret));
}

static void getPrimaryKey(
    const drogon::orm::DbClientPtr &db,
    const std::string &table,
    std::function<void(const std::string &)> onPk,
    std::function<void(const std::string &)> onError)
{
    db->execSqlAsync(
        "SELECT COLUMN_NAME FROM information_schema.KEY_COLUMN_USAGE "
        "WHERE TABLE_SCHEMA = 'win_xp_db' AND TABLE_NAME = ? "
        "AND CONSTRAINT_NAME = 'PRIMARY' LIMIT 1",
        [onPk, onError](const drogon::orm::Result &r)
        {
            if (r.empty())
            {
                onError("no primary key found");
                return;
            }
            onPk(r[0][0].as<std::string>());
        },
        [onError](const drogon::orm::DrogonDbException &e)
        {
            onError(e.base().what());
        },
        table);
}

void MainP::King(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto token = req->getCookie("admin_token");
    try
    {
        auto decoded = jwt::decode(token);
        auto username = decoded.get_payload_claim("username").as_string();
        auto role = decoded.get_payload_claim("role").as_string();

        const auto acceptHeader = req->getHeader("accept");
        const bool wantsJson = req->getParameter("json") == "1" ||
            acceptHeader.find("application/json") != std::string::npos;

        if (wantsJson)
        {
            auto db = drogon::app().getDbClient();
            db->execSqlAsync(
                "SHOW TABLES",
                [callback, username, role](const drogon::orm::Result &r)
                {
                    Json::Value ret;
                    Json::Value tables(Json::arrayValue);
                    ret["curr_user"] = username;
                    ret["curr_role"] = role;
                    for (const auto &row : r)
                        tables.append(row[0].as<std::string>());
                    ret["tables"] = tables;
                    callback(HttpResponse::newHttpJsonResponse(ret));
                },
                [callback](const drogon::orm::DrogonDbException &e)
                {
                    makeError(callback, 500, e.base().what());
                });
            return;
        }

        const char *vd = getenv("VIEWS_DIR");
        std::string viewsDir = vd ? vd : "../views";
        auto resp = HttpResponse::newFileResponse(viewsDir + "/mainpage.html");
        callback(resp);
    }
    catch (const std::exception &)
    {
        auto resp = HttpResponse::newRedirectionResponse(
            "/super/r1/LoginP/admin_in");
        callback(resp);
    }
}

void MainP::getTableData(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string tableName) const
{
    if (!isSafeName(tableName))
    {
        makeError(callback, 400, "非法表名");
        return;
    }

    auto db = drogon::app().getDbClient();
    std::string sql = "SELECT * FROM " + tableName + " LIMIT 200";
    db->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            Json::Value columns(Json::arrayValue);
            Json::Value data(Json::arrayValue);
            for (size_t i = 0; i < r.columns(); ++i)
                columns.append(r.columnName(i));
            for (const auto &row : r)
            {
                Json::Value item;
                for (size_t i = 0; i < r.columns(); ++i)
                {
                    auto col = r.columnName(i);
                    auto val = row[i].isNull() ? "" : row[i].as<std::string>();
                    if (col == "image_data" && val.length() > 50)
                        val = val.substr(0, 50) + "...";
                    item[col] = row[i].isNull()
                        ? Json::Value()
                        : Json::Value(val);
                }
                data.append(item);
            }
            ret["columns"] = columns;
            ret["data"] = data;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            makeError(callback, 500, e.base().what());
        });
}

void MainP::addTableData(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string tableName) const
{
    if (!isSafeName(tableName))
    {
        makeError(callback, 400, "非法表名");
        return;
    }

    auto json = req->getJsonObject();
    if (!json || json->empty())
    {
        makeError(callback, 400, "提交数据不能为空");
        return;
    }

    std::vector<std::string> cols;
    std::vector<std::string> vals;

    for (auto it = json->begin(); it != json->end(); ++it)
    {
        auto key = it.key().asString();
        if (!isSafeName(key))
        {
            makeError(callback, 400, "非法字段名");
            return;
        }
        cols.push_back(key);
        vals.push_back(toStringValue(*it));
    }

    if (cols.empty())
    {
        makeError(callback, 400, "没有可插入的字段");
        return;
    }

    auto db = drogon::app().getDbClient();
    std::string sql = "INSERT INTO " + tableName + " (" +
                      joinStrings(cols, ", ") + ") VALUES (" +
                      joinStrings(std::vector<std::string>(cols.size(), "?"),
                                  ", ") + ")";

    auto binder = (*db) << sql;
    for (const auto &v : vals)
        binder << v;
    binder >> [callback](const drogon::orm::Result &r)
    {
        Json::Value ret;
        ret["code"] = 200;
        ret["msg"] = "插入成功";
        ret["insertId"] = static_cast<Json::Int64>(r.insertId());
        callback(HttpResponse::newHttpJsonResponse(ret));
    };
    binder >> [callback](const drogon::orm::DrogonDbException &e)
    {
        makeError(callback, 500, e.base().what());
    };
}

void MainP::updateTableData(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string tableName,
    std::string id) const
{
    if (!isSafeName(tableName))
    {
        makeError(callback, 400, "非法表名");
        return;
    }
    if (id.empty())
    {
        makeError(callback, 400, "缺少记录 id");
        return;
    }

    auto json = req->getJsonObject();
    if (!json || json->empty())
    {
        makeError(callback, 400, "提交数据不能为空");
        return;
    }

    auto db = drogon::app().getDbClient();

    getPrimaryKey(db, tableName,
        [callback, db, json, tableName, id](const std::string &pkCol)
        {
            std::vector<std::string> setClauses;
            std::vector<std::string> params;

            for (auto it = json->begin(); it != json->end(); ++it)
            {
                auto key = it.key().asString();
                if (key == pkCol)
                    continue;
                if (!isSafeName(key))
                {
                    makeError(callback, 400, "非法字段名");
                    return;
                }
                setClauses.push_back(key + " = ?");
                params.push_back(toStringValue(*it));
            }

            if (setClauses.empty())
            {
                makeError(callback, 400, "没有可更新的字段");
                return;
            }

            params.push_back(id);

            std::string sql = "UPDATE " + tableName + " SET " +
                              joinStrings(setClauses, ", ") +
                              " WHERE " + pkCol + " = ?";

            auto binder = (*db) << sql;
            for (const auto &v : params)
                binder << v;
            binder >> [callback](const drogon::orm::Result &r)
            {
                Json::Value ret;
                ret["code"] = 200;
                ret["msg"] = "更新成功";
                ret["affectedRows"] =
                    static_cast<Json::Int64>(r.affectedRows());
                callback(HttpResponse::newHttpJsonResponse(ret));
            };
            binder >> [callback](const drogon::orm::DrogonDbException &e)
            {
                makeError(callback, 500, e.base().what());
            };
        },
        [callback](const std::string &err)
        {
            makeError(callback, 500, err);
        });
}

void MainP::deleteTableData(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string tableName,
    std::string id) const
{
    if (!isSafeName(tableName))
    {
        makeError(callback, 400, "非法表名");
        return;
    }
    if (id.empty())
    {
        makeError(callback, 400, "缺少记录 id");
        return;
    }

    auto db = drogon::app().getDbClient();

    getPrimaryKey(db, tableName,
        [callback, db, tableName, id](const std::string &pkCol)
        {
            std::string sql = "DELETE FROM " + tableName +
                              " WHERE " + pkCol + " = ?";

            auto binder = (*db) << sql;
            binder << id;
            binder >> [callback](const drogon::orm::Result &r)
            {
                Json::Value ret;
                ret["code"] = 200;
                ret["msg"] = "删除成功";
                ret["affectedRows"] =
                    static_cast<Json::Int64>(r.affectedRows());
                callback(HttpResponse::newHttpJsonResponse(ret));
            };
            binder >> [callback](const drogon::orm::DrogonDbException &e)
            {
                makeError(callback, 500, e.base().what());
            };
        },
        [callback](const std::string &err)
        {
            makeError(callback, 500, err);
        });
}

void MainP::getTableSchema(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string tableName) const
{
    if (!isSafeName(tableName))
    {
        makeError(callback, 400, "非法表名");
        return;
    }

    auto db = drogon::app().getDbClient();
    std::string schemaSql = "SHOW COLUMNS FROM " + tableName;

    db->execSqlAsync(
        schemaSql,
        [callback, db, tableName](const drogon::orm::Result &sr)
        {
            Json::Value columns(Json::arrayValue);
            Json::Value autoIncrement(Json::arrayValue);
            Json::Value defaultTs(Json::arrayValue);
            std::string pkCol;

            for (const auto &row : sr)
            {
                if (row["Field"].isNull())
                    continue;
                std::string col = row["Field"].as<std::string>();
                columns.append(col);
                if (!row["Key"].isNull() &&
                    row["Key"].as<std::string>() == "PRI")
                    pkCol = col;

                if (!row["Extra"].isNull())
                {
                    std::string extra = row["Extra"].as<std::string>();
                    std::transform(extra.begin(), extra.end(), extra.begin(), ::tolower);
                    if (extra.find("auto_increment") != std::string::npos)
                        autoIncrement.append(col);
                }

                if (!row["Default"].isNull())
                {
                    std::string def = row["Default"].as<std::string>();
                    std::transform(def.begin(), def.end(), def.begin(), ::tolower);
                    if (def.find("current_timestamp") != std::string::npos)
                        defaultTs.append(col);
                }
            }

            Json::Value skipInsert(Json::arrayValue);
            for (const auto &col : autoIncrement)
                skipInsert.append(col.asString());
            for (const auto &col : defaultTs)
                skipInsert.append(col.asString());

            std::string dataSql = "SELECT * FROM " + tableName + " LIMIT 200";
            db->execSqlAsync(
                dataSql,
                [callback, columns, pkCol, skipInsert](const drogon::orm::Result &dr)
                {
                    Json::Value ret;
                    Json::Value data(Json::arrayValue);
                    for (const auto &row : dr)
                    {
                        Json::Value item;
                        for (size_t i = 0; i < dr.columns(); ++i)
                        {
                            auto col = dr.columnName(i);
                            auto val = row[i].isNull() ? "" : row[i].as<std::string>();
                            if (col == "image_data" && val.length() > 50)
                                val = val.substr(0, 50) + "...";
                            item[col] = row[i].isNull()
                                ? Json::Value()
                                : Json::Value(val);
                        }
                        data.append(item);
                    }
                    ret["code"] = 200;
                    ret["columns"] = columns;
                    ret["primaryKey"] = pkCol;
                    ret["skipInsert"] = skipInsert;
                    ret["data"] = data;
                    callback(HttpResponse::newHttpJsonResponse(ret));
                },
                [callback](const drogon::orm::DrogonDbException &e)
                {
                    makeError(callback, 500, e.base().what());
                });
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            makeError(callback, 500, e.base().what());
        });
}

// ============ 文章模块 ============

void MainP::articleEditor(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    const char *vd = getenv("VIEWS_DIR");
    std::string viewsDir = vd ? vd : "../views";
    auto resp = HttpResponse::newFileResponse(viewsDir + "/article_editor.html");
    callback(resp);
}

void MainP::getCategories(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, name, description FROM article_category ORDER BY id",
        [callback](const drogon::orm::Result &r)
        {
            Json::Value ret;
            ret["code"] = 200;
            Json::Value data(Json::arrayValue);
            for (const auto &row : r)
            {
                Json::Value item;
                item["id"] = row["id"].as<int>();
                item["name"] = row["name"].as<std::string>();
                item["description"] = row["description"].isNull()
                    ? "" : row["description"].as<std::string>();
                data.append(item);
            }
            ret["data"] = data;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            makeError(callback, 500, e.base().what());
        });
}

void MainP::getArticles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
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
            makeError(callback, 500, e.base().what());
        });
}

void MainP::getArticle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string id) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT a.*, c.name AS category_name FROM article_md a "
        "LEFT JOIN article_category c ON a.category_id = c.id WHERE a.id = ?",
        [callback](const drogon::orm::Result &r)
        {
            if (r.empty())
            {
                makeError(callback, 404, "文章不存在");
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
            makeError(callback, 500, e.base().what());
        },
        id);
}

void MainP::createArticle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto json = req->getJsonObject();
    if (!json)
    {
        makeError(callback, 400, "请求体不能为空");
        return;
    }
    if ((*json)["title"].asString().empty())
    {
        makeError(callback, 400, "标题不能为空");
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
            makeError(callback, 500, e.base().what());
        },
        categoryId, title, summary, keywords, contentMd, status);
}

void MainP::updateArticle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string id) const
{
    auto json = req->getJsonObject();
    if (!json)
    {
        makeError(callback, 400, "请求体不能为空");
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
            makeError(callback, 500, e.base().what());
        },
        categoryId, title, summary, keywords, contentMd, status, id);
}

void MainP::deleteArticle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string id) const
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
            makeError(callback, 500, e.base().what());
        },
        id);
}

void MainP::uploadArticleImage(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string articleId) const
{
    MultiPartParser parser;
    if (parser.parse(req) != 0)
    {
        makeError(callback, 400, "解析上传数据失败");
        return;
    }
    const auto &files = parser.getFiles();
    if (files.empty())
    {
        makeError(callback, 400, "未找到上传文件");
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
            std::string url = "/super/r1/MainP/api/article_images/" +
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
            makeError(callback, 500, e.base().what());
        },
        articleId, fileName, mimeStr, fileData);
}

void MainP::getArticleImages(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string articleId) const
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
                item["url"] = "/super/r1/MainP/api/article_images/" + std::to_string(id);
                data.append(item);
            }
            ret["data"] = data;
            callback(HttpResponse::newHttpJsonResponse(ret));
        },
        [callback](const drogon::orm::DrogonDbException &e)
        {
            makeError(callback, 500, e.base().what());
        },
        articleId);
}

void MainP::serveImage(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string imageId) const
{
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT image_data, mime_type FROM article_md_image WHERE id = ?",
        [callback](const drogon::orm::Result &r)
        {
            if (r.empty())
            {
                makeError(callback, 404, "图片不存在");
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
            makeError(callback, 500, e.base().what());
        },
        imageId);
}
