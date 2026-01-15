//! HTTP Server Simulation Benchmark - Rust
//!
//! Tests Rust's capacity to handle complex object creation at scale,
//! simulating thousands of HTTP requests per second.

use criterion::{black_box, criterion_group, criterion_main, Criterion, BenchmarkId};

// ============================================================================
// HTTP Types
// ============================================================================

#[derive(Clone, Copy, PartialEq)]
pub enum HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
}

impl HttpMethod {
    pub fn name(&self) -> &'static str {
        match self {
            HttpMethod::Get => "GET",
            HttpMethod::Post => "POST",
            HttpMethod::Put => "PUT",
            HttpMethod::Delete => "DELETE",
            HttpMethod::Patch => "PATCH",
        }
    }
}

// ============================================================================
// HttpHeader
// ============================================================================

pub struct HttpHeader {
    pub name: String,
    pub value: String,
}

impl HttpHeader {
    pub fn new(name: &str, value: &str) -> Self {
        Self {
            name: name.to_string(),
            value: value.to_string(),
        }
    }
}

// ============================================================================
// HttpRequest
// ============================================================================

pub struct HttpRequest {
    pub method: HttpMethod,
    pub path: String,
    pub host: String,
    pub content_type: String,
    pub content_length: i64,
    pub user_agent: String,
    pub accept: String,
    pub connection: String,
    pub body: String,
    pub request_id: i64,
}

impl HttpRequest {
    pub fn new(method: HttpMethod, path: &str, request_id: i64) -> Self {
        Self {
            method,
            path: path.to_string(),
            host: "localhost:8080".to_string(),
            content_type: "application/json".to_string(),
            content_length: 0,
            user_agent: "Rust-Benchmark/1.0".to_string(),
            accept: "application/json".to_string(),
            connection: "keep-alive".to_string(),
            body: String::new(),
            request_id,
        }
    }

    pub fn with_body(method: HttpMethod, path: &str, body: &str, request_id: i64) -> Self {
        let body_string = body.to_string();
        let content_length = body_string.len() as i64;
        Self {
            method,
            path: path.to_string(),
            host: "localhost:8080".to_string(),
            content_type: "application/json".to_string(),
            content_length,
            user_agent: "Rust-Benchmark/1.0".to_string(),
            accept: "application/json".to_string(),
            connection: "keep-alive".to_string(),
            body: body_string,
            request_id,
        }
    }

    pub fn is_get(&self) -> bool {
        self.method == HttpMethod::Get
    }

    pub fn is_post(&self) -> bool {
        self.method == HttpMethod::Post
    }
}

// ============================================================================
// HttpResponse
// ============================================================================

pub struct HttpResponse {
    pub status_code: i32,
    pub status_text: String,
    pub content_type: String,
    pub content_length: i64,
    pub server: String,
    pub connection: String,
    pub body: String,
    pub request_id: i64,
}

impl HttpResponse {
    pub fn ok(body: &str, request_id: i64) -> Self {
        let body_string = body.to_string();
        Self {
            status_code: 200,
            status_text: "OK".to_string(),
            content_type: "application/json".to_string(),
            content_length: body_string.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "keep-alive".to_string(),
            body: body_string,
            request_id,
        }
    }

    pub fn created(body: &str, request_id: i64) -> Self {
        let body_string = body.to_string();
        Self {
            status_code: 201,
            status_text: "Created".to_string(),
            content_type: "application/json".to_string(),
            content_length: body_string.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "keep-alive".to_string(),
            body: body_string,
            request_id,
        }
    }

    pub fn not_found(request_id: i64) -> Self {
        let body = r#"{"error": "Not Found"}"#.to_string();
        Self {
            status_code: 404,
            status_text: "Not Found".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body,
            request_id,
        }
    }

