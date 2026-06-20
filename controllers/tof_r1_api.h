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
    METHOD_LIST_END

    void registerUser(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getDesktopIcons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getCategories(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void getFiles(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
    void downloadFile(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) const;
};
}
}
