#include "nicehttp.h"

int main() {
    NiceHTTP mhttp;
    Route r {"GET", "/test", [](const http::Request &req){
        cout << "YES! " << req.uri << endl;
        map<string,string> headers;
        string body = "{\"status\": \"OK\"}";
        http::Response resp {200, "OK", PROTO_HTTP1, headers, true, body.length(), body};
        return resp;
    }};
    Router& router = mhttp.getRouter();
    router.add(r);
    mhttp.start("127.0.0.1", 8090);
    cout << "Exiting main" << endl;
    return 0;
}