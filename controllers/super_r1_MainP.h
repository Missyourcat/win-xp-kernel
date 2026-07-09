#pragma once

#include <drogon/HttpController.h>
#include "../filters/AdminAuth.h"

using namespace drogon;

namespace super
{
namespace r1
{
class MainP : public drogon::HttpController<MainP>
{
  public:
    METHOD_LIST_BEGIN

    METHOD_ADD(MainP::King, "/king", Get, "AdminAuth");
    // 表数据接口
    METHOD_ADD(MainP::getTableData, "/api/table/{1}", Get, "AdminAuth");
    METHOD_ADD(MainP::addTableData, "/api/table/{1}", Post, "AdminAuth");
    METHOD_ADD(MainP::updateTableData, "/api/table/{1}/{2}", Put, "AdminAuth");
    METHOD_ADD(MainP::deleteTableData, "/api/table/{1}/{2}", Delete, "AdminAuth");
    // 获取表结构
    METHOD_ADD(MainP::getTableSchema, "/api/table/{1}/schema", Get, "AdminAuth");

    // 文章编辑器页面
    METHOD_ADD(MainP::articleEditor, "/article_editor", Get, "AdminAuth");

    METHOD_LIST_END
    
    void King(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback) const;
    
    // 获取表数据
    void getTableData(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      std::string tableName) const;
    
    // 新增数据
    void addTableData(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      std::string tableName) const;
    
    // 更新数据
    void updateTableData(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback,
                         std::string tableName,
                         std::string id) const;
    
    // 删除数据
    void deleteTableData(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback,
                         std::string tableName,
                         std::string id) const;
    
    // 获取表结构
    void getTableSchema(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        std::string tableName) const;

    // 文章编辑器页面
    void articleEditor(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback) const;
};
}
}