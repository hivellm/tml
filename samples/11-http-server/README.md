# 11 — HTTP Server (Production Patterns)

Complete HTTP server demonstrating production-ready patterns in TML.

## Run

```bash
tml run samples/11-http-server/server.tml
```

## Test

```bash
# Root
curl http://localhost:3000/

# Health check (inspect headers)
curl -I http://localhost:3000/api/health

# CRUD
curl http://localhost:3000/api/users
curl http://localhost:3000/api/users/42
curl -X POST http://localhost:3000/api/users -d '{"name":"alice"}'
curl -X PUT http://localhost:3000/api/users/1 -d '{"name":"updated"}'
curl -X DELETE http://localhost:3000/api/users/1

# Static files
curl http://localhost:3000/static/index.html
curl http://localhost:3000/static/style.css

# CORS preflight
curl -X OPTIONS http://localhost:3000/api/users -H "Origin: https://example.com"

# Benchmark
npx autocannon -c 100 -d 10 http://localhost:3000/api/users
```

## Features Demonstrated

| Feature | Implementation |
|---------|---------------|
| **JSON API** | `/api/users` CRUD with proper `Content-Type: application/json` |
| **CORS** | `Access-Control-Allow-Origin: *` on all API responses + OPTIONS preflight |
| **ETag** | Response body length-based ETag for cache validation |
| **Cache-Control** | `no-cache` for API, `public, max-age=3600` for static, `immutable` for CSS |
| **Security Headers** | `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `X-XSS-Protection` |
| **Static Files** | HTML page + CSS with proper MIME types and long cache |
| **Path Parameters** | `:id` extraction via `app_get_param(req, "id")` |
| **Request Body** | POST body parsing via `req.body()` |
| **Error Handling** | JSON 400/404 responses with error messages |
| **Keep-Alive** | `Connection: keep-alive` on all responses |

## Response Headers Example

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 67
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
Cache-Control: no-cache, no-store, must-revalidate
Pragma: no-cache
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
X-XSS-Protection: 1; mode=block
ETag: "67-200"
Connection: keep-alive
```
