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
    //本地用../，云端用./
    // drogon::app().loadConfigFile("./config.json");
    drogon::app().loadConfigFile("../config.json");
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

    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &req,
                                                 const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    });

    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                                drogon::AdviceCallback &&acb,
                                                drogon::AdviceChainCallback &&ccb) {
        if (req->getMethod() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            resp->addHeader("Access-Control-Max-Age", "86400");
            acb(resp);
            return;
        }
        ccb();
    });

    drogon::app().run();
    return 0;
}
