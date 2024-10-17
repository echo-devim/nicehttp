# NiceHTTP

Small cross-platform HTTP REST API client/server framework written as an exercise to learn modern C++23.

The framework supports only HTTP 1.1 and can be used to deploy simple rest api services.

This project is made for inter-communication among applications using HTTP protocol. The code is very simple and hackable, thus you can customize it for your needs.

Features:
- Cross-Platform (supports Linux/Windows)
- Supports only HTTP/1.1 protocol
- Supports authentication
- Server multi-threaded
- single file header to include
- very easy and fast

## Server example

```c++
//handle /test/<int> endpoint
http::Response handle_test(const http::Request &req) {
    // here you can manually parse the request, including parameters
    cout << "Requested uri: " << req.uri << endl;
    // generate response
    map<string,string> headers;
    string body = "{\"status\": \"OK\"}";
    http::Response resp {200, "OK", PROTO_HTTP1, headers, true, body.length(), body};
    return resp;
}

void main() {
    NiceHTTP mhttp;
    // Route supports regex
    Route r {"GET", "/test/[0-9]", handle_test, "apptoken123"};
    mhttp.getRouter().add(r);
    mhttp.start("127.0.0.1", 8090);
}
```

## Client example
```c++
void main() {
    NiceHTTP mhttp;
    map<string,string> headers;
    //Add app auth token
    headers.insert({"Authorization", "apptoken123"});
    http::Request req {"GET", "/test/2", PROTO_HTTP1, headers, false, 0, ""};
    http::Response r;
    r = mhttp.request(req, "localhost", 8090);
    cout << r.toString(false) << endl; //print raw response
}
```

# Compile

Use the following commands to compile the project for Linux.

Demo server compilation:
```sh
g++ -D server -D NICEHTTP_VERBOSE -std=c++23 -o nhttpsrv main.cpp
```
Demo client compilation:
```sh
g++ -D client -std=c++23 -o nhttpcl main.cpp
```

Use the following commands to compile the project for Windows.

Demo server compilation:
```sh
x86_64-w64-mingw32-g++ -D server -D NICEHTTP_VERBOSE -std=c++23 -static -fno-rtti -o nhttpsrv.exe main.cpp -lws2_32
```

Demo client compilation:
```sh
x86_64-w64-mingw32-g++ -D client -std=c++23 -static -fno-rtti -o nhttpcl.exe main.cpp -lws2_32
```