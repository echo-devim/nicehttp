#pragma once
#include <string_view>
#include <map>
#include <format>
#define PROTO_HTTP1 "HTTP/1.1"

using namespace std;

namespace http {

class Request {
public:
    string method;
    string uri;
    string proto;
    map<string,string> headers;
    string body;
    bool is_json = false;
    size_t content_length = 0;
    Request() {}
    Request(string method, string uri, string proto, map<string,string> headers, bool is_json, size_t content_length, string body = "") {
        this->method = method;
        this->uri = uri;
        this->proto = proto;
        this->content_length = content_length;
        this->body = body;
        this->headers = headers;
        this->is_json = is_json;
    }
    Request(const Request& hr) {
        method = hr.method;
        uri = hr.uri;
        proto = hr.proto;
        content_length = hr.content_length;
        body = hr.body;
        headers = hr.headers;
        is_json = hr.is_json;
    }
    string toString(bool carriage_return = true) {
        string endline;
        carriage_return ? endline = "\r\n" : endline = "\n";
        string req { std::format("{} {} {}{}", this->method, this->uri, this->proto, endline) };
        for (const auto& h : this->headers) {
            req += std::format("{}: {}{}", h.first, h.second, endline);
        }
        if (this->content_length != 0) {
            req += std::format("Content-Length: {}{}", this->content_length, endline);
            if (this->is_json) {
                req += "Content-Type: application/json" + endline;
            }
            req += endline;
            req += this->body;
        } else {
            req += endline;
        }
        return req;
    }
};

class Response {
public:
    short code;
    string message;
    string proto;
    map<string,string> headers;
    string body;
    bool is_json = false;
    size_t content_length = 0;
    Response() {}
    Response(short code, string message, string proto, map<string,string> headers, bool is_json, size_t content_length, string body = "") {
        this->message = message;
        this->code = code;
        this->proto = proto;
        this->content_length = content_length;
        this->body = body;
        this->headers = headers;
        this->is_json = is_json;
    }
    Response(const Response& r) {
        message = r.message;
        code = r.code;
        proto = r.proto;
        content_length = r.content_length;
        body = r.body;
        headers = r.headers;
        is_json = r.is_json;
    }
    string toString(bool carriage_return = true) {
        string endline;
        carriage_return ? endline = "\r\n" : endline = "\n";
        string res { std::format("{} {} {}{}", this->proto, this->code, this->message, endline) };
        for (const auto& h : this->headers) {
            res += std::format("{}: {}{}", h.first, h.second, endline);
        }
        if (this->content_length != 0) {
            res += std::format("Content-Length: {}{}", this->content_length, endline);
            if (this->is_json) {
                res += "Content-Type: application/json" + endline;
            }
            res += endline;
            res += this->body;
        } else {
            res += endline;
        }
        return res;
    }
};

};