    pub fn bad_request(message: &str, request_id: i64) -> Self {
        let body = format!(r#"{{"error": "{}"}}"#, message);
        Self {
            status_code: 400,
            status_text: "Bad Request".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body,
            request_id,
        }
    }

    pub fn server_error(request_id: i64) -> Self {
        let body = r#"{"error": "Internal Server Error"}"#.to_string();
        Self {
            status_code: 500,
            status_text: "Internal Server Error".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body,
            request_id,
        }
    }

    pub fn is_success(&self) -> bool {
        self.status_code >= 200 && self.status_code < 300
    }
}

// ============================================================================
// RequestContext
// ============================================================================

pub struct RequestContext {
    pub request: HttpRequest,
    pub response_sent: bool,
    pub processing_time: i64,
}

impl RequestContext {
    pub fn new(request: HttpRequest) -> Self {
        Self {
            request,
            response_sent: false,
            processing_time: 0,
        }
    }

    pub fn mark_complete(&mut self, processing_ns: i64) {
        self.response_sent = true;
        self.processing_time = processing_ns;
    }

    pub fn is_complete(&self) -> bool {
        self.response_sent
    }
}

// ============================================================================
// Router
// ============================================================================

pub struct Router {
    pub route_count: i32,
}

impl Router {
    pub fn new() -> Self {
        Self { route_count: 0 }
    }

    pub fn add_route(&mut self) {
        self.route_count += 1;
    }

    pub fn match_route(&self, path: &str) -> i32 {
        let mut hash: i32 = 0;
        for _ in path.chars() {
            hash = (hash.wrapping_mul(31).wrapping_add(1)) % 100;
        }
        hash % 10
    }
}

// ============================================================================
// ServerStats
// ============================================================================

pub struct ServerStats {
    pub total_requests: i64,
    pub successful_responses: i64,
    pub error_responses: i64,
    pub total_bytes_in: i64,
    pub total_bytes_out: i64,
    pub get_requests: i64,
    pub post_requests: i64,
}

impl ServerStats {
    pub fn new() -> Self {
        Self {
            total_requests: 0,
            successful_responses: 0,
            error_responses: 0,
            total_bytes_in: 0,
            total_bytes_out: 0,
            get_requests: 0,
            post_requests: 0,
        }
    }

    pub fn record_request(&mut self, req: &HttpRequest) {
        self.total_requests += 1;
        self.total_bytes_in += req.content_length;
        if req.is_get() {
            self.get_requests += 1;
        }
        if req.is_post() {
            self.post_requests += 1;
        }
    }

    pub fn record_response(&mut self, resp: &HttpResponse) {
        self.total_bytes_out += resp.content_length;
        if resp.is_success() {
            self.successful_responses += 1;
        } else {
            self.error_responses += 1;
        }
    }

