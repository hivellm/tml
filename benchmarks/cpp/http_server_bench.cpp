// HTTP Server Simulation Benchmark - C++
//
// Tests C++'s capacity to handle complex object creation at scale,
// simulating thousands of HTTP requests per second.
//
// Compile: g++ -O3 -std=c++17 -o http_server_bench http_server_bench.cpp
// Run: ./http_server_bench

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

// ============================================================================
// HTTP Constants
// ============================================================================

enum class HttpMethod { GET = 1, POST = 2, PUT = 3, DELETE = 4, PATCH = 5 };

enum class HttpStatus {
    OK = 200,
    CREATED = 201,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
};

// ============================================================================
// HttpHeader
// ============================================================================

class HttpHeader {
public:
    std::string name;
    std::string value;

    HttpHeader(const std::string& n, const std::string& v) : name(n), value(v) {}

    const std::string& get_name() const {
        return name;
    }
    const std::string& get_value() const {
        return value;
    }
};

// ============================================================================
// HttpRequest
// ============================================================================

class HttpRequest {
public:
    HttpMethod method;
    std::string path;
    std::string host;
    std::string content_type;
    int64_t content_length;
    std::string user_agent;
    std::string accept;
    std::string connection;
    std::string body;
    int64_t request_id;

    static HttpRequest create(HttpMethod method, const std::string& path, int64_t request_id) {
        HttpRequest req;
        req.method = method;
        req.path = path;
        req.host = "localhost:8080";
        req.content_type = "application/json";
        req.content_length = 0;
        req.user_agent = "CPP-Benchmark/1.0";
        req.accept = "application/json";
        req.connection = "keep-alive";
        req.body = "";
        req.request_id = request_id;
        return req;
    }

    static HttpRequest create_with_body(HttpMethod method, const std::string& path,
                                        const std::string& body, int64_t request_id) {
        HttpRequest req;
        req.method = method;
        req.path = path;
        req.host = "localhost:8080";
        req.content_type = "application/json";
        req.content_length = static_cast<int64_t>(body.length());
        req.user_agent = "CPP-Benchmark/1.0";
        req.accept = "application/json";
        req.connection = "keep-alive";
        req.body = body;
        req.request_id = request_id;
        return req;
    }

    HttpMethod get_method() const {
        return method;
    }
    const std::string& get_path() const {
        return path;
    }
    const std::string& get_body() const {
        return body;
    }
    int64_t get_request_id() const {
        return request_id;
    }

    bool is_get() const {
        return method == HttpMethod::GET;
    }
    bool is_post() const {
        return method == HttpMethod::POST;
    }
    bool has_body() const {
        return content_length > 0;
    }

    std::string method_name() const {
        switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE:
            return "DELETE";
        case HttpMethod::PATCH:
            return "PATCH";
        default:
            return "UNKNOWN";
        }
    }
};

// ============================================================================
// HttpResponse
// ============================================================================

class HttpResponse {
public:
    int32_t status_code;
    std::string status_text;
    std::string content_type;
    int64_t content_length;
    std::string server;
    std::string connection;
    std::string body;
    int64_t request_id;

    static HttpResponse ok(const std::string& body, int64_t request_id) {
        HttpResponse resp;
        resp.status_code = 200;
        resp.status_text = "OK";
        resp.content_type = "application/json";
        resp.content_length = static_cast<int64_t>(body.length());
        resp.server = "CPP-Server/1.0";
        resp.connection = "keep-alive";
        resp.body = body;
        resp.request_id = request_id;
        return resp;
    }

    static HttpResponse created(const std::string& body, int64_t request_id) {
        HttpResponse resp;
        resp.status_code = 201;
        resp.status_text = "Created";
        resp.content_type = "application/json";
        resp.content_length = static_cast<int64_t>(body.length());
        resp.server = "CPP-Server/1.0";
        resp.connection = "keep-alive";
        resp.body = body;
        resp.request_id = request_id;
        return resp;
    }

    static HttpResponse not_found(int64_t request_id) {
        std::string body = R"({"error": "Not Found"})";
        HttpResponse resp;
        resp.status_code = 404;
        resp.status_text = "Not Found";
        resp.content_type = "application/json";
        resp.content_length = static_cast<int64_t>(body.length());
        resp.server = "CPP-Server/1.0";
        resp.connection = "close";
        resp.body = body;
        resp.request_id = request_id;
        return resp;
    }

    static HttpResponse bad_request(const std::string& message, int64_t request_id) {
        std::string body = R"({"error": ")" + message + R"("})";
        HttpResponse resp;
        resp.status_code = 400;
        resp.status_text = "Bad Request";
        resp.content_type = "application/json";
        resp.content_length = static_cast<int64_t>(body.length());
        resp.server = "CPP-Server/1.0";
        resp.connection = "close";
        resp.body = body;
        resp.request_id = request_id;
        return resp;
    }

