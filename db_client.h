#pragma once
#include <drogon/drogon.h>
#include <string>

namespace db {

void initTiDbConnection();

drogon::orm::DbClientPtr getClient();

} // namespace db
