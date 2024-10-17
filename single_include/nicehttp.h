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

#include <algorithm>
#include <concepts>
#include <deque>
#include <mutex>
#include <optional>

namespace dp {
    /**
     * @brief Simple concept for the Lockable and Basic Lockable types as defined by the C++
     * standard.
     * @details See https://en.cppreference.com/w/cpp/named_req/Lockable and
     * https://en.cppreference.com/w/cpp/named_req/BasicLockable for details.
     */
    template <typename Lock>
    concept is_lockable = requires(Lock&& lock) {
        lock.lock();
        lock.unlock();
        { lock.try_lock() } -> std::convertible_to<bool>;
    };

    template <typename T, typename Lock = std::mutex>
        requires is_lockable<Lock>
    class thread_safe_queue {
      public:
        using value_type = T;
        using size_type = typename std::deque<T>::size_type;

        thread_safe_queue() = default;

        void push_back(T&& value) {
            std::scoped_lock lock(mutex_);
            data_.push_back(std::forward<T>(value));
        }

        void push_front(T&& value) {
            std::scoped_lock lock(mutex_);
            data_.push_front(std::forward<T>(value));
        }

        [[nodiscard]] bool empty() const {
            std::scoped_lock lock(mutex_);
            return data_.empty();
        }

        [[nodiscard]] std::optional<T> pop_front() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto front = std::move(data_.front());
            data_.pop_front();
            return front;
        }

        [[nodiscard]] std::optional<T> pop_back() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        }

        [[nodiscard]] std::optional<T> steal() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        }

        void rotate_to_front(const T& item) {
            std::scoped_lock lock(mutex_);
            auto iter = std::find(data_.begin(), data_.end(), item);

            if (iter != data_.end()) {
                std::ignore = data_.erase(iter);
            }

            data_.push_front(item);
        }

        [[nodiscard]] std::optional<T> copy_front_and_rotate_to_back() {
            std::scoped_lock lock(mutex_);

            if (data_.empty()) return std::nullopt;

            auto front = data_.front();
            data_.pop_front();

            data_.push_back(front);

            return front;
        }

      private:
        std::deque<T> data_{};
        mutable Lock mutex_{};
    };
}  // namespace dp

// External dependency from: https://github.com/DeveloperPaul123/thread-pool/blob/0.6.2/
#include <atomic>
#include <barrier>
#include <concepts>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <semaphore>
#include <thread>
#include <type_traits>
#ifdef __has_include
#    if __has_include(<version>)
#        include <version>
#    endif
#endif

namespace dp {
    namespace details {

#ifdef __cpp_lib_move_only_function
        using default_function_type = std::move_only_function<void()>;
#else
        using default_function_type = std::function<void()>;
#endif
    }  // namespace details

    template <typename FunctionType = details::default_function_type,
              typename ThreadType = std::jthread>
        requires std::invocable<FunctionType> &&
                 std::is_same_v<void, std::invoke_result_t<FunctionType>>
    class thread_pool {
      public:
        explicit thread_pool(
            const unsigned int &number_of_threads = std::thread::hardware_concurrency())
            : tasks_(number_of_threads) {
            std::size_t current_id = 0;
            for (std::size_t i = 0; i < number_of_threads; ++i) {
                priority_queue_.push_back(size_t(current_id));
                try {
                    threads_.emplace_back([&, id = current_id](const std::stop_token &stop_tok) {
                        do {
                            // wait until signaled
                            tasks_[id].signal.acquire();

                            do {
                                // invoke the task
                                while (auto task = tasks_[id].tasks.pop_front()) {
                                    try {
                                        pending_tasks_.fetch_sub(1, std::memory_order_release);
                                        std::invoke(std::move(task.value()));
                                    } catch (...) {
                                    }
                                }

                                // try to steal a task
                                for (std::size_t j = 1; j < tasks_.size(); ++j) {
                                    const std::size_t index = (id + j) % tasks_.size();
                                    if (auto task = tasks_[index].tasks.steal()) {
                                        // steal a task
                                        pending_tasks_.fetch_sub(1, std::memory_order_release);
                                        std::invoke(std::move(task.value()));
                                        // stop stealing once we have invoked a stolen task
                                        break;
                                    }
                                }

                            } while (pending_tasks_.load(std::memory_order_acquire) > 0);

                            priority_queue_.rotate_to_front(id);

                        } while (!stop_tok.stop_requested());
                    });
                    // increment the thread id
                    ++current_id;

                } catch (...) {
                    // catch all

                    // remove one item from the tasks
                    tasks_.pop_back();

                    // remove our thread from the priority queue
                    std::ignore = priority_queue_.pop_back();
                }
            }
        }

