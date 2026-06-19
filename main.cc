#include <drogon/drogon.h>
#include <drogon/orm/DbConfig.h>
#include "db_client.h"
#include <cstdlib>
#include <string>
#include <iostream>

int main()
{
    db::initTiDbConnection();

    const char *envListenPort = std::getenv("PORT");
    unsigned short listenPort = envListenPort
        ? static_cast<unsigned short>(std::stoul(envListenPort))
        : 5555;
    std::cout << "[config] Listening on port " << listenPort << std::endl;
    drogon::app().addListener("0.0.0.0", listenPort);
    drogon::app().loadConfigFile("./config.json");

    const char *envHost = std::getenv("DB_HOST");
    const char *envPort = std::getenv("DB_PORT");
    const char *envDbname = std::getenv("DB_NAME");
    const char *envUser = std::getenv("DB_USER");
    const char *envPass = std::getenv("DB_PASS");

    std::string host = envHost ? envHost : "127.0.0.1";
    unsigned short port = envPort
        ? static_cast<unsigned short>(std::stoul(envPort))
        : 4000;
    std::string dbname = envDbname ? envDbname : "win_xp_db";
    std::string user = envUser ? envUser : "root";
    std::string passwd = envPass ? envPass : "";

    std::cout << "[config] DB: " << user << "@" << host << ":"
              << port << "/" << dbname << std::endl;

    drogon::orm::MysqlConfig mysqlCfg;
    mysqlCfg.host = host;
    mysqlCfg.port = port;
    mysqlCfg.databaseName = dbname;
    mysqlCfg.username = user;
    mysqlCfg.password = passwd;
    mysqlCfg.connectionNumber = 1;
    mysqlCfg.name = "default";
    mysqlCfg.isFast = false;
    mysqlCfg.characterSet = "";
    mysqlCfg.timeout = -1.0;

    drogon::app().addDbClient(mysqlCfg);

    drogon::app().run();
    return 0;
}
