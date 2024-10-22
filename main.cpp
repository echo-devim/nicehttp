//#include "lib/nicehttp.h"
#include "single_include/nicehttp.h"

// Compile using C++23 standard

using namespace std;

http::Response handle_test(const http::Request &req) {
    // here you can parse the request, including parameters
    cout << "Requested uri: " << req.uri << endl;
    for (const auto& p : const_cast<http::Request&>(req).getParams()) {
        std::cout << "key '" << p.first << "' has value '" << p.second << "'" << std::endl;
    }
    // generate response
    map<string,string> headers;
    string body = "{\"status\": \"OK\"}";
    http::Response resp {200, "OK", PROTO_HTTP1, headers, true, body.length(), body};
    return resp;
}

void server_example() {
    NiceHTTP nhttp;
    // Route supports regex
    Route r {"GET", "/test/[0-9]", handle_test, "apptoken123"};
    nhttp.getRouter().add(r);
    nhttp.start("127.0.0.1", 8090);
}

void client_example() {
    NiceHTTP nhttp;
    map<string,string> headers;
    headers.insert({"Authorization", "apptoken123"});
    http::Request req {"GET", "/test/2?id=45&user=admin", PROTO_HTTP1, headers, false, 0, ""};
    http::Response r;
    r = nhttp.request(req, "localhost", 8090);
    cout << r.toString(false) << endl;
}

int main() {
    #ifdef server
    server_example();
    #else
    client_example();
    #endif
    cout << "Exiting main" << endl;
    return 0;
}