        ~thread_pool() {
            // stop all threads
            for (std::size_t i = 0; i < threads_.size(); ++i) {
                threads_[i].request_stop();
                tasks_[i].signal.release();
                threads_[i].join();
            }
        }

        /// thread pool is non-copyable
        thread_pool(const thread_pool &) = delete;
        thread_pool &operator=(const thread_pool &) = delete;

        /**
         * @brief Enqueue a task into the thread pool that returns a result.
         * @details Note that task execution begins once the task is enqueued.
         * @tparam Function An invokable type.
         * @tparam Args Argument parameter pack
         * @tparam ReturnType The return type of the Function
         * @param f The callable function
         * @param args The parameters that will be passed (copied) to the function.
         * @return A std::future<ReturnType> that can be used to retrieve the returned value.
         */
        template <typename Function, typename... Args,
                  typename ReturnType = std::invoke_result_t<Function &&, Args &&...>>
            requires std::invocable<Function, Args...>
        [[nodiscard]] std::future<ReturnType> enqueue(Function f, Args... args) {
#if __cpp_lib_move_only_function
            // we can do this in C++23 because we now have support for move only functions
            std::promise<ReturnType> promise;
            auto future = promise.get_future();
            auto task = [func = std::move(f), ... largs = std::move(args),
                         promise = std::move(promise)]() mutable {
                try {
                    if constexpr (std::is_same_v<ReturnType, void>) {
                        func(largs...);
                        promise.set_value();
                    } else {
                        promise.set_value(func(largs...));
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            };
            enqueue_task(std::move(task));
            return future;
#else
            /*
             * use shared promise here so that we don't break the promise later (until C++23)
             *
             * with C++23 we can do the following:
             *
             * std::promise<ReturnType> promise;
             * auto future = promise.get_future();
             * auto task = [func = std::move(f), ...largs = std::move(args),
                              promise = std::move(promise)]() mutable {...};
             */
            auto shared_promise = std::make_shared<std::promise<ReturnType>>();
            auto task = [func = std::move(f), ... largs = std::move(args),
                         promise = shared_promise]() {
                try {
                    if constexpr (std::is_same_v<ReturnType, void>) {
                        func(largs...);
                        promise->set_value();
                    } else {
                        promise->set_value(func(largs...));
                    }

                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            };

            // get the future before enqueuing the task
            auto future = shared_promise->get_future();
            // enqueue the task
            enqueue_task(std::move(task));
            return future;
#endif
        }

        /**
         * @brief Enqueue a task to be executed in the thread pool that returns void.
         * @tparam Function An invokable type.
         * @tparam Args Argument parameter pack for Function
         * @param func The callable to be executed
         * @param args Arguments that will be passed to the function.
         */
        template <typename Function, typename... Args>
            requires std::invocable<Function, Args...> &&
                     std::is_same_v<void, std::invoke_result_t<Function &&, Args &&...>>
        void enqueue_detach(Function &&func, Args &&...args) {
            enqueue_task(
                std::move([f = std::forward<Function>(func),
                           ... largs = std::forward<Args>(args)]() mutable -> decltype(auto) {
                    // suppress exceptions
                    try {
                        std::invoke(f, largs...);
                    } catch (...) {
                    }
                }));
        }

        [[nodiscard]] auto size() const { return threads_.size(); }

      private:
        template <typename Function>
        void enqueue_task(Function &&f) {
            auto i_opt = priority_queue_.copy_front_and_rotate_to_back();
            if (!i_opt.has_value()) {
                // would only be a problem if there are zero threads
                return;
            }
            auto i = *(i_opt);
            pending_tasks_.fetch_add(1, std::memory_order_relaxed);
            tasks_[i].tasks.push_back(std::forward<Function>(f));
            tasks_[i].signal.release();
        }

        struct task_item {
            dp::thread_safe_queue<FunctionType> tasks{};
            std::binary_semaphore signal{0};
        };

        std::vector<ThreadType> threads_;
        std::deque<task_item> tasks_;
        dp::thread_safe_queue<std::size_t> priority_queue_;
        std::atomic_int_fast64_t pending_tasks_{};
    };

    /**
     * @example mandelbrot/source/main.cpp
     * Example showing how to use thread pool with tasks that return a value. Outputs a PPM image of
     * a mandelbrot.
     */
}  // namespace dp



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

#include <set>
#include <functional>
#include <string_view>
#include <iostream>
#include <regex>

class Route {
    /*
    * This class handle a single request calling the specified callback function.
    * Supports authentication token passed through "Authentication" header.
    * Doesn't support parameter parsing.
    */
private:
    std::function<http::Response(const http::Request &req)> func;
public:
    std::string_view method;
    std::string_view uri;
    std::string_view auth;
    Route(const std::string_view &method, const std::string_view &uri, const std::function<http::Response(const http::Request &req)> &func, std::string_view auth = "") {
        this->method = method;
        this->uri = uri;
        this->func = func;
        this->auth = auth;
    }
    Route(const Route& route) {
        method = route.method;
        uri = route.uri;
        func = route.func;
        auth = route.auth;
    }
    friend bool operator<(const Route& l, const Route& r) {
        return std::tie(l.method, l.uri) < std::tie(r.method, r.uri);
    }
    http::Response handle(const http::Request &req);
};

class Router {
    /* This class is a container for a list of Routes */
private:
    std::set<Route> routes;
public:
    void add(const Route &route); // add route
    void del(const Route &route); // delete route
    http::Response handle(const http::Request &req); // find the Route and call the callback function
    Router() {}
};

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
    static void signalHandler(int signum);
    void cleanup();
};



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
    //Send response to client
    send(this->client_socket, raw_req.c_str(), raw_req.length(), 0);

