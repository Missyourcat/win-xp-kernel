#define _GNU_SOURCE
#include "db_client.h"
#include <dlfcn.h>
#include <mysql/mysql.h>
#include <cstdlib>
#include <iostream>

typedef int (*real_connect_start_t)(MYSQL **, MYSQL *, const char *, const char *,
                                    const char *, const char *, unsigned int,
                                    const char *, unsigned long);

extern "C" int mysql_real_connect_start(MYSQL **ret, MYSQL *mysql,
                                        const char *host, const char *user,
                                        const char *passwd, const char *db,
                                        unsigned int port,
                                        const char *unix_socket,
                                        unsigned long clientflag)
{
    static real_connect_start_t real_func = nullptr;
    if (!real_func)
    {
        real_func = (real_connect_start_t)dlsym(RTLD_NEXT,
                                                "mysql_real_connect_start");
        if (!real_func)
        {
            std::cerr << "dlsym failed: " << dlerror() << std::endl;
            std::_Exit(1);
        }
    }

    const char *caPath = std::getenv("DB_SSL_CA");
    if (!caPath)
        caPath = "/etc/ssl/certs/ca-certificates.crt";

    my_bool enforce = 1;
    mysql_options(mysql, MYSQL_OPT_SSL_ENFORCE, &enforce);
    mysql_options(mysql, MYSQL_OPT_SSL_CA, caPath);

    return real_func(ret, mysql, host, user, passwd, db, port,
                     unix_socket, clientflag);
}

namespace db {

void initTiDbConnection()
{
    std::cout << "[db_client] TiDB SSL interposer installed." << std::endl;
}

drogon::orm::DbClientPtr getClient()
{
    return drogon::app().getDbClient("default");
}

} // namespace db