    pub fn get_success_rate(&self) -> i64 {
        if self.total_requests == 0 {
            return 0;
        }
        (self.successful_responses * 100) / self.total_requests
    }
}

// ============================================================================
// Request Handler
// ============================================================================

fn handle_request(req: &HttpRequest, stats: &mut ServerStats) -> HttpResponse {
    stats.record_request(req);

    let req_id = req.request_id;
    let route = (req_id % 10) as i32;

    let response = match route {
        0 => HttpResponse::ok(r#"{"status": "healthy"}"#, req_id),
        1 => {
            if req.method == HttpMethod::Get {
                HttpResponse::ok(r#"{"users": [{"id": 1, "name": "Alice"}]}"#, req_id)
            } else if req.method == HttpMethod::Post {
                HttpResponse::created(r#"{"id": 2, "name": "Bob"}"#, req_id)
            } else {
                HttpResponse::bad_request("Method not allowed", req_id)
            }
        }
        2 => HttpResponse::ok(r#"{"products": [{"id": 1, "price": 99.99}]}"#, req_id),
        3 => HttpResponse::ok(r#"{"orders": []}"#, req_id),
        4 => HttpResponse::not_found(req_id),
        5 => HttpResponse::server_error(req_id),
        _ => HttpResponse::ok(r#"{"message": "OK"}"#, req_id),
    };

    stats.record_response(&response);
    response
}

// ============================================================================
// Benchmark Functions
// ============================================================================

fn bench_get_requests(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let mut successful = 0i64;

    for i in 0..n {
        let req = HttpRequest::new(HttpMethod::Get, "/api/users", i);
        let resp = handle_request(&req, &mut stats);
        if resp.is_success() {
            successful += 1;
        }
    }

    successful
}

fn bench_post_requests(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let mut successful = 0i64;
    let body = r#"{"name": "TestUser", "email": "test@example.com", "age": 25}"#;

    for i in 0..n {
        let req = HttpRequest::with_body(HttpMethod::Post, "/api/users", body, i);
        let resp = handle_request(&req, &mut stats);
        if resp.is_success() {
            successful += 1;
        }
    }

    successful
}

fn bench_mixed_workload(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let post_body = r#"{"data": "payload"}"#;

    for i in 0..n {
        let request_type = i % 10;
        let req = if request_type < 7 {
            HttpRequest::new(HttpMethod::Get, "/api/data", i)
        } else if request_type < 9 {
            HttpRequest::with_body(HttpMethod::Post, "/api/data", post_body, i)
        } else {
            HttpRequest::new(HttpMethod::Put, "/api/data", i)
        };

        handle_request(&req, &mut stats);
    }

    stats.get_success_rate()
}

fn bench_with_context(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let mut completed = 0i64;

    for i in 0..n {
        let req = HttpRequest::new(HttpMethod::Get, "/api/benchmark", i);
        let mut ctx = RequestContext::new(req);

        let resp = handle_request(&ctx.request, &mut stats);
        ctx.mark_complete(100);

        if ctx.is_complete() && resp.is_success() {
            completed += 1;
        }
    }

    completed
}

fn bench_object_creation(n: i64) -> i64 {
    let mut count = 0i64;

    for i in 0..n {
        let req = HttpRequest::with_body(HttpMethod::Post, "/api/test", r#"{"key": "value"}"#, i);
        let resp = HttpResponse::ok(r#"{"result": "success", "id": 12345}"#, i);
        let ctx = RequestContext::new(req);

        let _h1 = HttpHeader::new("Content-Type", "application/json");
        let _h2 = HttpHeader::new("Authorization", "Bearer token123");
        let _h3 = HttpHeader::new("X-Request-ID", "req-12345");

        if resp.is_success() && !ctx.is_complete() {
            count += 1;
        }
    }

    count
}

fn bench_routing(n: i64) -> i64 {
    let mut router = Router::new();
    for _ in 0..10 {
        router.add_route();
    }

    let mut matches = 0i64;
    for _ in 0..n {
        let route_idx = router.match_route("/api/endpoint");
        if route_idx >= 0 && route_idx < 10 {
            matches += 1;
        }
    }

    matches
}

// ============================================================================
// Criterion Benchmarks
// ============================================================================

fn criterion_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("http_server");

    // Configure for high-throughput benchmarking
    group.sample_size(50);

    for size in [1000i64, 10000, 100000].iter() {
        group.bench_with_input(
            BenchmarkId::new("get_requests", size),
            size,
            |b, &n| b.iter(|| bench_get_requests(black_box(n))),
        );

        group.bench_with_input(
            BenchmarkId::new("post_requests", size),
            size,
            |b, &n| b.iter(|| bench_post_requests(black_box(n))),
        );

        group.bench_with_input(
            BenchmarkId::new("mixed_workload", size),
            size,
            |b, &n| b.iter(|| bench_mixed_workload(black_box(n))),
        );

        group.bench_with_input(
            BenchmarkId::new("with_context", size),
            size,
            |b, &n| b.iter(|| bench_with_context(black_box(n))),
        );

        group.bench_with_input(
            BenchmarkId::new("object_creation", size),
            size,
            |b, &n| b.iter(|| bench_object_creation(black_box(n))),
        );

        group.bench_with_input(
            BenchmarkId::new("routing", size),
            size,
            |b, &n| b.iter(|| bench_routing(black_box(n))),
        );
    }

    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