    std::string body;
    std::string resp;
    this->recv_http(this->client_socket, resp, body);

    http::Response r(resp, body);
    close(this->client_socket);

    return r;
}

void Router::add(const Route &route) {
    this->routes.insert(route);
}

void Router::del(const Route &route) {
    this->routes.erase(route);
}

http::Response Router::handle(const http::Request &req) {
    // handle the request finding the right route
    auto match = [&req](const Route &r){ //match condition to find the right route
        std::smatch m;
        std::regex rgx(r.uri.data());
        return ((req.method == r.method) &&
                (std::regex_match(req.uri.begin(), req.uri.end(), m, rgx)));
    };
    std::set<Route>::iterator result = std::ranges::find_if(this->routes, match);
    if (result != this->routes.end()) {
        Route r = *result;
        return r.handle(req);
    }
    std::map<std::string,std::string> headers;
    http::Response resp(404, "Not Found", PROTO_HTTP1, headers, false, 0);
    return resp;
}

http::Response Route::handle(const http::Request &req) {
    if (auth != "") { // Authentication is set for this route
        bool denied = true;
        for (const auto& h : req.headers) {
            if ((h.first == "authorization") && (h.second == auth)) {
                denied = false;
                break;
            }
        }
        if (denied) {
            std::map<std::string,std::string> headers;
            http::Response resp(401, "Unauthorized", PROTO_HTTP1, headers, false, 0);
            return resp;
        }
    }
    return this->func(req);
}