    static HttpResponse server_error(int64_t request_id) {
        std::string body = R"({"error": "Internal Server Error"})";
        HttpResponse resp;
        resp.status_code = 500;
        resp.status_text = "Internal Server Error";
        resp.content_type = "application/json";
        resp.content_length = static_cast<int64_t>(body.length());
        resp.server = "CPP-Server/1.0";
        resp.connection = "close";
        resp.body = body;
        resp.request_id = request_id;
        return resp;
    }

    int32_t get_status() const {
        return status_code;
    }
    const std::string& get_body() const {
        return body;
    }
    bool is_success() const {
        return status_code >= 200 && status_code < 300;
    }
    bool is_error() const {
        return status_code >= 400;
    }
};

// ============================================================================
// RequestContext
// ============================================================================

class RequestContext {
public:
    HttpRequest request;
    bool response_sent;
    int64_t start_time;
    int64_t processing_time;

    static RequestContext create(const HttpRequest& req) {
        RequestContext ctx;
        ctx.request = req;
        ctx.response_sent = false;
        ctx.start_time = 0;
        ctx.processing_time = 0;
        return ctx;
    }

    const HttpRequest& get_request() const {
        return request;
    }
    void mark_complete(int64_t processing_ns) {
        response_sent = true;
        processing_time = processing_ns;
    }
    bool is_complete() const {
        return response_sent;
    }
};

// ============================================================================
// Router
// ============================================================================

class Router {
public:
    int32_t route_count;

    Router() : route_count(0) {}

    void add_route() {
        route_count++;
    }

    int32_t match_route(const std::string& path) const {
        int32_t hash = 0;
        for (char c : path) {
            hash = (hash * 31 + 1) % 100;
        }
        return hash % 10;
    }
};

// ============================================================================
// ServerStats
// ============================================================================

class ServerStats {
public:
    int64_t total_requests;
    int64_t successful_responses;
    int64_t error_responses;
    int64_t total_bytes_in;
    int64_t total_bytes_out;
    int64_t get_requests;
    int64_t post_requests;

    ServerStats()
        : total_requests(0), successful_responses(0), error_responses(0), total_bytes_in(0),
          total_bytes_out(0), get_requests(0), post_requests(0) {}

    void record_request(const HttpRequest& req) {
        total_requests++;
        total_bytes_in += req.content_length;
        if (req.is_get())
            get_requests++;
        if (req.is_post())
            post_requests++;
    }

    void record_response(const HttpResponse& resp) {
        total_bytes_out += resp.content_length;
        if (resp.is_success()) {
            successful_responses++;
        } else {
            error_responses++;
        }
    }

    int64_t get_total_requests() const {
        return total_requests;
    }
    int64_t get_success_rate() const {
        if (total_requests == 0)
            return 0;
        return (successful_responses * 100) / total_requests;
    }
};

// ============================================================================
// Request Handler
// ============================================================================

HttpResponse handle_request(const HttpRequest& req, ServerStats& stats) {
    stats.record_request(req);

    int64_t req_id = req.get_request_id();
    int32_t route = static_cast<int32_t>(req_id % 10);

    HttpResponse response;
    if (route == 0) {
        response = HttpResponse::ok(R"({"status": "healthy"})", req_id);
    } else if (route == 1) {
        if (req.get_method() == HttpMethod::GET) {
            response = HttpResponse::ok(R"({"users": [{"id": 1, "name": "Alice"}]})", req_id);
        } else if (req.get_method() == HttpMethod::POST) {
            response = HttpResponse::created(R"({"id": 2, "name": "Bob"})", req_id);
        } else {
            response = HttpResponse::bad_request("Method not allowed", req_id);
        }
    } else if (route == 2) {
        response = HttpResponse::ok(R"({"products": [{"id": 1, "price": 99.99}]})", req_id);
    } else if (route == 3) {
        response = HttpResponse::ok(R"({"orders": []})", req_id);
    } else if (route == 4) {
        response = HttpResponse::not_found(req_id);
    } else if (route == 5) {
        response = HttpResponse::server_error(req_id);
    } else {
        response = HttpResponse::ok(R"({"message": "OK"})", req_id);
    }

    stats.record_response(response);
    return response;
}

// ============================================================================
// Benchmark Functions
// ============================================================================

int64_t bench_get_requests(int64_t n) {
    ServerStats stats;
    int64_t successful = 0;

    for (int64_t i = 0; i < n; ++i) {
        auto req = HttpRequest::create(HttpMethod::GET, "/api/users", i);
        auto resp = handle_request(req, stats);
        if (resp.is_success())
            successful++;
    }

    return successful;
}

