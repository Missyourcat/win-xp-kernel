/**
 *
 *  AdminAuth.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
#include <jwt-cpp/jwt.h>
#include <iostream>

using namespace drogon;


class AdminAuth : public HttpFilter<AdminAuth>
{
  public:
    AdminAuth() {}
    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;
  private:
      std::string getSecret() const {
        auto secret = getenv("JWT_SECRET");
        return secret ? secret : "your_secret_key_change_in_production";
      }
};

