#pragma once
#include <set>
#include <functional>
#include <string_view>
#include <iostream>
#include "http.h"

using namespace std;

class Route {
private:
    std::function<http::Response(const http::Request &req)> func;
public:
    string_view method;
    string_view uri;
    Route(const string_view &method, const string_view &uri, const std::function<http::Response(const http::Request &req)> &func) {
        this->method = method;
        this->uri = uri;
        this->func = func;
    }
    Route(const Route& route) {
        method = route.method;
        uri = route.uri;
        func = route.func;
    }
    friend bool operator<(const Route& l, const Route& r) {
        return std::tie(l.method, l.uri) < std::tie(r.method, r.uri);
    }
    http::Response handle(const http::Request &req) {
        return this->func(req);
    }
};

class Router {
private:
    set<Route> routes;
public:
    void add(const Route &route);
    http::Response handle(const http::Request &req);
    Router() {}
};