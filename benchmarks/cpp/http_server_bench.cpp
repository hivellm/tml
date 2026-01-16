// HTTP Server Simulation Benchmark - C++ version
#include <cstdint>
#include <iostream>
#include <string>

constexpr int32_t METHOD_GET = 0;
constexpr int32_t METHOD_POST = 1;
constexpr int32_t METHOD_PUT = 2;

struct HttpRequest {
    int32_t method;
    std::string path;
    std::string host;
    std::string content_type;
    int64_t content_length;
    std::string user_agent;
    std::string accept;
    std::string connection;
    std::string body;
    int64_t request_id;

    static HttpRequest create(int32_t method, const std::string& path, int64_t request_id) {
        return HttpRequest{method,
                           path,
                           "localhost:8080",
                           "application/json",
                           0,
                           "CPP-Benchmark/1.0",
                           "application/json",
                           "keep-alive",
                           "",
                           request_id};
    }

    static HttpRequest create_with_body(int32_t method, const std::string& path,
                                        const std::string& body, int64_t request_id) {
        return HttpRequest{method,
                           path,
                           "localhost:8080",
                           "application/json",
                           static_cast<int64_t>(body.length()),
                           "CPP-Benchmark/1.0",
                           "application/json",
                           "keep-alive",
                           body,
                           request_id};
    }

    bool is_get() const {
        return method == METHOD_GET;
    }
    bool is_post() const {
        return method == METHOD_POST;
    }
    int64_t get_request_id() const {
        return request_id;
    }
    int64_t get_content_length() const {
        return content_length;
    }
};

struct HttpResponse {
    int32_t status_code;
    std::string status_text;
    std::string content_type;
    int64_t content_length;
    std::string server;
    std::string connection;
    std::string body;
    int64_t request_id;

    static HttpResponse ok(const std::string& body, int64_t request_id) {
        return HttpResponse{200,
                            "OK",
                            "application/json",
                            static_cast<int64_t>(body.length()),
                            "CPP-Server/1.0",
                            "keep-alive",
                            body,
                            request_id};
    }

    static HttpResponse created(const std::string& body, int64_t request_id) {
        return HttpResponse{201,
                            "Created",
                            "application/json",
                            static_cast<int64_t>(body.length()),
                            "CPP-Server/1.0",
                            "keep-alive",
                            body,
                            request_id};
    }

    static HttpResponse not_found(int64_t request_id) {
        std::string body = "{\"error\": \"Not Found\"}";
        return HttpResponse{404,
                            "Not Found",
                            "application/json",
                            static_cast<int64_t>(body.length()),
                            "CPP-Server/1.0",
                            "close",
                            body,
                            request_id};
    }

    static HttpResponse bad_request(const std::string&, int64_t request_id) {
        std::string body = "{\"error\": \"Bad Request\"}";
        return HttpResponse{400,
                            "Bad Request",
                            "application/json",
                            static_cast<int64_t>(body.length()),
                            "CPP-Server/1.0",
                            "close",
                            body,
                            request_id};
    }

    static HttpResponse server_error(int64_t request_id) {
        std::string body = "{\"error\": \"Internal Server Error\"}";
        return HttpResponse{500,
                            "Internal Server Error",
                            "application/json",
                            static_cast<int64_t>(body.length()),
                            "CPP-Server/1.0",
                            "close",
                            body,
                            request_id};
    }

    bool is_success() const {
        return status_code >= 200 && status_code < 300;
    }
    int64_t get_content_length() const {
        return content_length;
    }
};

struct ServerStats {
    int64_t total_requests = 0;
    int64_t successful_responses = 0;
    int64_t error_responses = 0;
    int64_t total_bytes_in = 0;
    int64_t total_bytes_out = 0;
    int64_t get_requests = 0;
    int64_t post_requests = 0;

    void record_request(const HttpRequest& req) {
        total_requests++;
        total_bytes_in += req.get_content_length();
        if (req.is_get())
            get_requests++;
        if (req.is_post())
            post_requests++;
    }

    void record_response(const HttpResponse& resp) {
        total_bytes_out += resp.get_content_length();
        if (resp.is_success())
            successful_responses++;
        else
            error_responses++;
    }

    int64_t get_success_rate() const {
        if (total_requests == 0)
            return 0;
        return (successful_responses * 100) / total_requests;
    }
};

