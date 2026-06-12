#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace super
{
namespace r1
{
class MainP : public drogon::HttpController<MainP>
{
  public:
    METHOD_LIST_BEGIN

    METHOD_ADD(MainP::King,"/king",Get);

    METHOD_LIST_END
    void King(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    )const;

};
}
}
