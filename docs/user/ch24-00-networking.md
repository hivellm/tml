# Networking and HTTP

TML provides a complete networking stack covering TCP, UDP, DNS resolution, and a full HTTP client and server framework.

## IP Addresses

The `std::net::ip` module provides IP address types:

```tml
use std::net::ip::{Ipv4Addr, Ipv6Addr, IpAddr, SocketAddr}

// IPv4
let localhost = Ipv4Addr.new(127, 0, 0, 1)
let any = Ipv4Addr.UNSPECIFIED          // 0.0.0.0
println(localhost.to_string())           // "127.0.0.1"
println(localhost.is_loopback().to_string())  // "true"

// IPv6
let v6 = Ipv6Addr.LOCALHOST             // ::1

// Socket address (IP + port)
let addr = SocketAddr.new(IpAddr.V4(localhost), 8080)
```

## TCP

### TCP Client

```tml
use std::net::{TcpStream}

let stream = TcpStream.connect("127.0.0.1:8080")!
stream.write("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n".as_bytes())!
let response = stream.read_all()!
stream.close()
```

### TCP Server

```tml
use std::net::{TcpListener}
use std::thread::spawn

let listener = TcpListener.bind("0.0.0.0:8080")!
println("Listening on port 8080")

loop {
    let (stream, addr) = listener.accept()!
    println("Connection from " + addr.to_string())

    spawn(do() {
        handle_client(stream)
    })
}

func handle_client(stream: TcpStream) {
    let request = stream.read_all()!
    stream.write("HTTP/1.1 200 OK\r\n\r\nHello!".as_bytes())!
    stream.close()
}
```

## UDP

```tml
use std::net::{UdpSocket}

// Sender
let socket = UdpSocket.bind("0.0.0.0:0")!
socket.send_to("Hello".as_bytes(), "127.0.0.1:9000")!

// Receiver
let listener = UdpSocket.bind("0.0.0.0:9000")!
let (data, addr) = listener.recv_from()!
println("Received from " + addr.to_string())
```

## HTTP Client

The `std::http` module provides a high-level HTTP client:

```tml
use std::http::{HttpClient, Request, Method, Headers}

let client = HttpClient.new()

// Simple GET request
let response = client.get("https://api.example.com/users")!
println("Status: " + response.status().to_string())
println("Body: " + response.body())

// POST with JSON body
let request = Request.new(Method.POST, "https://api.example.com/users")
    .header("Content-Type", "application/json")
    .body("{\"name\": \"Alice\", \"email\": \"alice@example.com\"}")

let response = client.send(request)!

when response.status() {
    200 => println("User created"),
    409 => println("User already exists"),
    _ => println("Error: " + response.status().to_string()),
}
```

### Custom Headers

```tml
let request = Request.new(Method.GET, "https://api.example.com/data")
    .header("Authorization", "Bearer " + token)
    .header("Accept", "application/json")
    .header("User-Agent", "MyApp/1.0")

let response = client.send(request)!
```

## HTTP Server

### Basic Server

```tml
use std::http::{HttpServer, Request, Response, Status}

let server = HttpServer.bind("0.0.0.0:3000")!

server.on_request(do(req: Request) -> Response {
    when req.path() {
        "/" => Response.new(Status.OK).body("Welcome!"),
        "/health" => Response.new(Status.OK).body("ok"),
        _ => Response.new(Status.NOT_FOUND).body("Not found"),
    }
})

println("Server running on http://localhost:3000")
server.listen()!
```

### Router

The Router provides path matching with parameters and wildcards:

```tml
use std::http::router::{Router, RouteMatch}

var router = Router.new()

router.get("/users", do(req) -> Response {
    Response.new(Status.OK).body(list_users())
})

router.get("/users/:id", do(req) -> Response {
    let id = req.param("id")
    let user = find_user(id)! else {
        return Response.new(Status.NOT_FOUND).body("User not found")
    }
    Response.new(Status.OK).body(user.to_json())
})

router.post("/users", do(req) -> Response {
    let body = req.json()!
    let user = create_user(body)!
    Response.new(Status.CREATED).body(user.to_json())
})

router.delete("/users/:id", do(req) -> Response {
    let id = req.param("id")
    delete_user(id)!
    Response.new(Status.NO_CONTENT)
})
```

### Middleware

TML's HTTP framework includes built-in middleware:

```tml
use std::http::middleware::{CORS, Compression, RateLimit, Security}

// CORS headers
let cors = CORS.new()
    .allow_origin("https://example.com")
    .allow_methods(["GET", "POST", "PUT", "DELETE"])
    .allow_headers(["Content-Type", "Authorization"])

// Response compression
let compress = Compression.new()  // gzip, brotli auto-negotiation

// Rate limiting
let limiter = RateLimit.new()
    .max_requests(100)
    .window_seconds(60)

// Security headers (HSTS, CSP, etc.)
let security = Security.new()
    .hsts(31536000)
    .content_security_policy("default-src 'self'")
```

## Practical Example: REST API Client

```tml
use std::http::{HttpClient, Request, Method}
use std::json::Json

type ApiClient {
    base_url: Str,
    client: HttpClient,
    token: Str,
}

extend ApiClient {
    func new(base_url: Str, token: Str) -> ApiClient {
        ApiClient {
            base_url: base_url,
            client: HttpClient.new(),
            token: token,
        }
    }

    func get(this, path: Str) -> Outcome[Json, HttpError] {
        let url = this.base_url + path
        let request = Request.new(Method.GET, url)
            .header("Authorization", "Bearer " + this.token)
            .header("Accept", "application/json")

        let response = this.client.send(request)!
        if response.status() != 200 {
            return Err(HttpError.Status(response.status()))
        }

        let json = Json.parse(response.body())!
        return Ok(json)
    }

    func post(this, path: Str, body: Json) -> Outcome[Json, HttpError] {
        let url = this.base_url + path
        let request = Request.new(Method.POST, url)
            .header("Authorization", "Bearer " + this.token)
            .header("Content-Type", "application/json")
            .body(body.to_string())

        let response = this.client.send(request)!
        let json = Json.parse(response.body())!
        return Ok(json)
    }
}

// Usage
func main() {
    let api = ApiClient.new("https://api.example.com", "my-token")

    let users = api.get("/users")!
    println("Users: " + users.to_string())

    let new_user = Json.object()
        .ks("name", "Alice")
        .ks("email", "alice@example.com")
        .end_obj()

    let created = api.post("/users", new_user)!
    println("Created: " + created.to_string())
}
```

## Error Handling in Network Code

Network operations are inherently fallible. Use the `!` operator with `else` recovery for robust networking:

```tml
// Retry pattern
func fetch_with_retry(url: Str, max_retries: I32) -> Outcome[Str, HttpError] {
    var attempts = 0
    loop while attempts < max_retries {
        let result = client.get(url)
        when result {
            Ok(response) => return Ok(response.body()),
            Err(e) => {
                attempts = attempts + 1
                if attempts >= max_retries {
                    return Err(e)
                }
                sleep(Duration.from_secs(attempts as I64))
            }
        }
    }
    return Err(HttpError.MaxRetries)
}
```
