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
#include <string_view>
#include <map>
#include <format>
#include <ranges>
#include <string>
#include <iostream>
#include <algorithm>
#define PROTO_HTTP1 "HTTP/1.1"

namespace http {

// Common http message (could be request or response)
class Message { // base class
public:
    std::string proto;
    std::map<std::string,std::string> headers;
    std::string body;
    bool is_json = false;
    size_t content_length = 0;
    Message() {}
    void parseHeaders(const std::string& headerstr);
};


class Request : public Message {
public:
    std::string method;
    std::string uri;
    Request() {}
    Request(std::string &head, std::string &body);
    Request(std::string method, std::string uri, std::string proto, std::map<std::string,std::string> headers, bool is_json, size_t content_length, std::string body = "");
    Request(const Request& hr);
    std::string toString(bool carriage_return = true);
    Request& operator=(const Request& other);
};

class Response : public Message {
public:
    short code;
    std::string message;
    Response() {}
    Response(std::string &head, std::string &body);
    Response(short code, std::string message, std::string proto, std::map<std::string,std::string> headers, bool is_json, size_t content_length, std::string body = "");
    Response(const Response& r);
    std::string toString(bool carriage_return = true);
    Response& operator=(const Response& other);
};

};