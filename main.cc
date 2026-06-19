#include <drogon/drogon.h>
#include "db_client.h"

int main()
{
    db::initTiDbConnection();

    drogon::app().addListener("0.0.0.0", 5555);
    drogon::app().loadConfigFile("../config.json");
    drogon::app().run();
    return 0;
}