HttpResponse handle_request(const HttpRequest& req, ServerStats& stats) {
    stats.record_request(req);
    int64_t req_id = req.get_request_id();
    int64_t route = req_id % 10;

    HttpResponse response = [&]() {
        switch (route) {
        case 0:
            return HttpResponse::ok("{\"status\": \"healthy\"}", req_id);
        case 1:
            if (req.is_get())
                return HttpResponse::ok("{\"users\": [{\"id\": 1, \"name\": \"Alice\"}]}", req_id);
            else if (req.is_post())
                return HttpResponse::created("{\"id\": 2, \"name\": \"Bob\"}", req_id);
            else
                return HttpResponse::bad_request("Method not allowed", req_id);
        case 2:
            return HttpResponse::ok("{\"products\": [{\"id\": 1, \"price\": 99.99}]}", req_id);
        case 3:
            return HttpResponse::ok("{\"orders\": []}", req_id);
        case 4:
            return HttpResponse::not_found(req_id);
        case 5:
            return HttpResponse::server_error(req_id);
        default:
            return HttpResponse::ok("{\"message\": \"OK\"}", req_id);
        }
    }();

    stats.record_response(response);
    return response;
}

int64_t bench_get_requests(int64_t n) {
    ServerStats stats;
    int64_t successful = 0;
    for (int64_t i = 0; i < n; i++) {
        HttpRequest req = HttpRequest::create(METHOD_GET, "/api/users", i);
        HttpResponse resp = handle_request(req, stats);
        if (resp.is_success())
            successful++;
    }
    return successful;
}

int64_t bench_post_requests(int64_t n) {
    ServerStats stats;
    int64_t successful = 0;
    std::string body = "{\"name\": \"TestUser\", \"email\": \"test@example.com\", \"age\": 25}";
    for (int64_t i = 0; i < n; i++) {
        HttpRequest req = HttpRequest::create_with_body(METHOD_POST, "/api/users", body, i);
        HttpResponse resp = handle_request(req, stats);
        if (resp.is_success())
            successful++;
    }
    return successful;
}

int64_t bench_mixed_workload(int64_t n) {
    ServerStats stats;
    std::string post_body = "{\"data\": \"payload\"}";
    for (int64_t i = 0; i < n; i++) {
        int64_t request_type = i % 10;
        HttpRequest req =
            (request_type < 7) ? HttpRequest::create(METHOD_GET, "/api/data", i)
            : (request_type < 9)
                ? HttpRequest::create_with_body(METHOD_POST, "/api/data", post_body, i)
                : HttpRequest::create(METHOD_PUT, "/api/data", i);
        handle_request(req, stats);
    }
    return stats.get_success_rate();
}

int64_t bench_object_creation(int64_t n) {
    int64_t count = 0;
    for (int64_t i = 0; i < n; i++) {
        HttpRequest req =
            HttpRequest::create_with_body(METHOD_POST, "/api/test", "{\"key\": \"value\"}", i);
        HttpResponse resp = HttpResponse::ok("{\"result\": \"success\", \"id\": 12345}", i);
        if (resp.is_success())
            count++;
    }
    return count;
}

int main() {
    std::cout << "=== C++ HTTP Server Benchmark ===" << std::endl << std::endl;
    std::cout << "Warming up..." << std::endl;
    bench_get_requests(100);

    std::cout << std::endl << "--- 1,000 Requests ---" << std::endl;
    std::cout << "GET requests:       " << bench_get_requests(1000) << std::endl;
    std::cout << "POST requests:      " << bench_post_requests(1000) << std::endl;
    std::cout << "Mixed workload:     " << bench_mixed_workload(1000) << std::endl;
    std::cout << "Object creation:    " << bench_object_creation(1000) << std::endl;

    std::cout << std::endl << "--- 100,000 Requests ---" << std::endl;
    std::cout << "GET requests:       " << bench_get_requests(100000) << std::endl;
    std::cout << "POST requests:      " << bench_post_requests(100000) << std::endl;
    std::cout << "Mixed workload:     " << bench_mixed_workload(100000) << std::endl;
    std::cout << "Object creation:    " << bench_object_creation(100000) << std::endl;

    std::cout << std::endl << "=== Benchmark Complete ===" << std::endl;
    return 0;
}
