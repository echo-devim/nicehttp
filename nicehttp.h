#pragma once
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <ranges>
#include <string_view>
#include <map>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include "http.h"
#include "router.h"

#define BLOCK_SIZE 4096

using namespace std;

class NiceHTTP {
public:
    NiceHTTP();
    ~NiceHTTP();
    void start(string iface, short port); //Start the server
    bool connect(string ip, short port); //Start the client
    Router& getRouter();
private:
    Router router;
    int server_socket;
    bool setup(const string& iface, const short& port);
    void parsereq(int& new_sd);
    http::Request parsehttp(string &req);
    void cleanup();
};