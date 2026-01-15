// HTTP Server Simulation Benchmark - Rust version
// Equivalent to TML's http_server_bench.tml

const METHOD_GET: i32 = 0;
const METHOD_POST: i32 = 1;
const METHOD_PUT: i32 = 2;

struct HttpRequest {
    method: i32,
    path: String,
    host: String,
    content_type: String,
    content_length: i64,
    user_agent: String,
    accept: String,
    connection: String,
    body: String,
    request_id: i64,
}

impl HttpRequest {
    fn create(method: i32, path: &str, request_id: i64) -> Self {
        HttpRequest {
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

    fn create_with_body(method: i32, path: &str, body: &str, request_id: i64) -> Self {
        HttpRequest {
            method,
            path: path.to_string(),
            host: "localhost:8080".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            user_agent: "Rust-Benchmark/1.0".to_string(),
            accept: "application/json".to_string(),
            connection: "keep-alive".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn is_get(&self) -> bool { self.method == METHOD_GET }
    fn is_post(&self) -> bool { self.method == METHOD_POST }
    fn get_request_id(&self) -> i64 { self.request_id }
    fn get_content_length(&self) -> i64 { self.content_length }
}

struct HttpResponse {
    status_code: i32,
    status_text: String,
    content_type: String,
    content_length: i64,
    server: String,
    connection: String,
    body: String,
    request_id: i64,
}

impl HttpResponse {
    fn ok(body: &str, request_id: i64) -> Self {
        HttpResponse {
            status_code: 200,
            status_text: "OK".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "keep-alive".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn created(body: &str, request_id: i64) -> Self {
        HttpResponse {
            status_code: 201,
            status_text: "Created".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "keep-alive".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn not_found(request_id: i64) -> Self {
        let body = r#"{"error": "Not Found"}"#;
        HttpResponse {
            status_code: 404,
            status_text: "Not Found".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn bad_request(_message: &str, request_id: i64) -> Self {
        let body = r#"{"error": "Bad Request"}"#;
        HttpResponse {
            status_code: 400,
            status_text: "Bad Request".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn server_error(request_id: i64) -> Self {
        let body = r#"{"error": "Internal Server Error"}"#;
        HttpResponse {
            status_code: 500,
            status_text: "Internal Server Error".to_string(),
            content_type: "application/json".to_string(),
            content_length: body.len() as i64,
            server: "Rust-Server/1.0".to_string(),
            connection: "close".to_string(),
            body: body.to_string(),
            request_id,
        }
    }

    fn is_success(&self) -> bool {
        self.status_code >= 200 && self.status_code < 300
    }

    fn get_content_length(&self) -> i64 { self.content_length }
}

struct ServerStats {
    total_requests: i64,
    successful_responses: i64,
    error_responses: i64,
    total_bytes_in: i64,
    total_bytes_out: i64,
    get_requests: i64,
    post_requests: i64,
}

impl ServerStats {
    fn new() -> Self {
        ServerStats {
            total_requests: 0,
            successful_responses: 0,
            error_responses: 0,
            total_bytes_in: 0,
            total_bytes_out: 0,
            get_requests: 0,
            post_requests: 0,
        }
    }

    fn record_request(&mut self, req: &HttpRequest) {
        self.total_requests += 1;
        self.total_bytes_in += req.get_content_length();
        if req.is_get() { self.get_requests += 1; }
        if req.is_post() { self.post_requests += 1; }
    }

    fn record_response(&mut self, resp: &HttpResponse) {
        self.total_bytes_out += resp.get_content_length();
        if resp.is_success() {
            self.successful_responses += 1;
        } else {
            self.error_responses += 1;
        }
    }

    fn get_success_rate(&self) -> i64 {
        if self.total_requests == 0 { return 0; }
        (self.successful_responses * 100) / self.total_requests
    }
}

fn handle_request(req: &HttpRequest, stats: &mut ServerStats) -> HttpResponse {
    stats.record_request(req);

    let req_id = req.get_request_id();
    let route = req_id % 10;

    let response = match route {
        0 => HttpResponse::ok(r#"{"status": "healthy"}"#, req_id),
        1 => {
            if req.is_get() {
                HttpResponse::ok(r#"{"users": [{"id": 1, "name": "Alice"}]}"#, req_id)
            } else if req.is_post() {
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

fn bench_get_requests(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let mut successful = 0i64;

    for i in 0..n {
        let req = HttpRequest::create(METHOD_GET, "/api/users", i);
        let resp = handle_request(&req, &mut stats);
        if resp.is_success() { successful += 1; }
    }

    successful
}

fn bench_post_requests(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let mut successful = 0i64;
    let body = r#"{"name": "TestUser", "email": "test@example.com", "age": 25}"#;

    for i in 0..n {
        let req = HttpRequest::create_with_body(METHOD_POST, "/api/users", body, i);
        let resp = handle_request(&req, &mut stats);
        if resp.is_success() { successful += 1; }
    }

    successful
}

fn bench_mixed_workload(n: i64) -> i64 {
    let mut stats = ServerStats::new();
    let post_body = r#"{"data": "payload"}"#;

    for i in 0..n {
        let request_type = i % 10;
        let req = if request_type < 7 {
            HttpRequest::create(METHOD_GET, "/api/data", i)
        } else if request_type < 9 {
            HttpRequest::create_with_body(METHOD_POST, "/api/data", post_body, i)
        } else {
            HttpRequest::create(METHOD_PUT, "/api/data", i)
        };
        let _ = handle_request(&req, &mut stats);
    }

    stats.get_success_rate()
}

fn bench_object_creation(n: i64) -> i64 {
    let mut count = 0i64;

    for i in 0..n {
        let _req = HttpRequest::create_with_body(METHOD_POST, "/api/test", r#"{"key": "value"}"#, i);
        let resp = HttpResponse::ok(r#"{"result": "success", "id": 12345}"#, i);
        if resp.is_success() { count += 1; }
    }

    count
}

fn main() {
    println!("=== Rust HTTP Server Benchmark ===\n");

    println!("Warming up...");
    let _ = bench_get_requests(100);

    println!("\n--- 1,000 Requests ---");
    println!("GET requests:       {}", bench_get_requests(1000));
    println!("POST requests:      {}", bench_post_requests(1000));
    println!("Mixed workload:     {}", bench_mixed_workload(1000));
    println!("Object creation:    {}", bench_object_creation(1000));

    println!("\n--- 100,000 Requests ---");
    println!("GET requests:       {}", bench_get_requests(100000));
    println!("POST requests:      {}", bench_post_requests(100000));
    println!("Mixed workload:     {}", bench_mixed_workload(100000));
    println!("Object creation:    {}", bench_object_creation(100000));

    println!("\n=== Benchmark Complete ===");
}
