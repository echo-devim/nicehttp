#include "router.h"

void Router::add(const Route &route) {
    this->routes.insert(route);
}

http::Response Router::handle(const http::Request &req) {
    auto match = [&req](const Route &r){ return ((req.method == r.method)&&(req.uri == r.uri)); };
    set<Route>::iterator result = ranges::find_if(this->routes, match);
    if (result != this->routes.end()) {
        Route r = *result;
        return r.handle(req);
    }
    map<string,string> headers;
    http::Response resp(404, "Not Found", PROTO_HTTP1, headers, false, 0);
    return resp;
}