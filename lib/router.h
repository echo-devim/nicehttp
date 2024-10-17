/*
Copyright 2024 echo-devim

Redistribution and use in source and binary forms, with or without modification, are permitted provided
that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this list of conditions and
    the following disclaimer. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and thefollowing disclaimer in the documentation and/or other materials provided
    with the distribution. Neither the name of the copyright holder nor the names of its contributors may
    be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include <set>
#include <functional>
#include <string_view>
#include <iostream>
#include <regex>
#include "http.h"

class Route {
    /*
    * This class handle a single request calling the specified callback function.
    * Supports authentication token passed through "Authentication" header.
    * Doesn't support parameter parsing.
    */
private:
    std::function<http::Response(const http::Request &req)> func;
public:
    std::string_view method;
    std::string_view uri;
    std::string_view auth;
    Route(const std::string_view &method, const std::string_view &uri, const std::function<http::Response(const http::Request &req)> &func, std::string_view auth = "") {
        this->method = method;
        this->uri = uri;
        this->func = func;
        this->auth = auth;
    }
    Route(const Route& route) {
        method = route.method;
        uri = route.uri;
        func = route.func;
        auth = route.auth;
    }
    friend bool operator<(const Route& l, const Route& r) {
        return std::tie(l.method, l.uri) < std::tie(r.method, r.uri);
    }
    http::Response handle(const http::Request &req);
};

class Router {
    /* This class is a container for a list of Routes */
private:
    std::set<Route> routes;
public:
    void add(const Route &route); // add route
    void del(const Route &route); // delete route
    http::Response handle(const http::Request &req); // find the Route and call the callback function
    Router() {}
};