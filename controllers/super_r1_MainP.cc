#include "super_r1_MainP.h"
#include "models/AdminUser.h"
#include "models/DesktopIcon.h"
#include "models/WinUser.h"
#include <drogon/orm/SqlBinder.h>
#include <jwt-cpp/jwt.h>
#include <algorithm>
#include <cctype>
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

        auto resp = HttpResponse::newFileResponse("../views/mainpage.html");
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
                    item[col] = row[i].isNull()
                        ? Json::Value()
                        : Json::Value(row[i].as<std::string>());
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
                            item[col] = row[i].isNull()
                                ? Json::Value()
                                : Json::Value(row[i].as<std::string>());
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
