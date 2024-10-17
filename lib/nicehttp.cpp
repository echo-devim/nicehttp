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
    if (this->client_socket != -1) {
        #ifdef _WIN32
        WSACleanup();
        closesocket(this->client_socket);
        #else
        close(this->client_socket);
        #endif
        this->client_socket = -1;
    }
}

NiceHTTP::~NiceHTTP() {
    this->cleanup();
}

Router& NiceHTTP::getRouter() {
    return this->router;
}

void NiceHTTP::recv_http(const int& socket, std::string& head, std::string& body) {
    /* Parse basic http structure
    *  <header>\r\n\r\n<body>
    * head , body of request are the return values
    */
    head = "";
    body = "";
    int n;
    char buff[PKT_BLOCK_SIZE] = {0,};
    while ((n = recv(socket, buff, sizeof(buff), 0)))
    {
        std::string_view b(buff);
        // if we were processing body, continute to put data into body
        if (body != "") {
            body += b;
        } else { //else we process the headers
            size_t header_end = b.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                head += b.substr(0, header_end+4);
                body += b.substr(header_end+4);
            } else {
                head += b;
            }
        }
        // if the end of buff is null we assume to have read all data (not true for binary data)
        if (buff[PKT_BLOCK_SIZE-1] == 0) {
            break;
        }
        memset(buff, 0, sizeof(buff));
    }
}

void NiceHTTP::parsereq(const int& client_fd) {
    NLOG("Current Thread ID " << std::this_thread::get_id())
    // Receive request from client
    std::string req, body;
    this->recv_http(client_fd, req, body);
    http::Request r(req, body);
    NLOG(r.method << " " << r.uri)
    http::Response resp = this->router.handle(r);
    resp.headers.insert({"Server", "NiceHTTP"});
    resp.headers.insert({"Connection", "close"});
    std::string raw_resp = resp.toString();
    NLOG(resp.proto << " " << resp.code << " " << resp.message)
    //Send response to client
    send(client_fd, raw_resp.c_str(), raw_resp.length(), 0);
    NLOG("Exiting thread")
    #ifdef _WIN32
    closesocket(client_fd);
    #else
    close(client_fd);
    #endif
}

bool NiceHTTP::server_setup(const std::string& iface, const short& port) {
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

    // Enable port re-use
    int optval = 1;
    #ifdef _WIN32
    setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
    #else
    setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    #endif

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

void NiceHTTP::start(std::string iface, short port) {

    if (!this->server_setup(iface, port) || (this->server_socket == -1)) {
        return;
    }

    if (listen(this->server_socket, 1) == -1) {
        std::cerr << "listen(): Error listening on socket" << std::endl;
        this->cleanup();
        return;
    }

    dp::thread_pool pool(NICEHTTP_THREADS);
    int timeout = 1 * 60 * 1000; // wait 1 minute
    struct pollfd fds[1] = {0};
    fds[0].fd = this->server_socket;
    fds[0].events = POLLIN;

    //Loop forever waiting for new clients
    int client_socket;
    #ifdef _WIN32
    while(int rc = WSAPoll(fds, 1, timeout)) { // query socket status
        if (rc > 0) { //accept the incoming connection
            client_socket = accept(this->server_socket, NULL, NULL);
            if (client_socket > 0) {
                //Client accepted
                pool.enqueue_detach([this, &client_socket]() { this->parsereq(client_socket); });
            } else {
                std::cerr << "Failed to accept incoming connection" << std::endl;
            }
        } else if (rc < 0) {
            return;
        }
    }
    #else
    while(int rc = poll(fds, 1, timeout)) { // query socket status
        if (rc > 0) { //accept the incoming connection
            client_socket = accept(this->server_socket, NULL, NULL);
            if (client_socket > 0) {
                //Client accepted
                pool.enqueue_detach([this, &client_socket]() { this->parsereq(client_socket); });
            } else {
                std::cerr << "Failed to accept incoming connection" << std::endl;
            }
        } else if (rc < 0) {
            return;
        }
    }
    #endif

}

bool NiceHTTP::client_setup(const std::string& host, const short& port) {
    #ifdef _WIN32
    // Initialize WSA variables
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    #endif  
    this->client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (this->client_socket == -1)
    {
        std::cerr << "Error creating the socket" << std::endl;
        return false;
    }

    std::string ip;

    if (!is_ipaddr(host)) {
        // Solve domain name to ip
        try { 
            hostent *h = gethostbyname(host.c_str());
            if (h && (h->h_addrtype == AF_INET)) {
                unsigned char *addr = reinterpret_cast<unsigned char *>(h->h_addr_list[0]);
                std::stringstream ss;
                std::copy(addr, addr+4, std::ostream_iterator<unsigned int>(ss, "."));
                ip = ss.str();
                ip.erase(ip.length()-1);
            } else {
                std::cerr << "Cannot resolve domain to address" << std::endl;
                return false;
            }
        }
        catch (std::exception const &exc) {
            std::cerr << exc.what() << "\n";
            return false;
        }
    } else {
        ip = host;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    serverAddr.sin_port = htons(port);

    if (connect(this->client_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error binding the socket" << std::endl;
        this->cleanup();
        return false;
    }

    return true;
}

bool NiceHTTP::is_ipaddr(const std::string& host) {
    for (char c : host) {
        if (!(isdigit(c) || (c == '.'))) {
            return false;
        }
    }
    return true;
}

http::Response NiceHTTP::request(http::Request req, std::string host, short port) {
    /* Perform generic request req to host:port */
    if (!this->client_setup(host, port) || (this->client_socket == -1)) {
        throw std::runtime_error("Cannot connect to server");
    } 

    std::string raw_req = req.toString();
    //Send request
    send(this->client_socket, raw_req.c_str(), raw_req.length(), 0);

    std::string body;
    std::string resp;
    this->recv_http(this->client_socket, resp, body);

    http::Response r(resp, body);
    close(this->client_socket);

    return r;
}