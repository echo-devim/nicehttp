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
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <string_view>
#include <map>
#include <exception>
#include <iterator>
#include <sstream>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#endif
#include <signal.h>
#include "thread_pool.h"
#include "router.h"

#define NICEHTTP_THREADS 10 // thread pool size
#define PKT_BLOCK_SIZE 4096 // Block size (in byte) read from tcp socket

#ifdef NICEHTTP_VERBOSE
#define NLOG(x)  std::cout << x << std::endl;
#else
#define NLOG(X)
#endif

class NiceHTTP {
    /* Implements HTTP REST API server and client.
     * Connections are closed after each response.
     * Implemented mainly to exchange json messages,
     * you must parse the json payload using external libraries.
     * The server is multi-threaded.
     */
public:
    NiceHTTP();
    ~NiceHTTP();
    void start(std::string iface, short port); //Start the server
    http::Response request(http::Request req, std::string host, short port); //Start the client
    Router& getRouter();
private:
    Router router;
    int server_socket = -1;
    int client_socket = -1;
    bool server_setup(const std::string& iface, const short& port);
    bool client_setup(const std::string& host, const short& port);
    void recv_http(const int& socket, std::string& head, std::string& body);
    bool is_ipaddr(const std::string& host);
    void parsereq(const int& client_fd);
    void cleanup();
};