int64_t bench_post_requests(int64_t n) {
    ServerStats stats;
    int64_t successful = 0;
    std::string body = R"({"name": "TestUser", "email": "test@example.com", "age": 25})";

    for (int64_t i = 0; i < n; ++i) {
        auto req = HttpRequest::create_with_body(HttpMethod::POST, "/api/users", body, i);
        auto resp = handle_request(req, stats);
        if (resp.is_success())
            successful++;
    }

    return successful;
}

int64_t bench_mixed_workload(int64_t n) {
    ServerStats stats;
    std::string post_body = R"({"data": "payload"})";

    for (int64_t i = 0; i < n; ++i) {
        int64_t request_type = i % 10;
        HttpRequest req;

        if (request_type < 7) {
            req = HttpRequest::create(HttpMethod::GET, "/api/data", i);
        } else if (request_type < 9) {
            req = HttpRequest::create_with_body(HttpMethod::POST, "/api/data", post_body, i);
        } else {
            req = HttpRequest::create(HttpMethod::PUT, "/api/data", i);
        }

        handle_request(req, stats);
    }

    return stats.get_success_rate();
}

int64_t bench_with_context(int64_t n) {
    ServerStats stats;
    int64_t completed = 0;

    for (int64_t i = 0; i < n; ++i) {
        auto req = HttpRequest::create(HttpMethod::GET, "/api/benchmark", i);
        auto ctx = RequestContext::create(req);

        auto resp = handle_request(ctx.get_request(), stats);
        ctx.mark_complete(100);

        if (ctx.is_complete() && resp.is_success())
            completed++;
    }

    return completed;
}

int64_t bench_object_creation(int64_t n) {
    int64_t count = 0;

    for (int64_t i = 0; i < n; ++i) {
        auto req =
            HttpRequest::create_with_body(HttpMethod::POST, "/api/test", R"({"key": "value"})", i);
        auto resp = HttpResponse::ok(R"({"result": "success", "id": 12345})", i);
        auto ctx = RequestContext::create(req);

        HttpHeader h1("Content-Type", "application/json");
        HttpHeader h2("Authorization", "Bearer token123");
        HttpHeader h3("X-Request-ID", "req-12345");

        if (resp.is_success() && !ctx.is_complete())
            count++;
    }

    return count;
}

int64_t bench_routing(int64_t n) {
    Router router;
    for (int r = 0; r < 10; ++r)
        router.add_route();

    int64_t matches = 0;
    for (int64_t i = 0; i < n; ++i) {
        int32_t route_idx = router.match_route("/api/endpoint");
        if (route_idx >= 0 && route_idx < 10)
            matches++;
    }

    return matches;
}

// ============================================================================
// Timing Helper
// ============================================================================

template <typename Func> void run_benchmark(const char* name, Func func, int64_t n) {
    auto start = std::chrono::high_resolution_clock::now();
    int64_t result = func(n);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double ms = duration.count() / 1000.0;
    double rps = (n * 1000.0) / ms;

    std::cout << name << ": " << result << " (";
    std::cout << std::fixed;
    std::cout.precision(2);
    std::cout << ms << " ms, " << rps << " req/s)" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== C++ HTTP Server Simulation Benchmark ===" << std::endl << std::endl;

    // Warm-up
    std::cout << "Warming up..." << std::endl;
    bench_get_requests(1000);

    // 10K requests
    std::cout << std::endl << "--- 10,000 Requests ---" << std::endl;
    run_benchmark("GET requests    ", bench_get_requests, 10000);
    run_benchmark("POST requests   ", bench_post_requests, 10000);
    run_benchmark("Mixed workload  ", bench_mixed_workload, 10000);
    run_benchmark("With context    ", bench_with_context, 10000);
    run_benchmark("Object creation ", bench_object_creation, 10000);
    run_benchmark("Routing         ", bench_routing, 10000);

    // 100K requests
    std::cout << std::endl << "--- 100,000 Requests ---" << std::endl;
    run_benchmark("GET requests    ", bench_get_requests, 100000);
    run_benchmark("Mixed workload  ", bench_mixed_workload, 100000);
    run_benchmark("Object creation ", bench_object_creation, 100000);

    // 1M requests
    std::cout << std::endl << "--- 1,000,000 Requests (Stress Test) ---" << std::endl;
    run_benchmark("GET requests    ", bench_get_requests, 1000000);
    run_benchmark("Object creation ", bench_object_creation, 1000000);

    std::cout << std::endl << "=== Benchmark Complete ===" << std::endl;

    return 0;
}
