#include "http.h"

void http::Message::parseHeaders(const std::string& headerstr) {
    for (const auto h : std::views::split(headerstr, '\n')) {
        std::string_view header(h);
        size_t i = header.find(": ");
        if (i == std::string::npos) {
            //invalid header, skip it..
            continue;
        }
        std::string key(header.substr(0, i));
        std::string val(header.substr(i+2, header.length()-i-3));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
        if (key == "content-length") {
            //The request has a payload
            this->content_length = strtoul(std::string(val).c_str(), nullptr, 0);
        } else if (( key == "content-type") && (val == "application/json")) {
            this->is_json = true;
        } else {
            this->headers.insert({key, val});
        }
    }
}

http::Request::Request(std::string method, std::string uri, std::string proto, std::map<std::string,std::string> headers, bool is_json, size_t content_length, std::string body) {
    this->method = method;
    this->uri = uri;
    this->proto = proto;
    this->content_length = content_length;
    this->body = body;
    this->headers = headers;
    this->is_json = is_json;
}

http::Request::Request(std::string &head, std::string& body) {
    size_t i = head.find("\r\n");
    if (i != std::string::npos) {
        std::string line = head.substr(0, i);
        short j = 0;
        for (const auto word : std::views::split(line, ' ')) {
            switch (j) {
                case 0:
                    this->method = std::string_view(word);
                    break;
                case 1:
                    this->uri = std::string_view(word);
                    break;
                case 2:
                    this->proto = std::string_view(word);
                    break;
                default:
                    break;
            }
            j++;
        }
        if (this->proto != PROTO_HTTP1) {
            std::cerr << "Protocol not supported" << std::endl;
        }
        std::string headerstr = head.substr(i+2);
        this->parseHeaders(headerstr);
        if ((this->content_length > 0) && (body.length() != this->content_length)){
            std::cout << "Error in body parsing! Content length " << this->content_length << " != body length " << body.length() << std::endl;
        } else {
            this->body = body;
        }
    } else {
        std::cerr << "Malformed request" << std::endl;
    }
}

http::Request::Request(const Request& hr) {
    method = hr.method;
    uri = hr.uri;
    proto = hr.proto;
    content_length = hr.content_length;
    body = hr.body;
    headers = hr.headers;
    is_json = hr.is_json;
}

std::string http::Request::toString(bool carriage_return) {
    std::string endline;
    carriage_return ? endline = "\r\n" : endline = "\n";
    std::string req { std::format("{} {} {}{}", this->method, this->uri, this->proto, endline) };
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

http::Response::Response(short code, std::string message, std::string proto, std::map<std::string,std::string> headers, bool is_json, size_t content_length, std::string body) {
    this->message = message;
    this->code = code;
    this->proto = proto;
    this->content_length = content_length;
    this->body = body;
    this->headers = headers;
    this->is_json = is_json;
}

http::Response::Response(std::string &head, std::string &body) {
    size_t i = head.find("\r\n");
    if (i != std::string::npos) {
        std::string line = head.substr(0, i);
        short j = 0;
        int digits = 0;
        for (const auto word : std::views::split(line, ' ')) {
            std::string_view w(word);
            switch (j) {
                case 0:
                    this->proto = std::string_view(w);
                    break;
                case 1:
                    digits = w.length();
                    this->code = atoi(w.data());
                    break;
                default:
                    break;
            }
            j++;
        }
        // All the rest of the line is the message
        this->message = line.substr(this->proto.length()+digits+2);
        if (this->proto != PROTO_HTTP1) {
            std::cerr << "Protocol not supported" << std::endl;
        }
        std::string headerstr = head.substr(i+2);
        this->parseHeaders(headerstr);
        if ((this->content_length > 0) && (body.length() != this->content_length)){
            std::cout << "Error in body parsing! Content length " << this->content_length << " != body length " << body.length() << std::endl;
        } else {
            this->body = body;
        }
    } else {
        std::cerr << "Malformed request" << std::endl;
    }
}

http::Response::Response(const Response& r) {
    message = r.message;
    code = r.code;
    proto = r.proto;
    content_length = r.content_length;
    body = r.body;
    headers = r.headers;
    is_json = r.is_json;
}

std::string http::Response::toString(bool carriage_return) {
    std::string endline;
    carriage_return ? endline = "\r\n" : endline = "\n";
    std::string res { std::format("{} {} {}{}", this->proto, this->code, this->message, endline) };
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

http::Request& http::Request::operator=(const Request& other) {
    // Guard self assignment
    if (this == &other)
        return *this;
 
    this->method = other.method;
    this->uri = other.uri;
    this->proto = other.proto;
    this->content_length = other.content_length;
    this->body = other.body;
    this->headers = other.headers;
    this->is_json = other.is_json;

    return *this;
}

http::Response& http::Response::operator=(const Response& other) {
    // Guard self assignment
    if (this == &other)
        return *this;
 
    this->message = other.message;
    this->code = other.code;
    this->proto = other.proto;
    this->content_length = other.content_length;
    this->body = other.body;
    this->headers = other.headers;
    this->is_json = other.is_json;

    return *this;
}