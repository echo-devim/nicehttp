#include "nicehttp.h"

NiceHTTP::NiceHTTP() {
}

void NiceHTTP::cleanup() {
    if (this->server_socket != -1) {
        #ifdef _WIN32
        WSACleanup();
        closesocket(this->server_socket);
        #else
        close(this->server_socket);
        #endif
        this->server_socket = -1;
    }
}

NiceHTTP::~NiceHTTP() {
    this->cleanup();
}

Router& NiceHTTP::getRouter() {
    return this->router;
}

http::Request NiceHTTP::parsehttp(string &req) {
    http::Request obj_req;
    size_t i = req.find("\r\n");
    if (i != string::npos) {
        string line = req.substr(0, i);
        short j = 0;
        for (const auto word : std::views::split(line, ' ')) {
            switch (j) {
                case 0:
                    obj_req.method = string_view(word);
                    break;
                case 1:
                    obj_req.uri = string_view(word);
                    break;
                case 2:
                    obj_req.proto = string_view(word);
                    break;
                default:
                    break;
            }
            j++;
        }
        if (obj_req.proto != PROTO_HTTP1) {
            cerr << "Protocol not supported" << endl;
        }
        string headers = req.substr(i+2);
        for (const auto h : std::views::split(headers, '\n')) {
            string_view header(h);
            size_t i = header.find(": ");
            if (i == string::npos) {
                //invalid header, skip it..
                continue;
            }
            string key(header.substr(0, i));
            string val(header.substr(i+2, header.length()-i-3));
            obj_req.headers.insert({key, val});
            if (key == "Content-Length") {
                //The request has a payload
                obj_req.content_length = strtoul(string(val).c_str(), nullptr, 0);
            } else if (( key == "Content-Type") && (val == "application/json")) {
                obj_req.is_json = true;
            }
        }
        
    } else {
        cerr << "Malformed request" << endl;
    }

    return obj_req;
}

void NiceHTTP::parsereq(int& client_fd) {
    string req = "";
    string body = "";
    int n;
    char buff[BLOCK_SIZE] = {0,};
    std::cout << "Current Thread ID " << this_thread::get_id() << endl;
    while ((n = recv(client_fd, buff, sizeof(buff), 0)))
    {
        string_view b(buff);
        // if we were processing body, continute to put data into body
        if (body != "") {
            body += b;
        } else { //else we process the headers
            size_t header_end = b.find("\r\n\r\n");
            if (header_end != string::npos) {
                req += b.substr(0, header_end+4);
                body += b.substr(header_end+4);
            } else {
                req += b;
            }
        }
        // if the end of buff is null we assume to have read all data (not true for binary data)
        if (buff[BLOCK_SIZE-1] == 0) {
            break;
        }
        memset(buff, 0, sizeof(buff));
    }
    http::Request r(parsehttp(req));
    r.body = body;
    if ((r.content_length > 0) && (body.length() != r.content_length)){
        cout << "Error in body parsing! Content length " << r.content_length << " != body length " << body.length() << endl;
    } else {
        r.body = body;
    }
    http::Response resp = this->router.handle(r);
    resp.headers.insert({"Server", "NiceHTTP"});
    resp.headers.insert({"Connection", "close"});
    string raw_resp = resp.toString();
    //Send response to client
    send(client_fd, raw_resp.c_str(), raw_resp.length(), 0);
    cout << "Exiting thread" << endl;
    close(client_fd);
}

bool NiceHTTP::setup(const string& iface, const short& port) {
    #ifdef _WIN32
    // Initialize WSA variables
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    #endif
    this->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (this->server_socket == -1)
    {
        std::cerr << "Error creating the socket" << std::endl;
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(iface.c_str());
    serverAddr.sin_port = htons(port);

    if (bind(this->server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error binding the socket" << std::endl;
        this->cleanup();
        return false;
    }

    return true;
}

void NiceHTTP::start(string iface, short port) {
    if (!this->setup(iface, port)) {
        return;
    }

    if (listen(this->server_socket, 1) == -1) {
        std::cout << "listen(): Error listening on socket" << std::endl;
        this->cleanup();
        return;
    }

    int client_socket;
    while((client_socket = accept(this->server_socket, NULL, NULL)) > 0) {
        //Client accepted
        thread t(&NiceHTTP::parsereq, this, ref(client_socket));
        t.detach();
    }
}

bool NiceHTTP::connect(string ip, short port) {
    //TODO
}