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
    // 分类接口
    METHOD_ADD(MainP::getCategories, "/api/categories", Get, "AdminAuth");
    // 文章 CRUD
    METHOD_ADD(MainP::getArticles, "/api/articles", Get, "AdminAuth");
    METHOD_ADD(MainP::getArticle, "/api/articles/{1}", Get, "AdminAuth");
    METHOD_ADD(MainP::createArticle, "/api/articles", Post, "AdminAuth");
    METHOD_ADD(MainP::updateArticle, "/api/articles/{1}", Put, "AdminAuth");
    METHOD_ADD(MainP::deleteArticle, "/api/articles/{1}", Delete, "AdminAuth");
    // 文章图片
    METHOD_ADD(MainP::uploadArticleImage, "/api/articles/{1}/image", Post, "AdminAuth");
    METHOD_ADD(MainP::getArticleImages, "/api/articles/{1}/images", Get, "AdminAuth");
    METHOD_ADD(MainP::serveImage, "/api/article_images/{1}", Get, "AdminAuth");

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
    // 获取文章分类
    void getCategories(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback) const;
    // 获取文章列表
    void getArticles(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback) const;
    // 获取单篇文章
    void getArticle(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string id) const;
    // 创建文章
    void createArticle(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback) const;
    // 更新文章
    void updateArticle(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       std::string id) const;
    // 删除文章
    void deleteArticle(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       std::string id) const;
    // 上传文章图片
    void uploadArticleImage(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback,
                            std::string articleId) const;
    // 获取文章图片列表
    void getArticleImages(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback,
                          std::string articleId) const;
    // 提供图片二进制数据
    void serveImage(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string imageId) const;
};
}
}