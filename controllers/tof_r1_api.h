#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace tof
{
namespace r1
{
class api : public drogon::HttpController<api>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(api::registerUser, "/tof/r1/api/register", Post);
    ADD_METHOD_TO(api::login, "/tof/r1/api/login", Post);
    ADD_METHOD_TO(api::getDesktopIcons, "/tof/r1/api/desktop_icons", Get);
    ADD_METHOD_TO(api::getCategories, "/tof/r1/api/categories", Get);
    ADD_METHOD_TO(api::getFiles, "/tof/r1/api/files", Get);
    ADD_METHOD_TO(api::downloadFile, "/tof/r1/api/file/download", Get);
    ADD_METHOD_TO(api::getSystemInfo, "/tof/r1/api/system/info", Get);
    // 公开文章浏览
    ADD_METHOD_TO(api::getArticleCategories, "/tof/r1/api/article_categories", Get);
    ADD_METHOD_TO(api::getArticles, "/tof/r1/api/articles", Get);
    ADD_METHOD_TO(api::getArticle, "/tof/r1/api/articles/{1}", Get);
    ADD_METHOD_TO(api::incrementView, "/tof/r1/api/articles/{1}/view", Put);
    ADD_METHOD_TO(api::getArticleImage, "/tof/r1/api/article_images/{1}", Get);
    // 后台文章管理
    ADD_METHOD_TO(api::getAdminArticles, "/tof/r1/api/admin/articles", Get);
    ADD_METHOD_TO(api::getAdminArticle, "/tof/r1/api/admin/articles/{1}", Get);
    ADD_METHOD_TO(api::createArticle, "/tof/r1/api/admin/articles", Post);
    ADD_METHOD_TO(api::updateArticle, "/tof/r1/api/admin/articles/{1}", Put);
    ADD_METHOD_TO(api::deleteArticle, "/tof/r1/api/admin/articles/{1}", Delete);
    ADD_METHOD_TO(api::uploadArticleImage, "/tof/r1/api/admin/articles/{1}/image", Post);
    ADD_METHOD_TO(api::getArticleImages, "/tof/r1/api/admin/articles/{1}/images", Get);
    METHOD_LIST_END

    void registerUser(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getDesktopIcons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getCategories(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getFiles(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void downloadFile(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getSystemInfo(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    // 公开文章浏览
    void getArticleCategories(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getArticles(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getArticle(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string id) const;
    void incrementView(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string id) const;
    void getArticleImage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string imageId) const;
    // 后台文章管理
    void getAdminArticles(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getAdminArticle(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string id) const;
    void createArticle(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void updateArticle(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string id) const;
    void deleteArticle(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string id) const;
    void uploadArticleImage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string articleId) const;
    void getArticleImages(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string articleId) const;
};
}
}
