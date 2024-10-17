#include "router.h"

void Router::add(const Route &route) {
    this->routes.insert(route);
}

void Router::del(const Route &route) {
    this->routes.erase(route);
}

http::Response Router::handle(const http::Request &req) {
    // handle the request finding the right route
    auto match = [&req](const Route &r){ //match condition to find the right route
        std::smatch m;
        std::regex rgx(r.uri.data());
        return ((req.method == r.method) &&
                (std::regex_match(req.uri.begin(), req.uri.end(), m, rgx)));
    };
    std::set<Route>::iterator result = std::ranges::find_if(this->routes, match);
    if (result != this->routes.end()) {
        Route r = *result;
        return r.handle(req);
    }
    std::map<std::string,std::string> headers;
    http::Response resp(404, "Not Found", PROTO_HTTP1, headers, false, 0);
    return resp;
}

http::Response Route::handle(const http::Request &req) {
    if (auth != "") { // Authentication is set for this route
        bool denied = true;
        for (const auto& h : req.headers) {
            if ((h.first == "authorization") && (h.second == auth)) {
                denied = false;
                break;
            }
        }
        if (denied) {
            std::map<std::string,std::string> headers;
            http::Response resp(401, "Unauthorized", PROTO_HTTP1, headers, false, 0);
            return resp;
        }
    }
    return this->func(req);
}