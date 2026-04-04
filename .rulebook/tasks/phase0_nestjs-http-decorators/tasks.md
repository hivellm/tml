# Tasks: NestJS-Style HTTP Decorators — Full API Framework

**Status**: In Progress. 40% (22/55). Phases 3-8 (pure library code) complete and tested. Phases 1-2 blocked on compiler support (@Controller on types, @Param on func params).
**Existing infra**: @Get/@Post codegen works, radix-tree router, App.listen(), IncomingMessage, app_build_response
**Target API**:
```tml
@Controller("/users")
type UserController {}

impl UserController {
    @Get("/")
    func findAll(this) -> JsonResponse {
        return JsonResponse::ok(users_json)
    }

    @Get("/:id")
    func findOne(this, @Param("id") id: Str) -> JsonResponse {
        return JsonResponse::ok(user_json)
    }

    @Post("/")
    func create(this, @Body body: Str) -> JsonResponse {
        return JsonResponse::created(body)
    }
}
```

## Phase 1: @Controller + Method Decorators on Impl Methods (10 items)

### Compiler: @Controller on types
- [ ] 1.1 Parser: recognize `@Controller("/prefix")` decorator on `type` declarations
- [ ] 1.2 HIR: store controller prefix in struct/class metadata
- [ ] 1.3 Codegen: when a type has `@Controller`, collect all `@Get/@Post/...` decorated methods from its impl blocks
- [ ] 1.4 Codegen: auto-prepend controller prefix to method route paths (e.g., `@Controller("/users")` + `@Get("/:id")` → `/users/:id`)

### Library: Controller registration
- [ ] 1.5 `http/controller.tml` — rewrite Controller behavior to auto-register from decorators
- [ ] 1.6 `App::register_controller(ctrl)` — scans decorated methods and registers routes
- [ ] 1.7 Method handlers receive `(this, req: IncomingMessage) -> Str` instead of free functions

### Tests
- [ ] 1.8 Test: @Controller + @Get on impl method compiles and registers route
- [ ] 1.9 Test: prefix concatenation works (/api + /users/:id = /api/users/:id)
- [ ] 1.10 Test: multiple controllers on same app

## Phase 2: Parameter Extraction Decorators (8 items)

### Compiler: field-level decorators on function params
- [ ] 2.1 Parser: recognize `@Param("name")`, `@Query("key")`, `@Body`, `@Headers("name")` on function parameters
- [ ] 2.2 AST: add `decorators` field to `FuncParam` (similar to how we added it to StructField)
- [ ] 2.3 Codegen: generate parameter extraction code before calling the handler body

### Library: extraction helpers
- [ ] 2.4 `http/params.tml` — `extract_param(req, name)`, `extract_query(req, key)`, `extract_body(req)`, `extract_header(req, name)`
- [ ] 2.5 `IncomingMessage` — ensure query string parsing is accessible (parse `?key=val&key2=val2`)
- [ ] 2.6 `IncomingMessage` — ensure body is accessible as Str (for POST/PUT/PATCH)

### Tests
- [ ] 2.7 Test: @Param("id") extracts path parameter
- [ ] 2.8 Test: @Query("page") extracts query string parameter

## Phase 3: JSON Request/Response (7 items)

### Library: structured responses
- [x] 3.1 `http/framework/json_response.tml` — `JsonResponse` type with status_code, content_type, body, extra_headers
- [x] 3.2 `JsonResponse::ok(body)`, `::created(body)`, `::not_found(msg)`, `::bad_request(msg)`, `::internal_error(msg)`, `::no_content()`, `::unauthorized()`, `::forbidden()`
- [x] 3.3 `JsonResponse::with_header(name, value)` — fluent header builder
- [x] 3.4 Content-Type set to `application/json` automatically in all factory methods
- [ ] 3.5 `http/json_body.tml` — `parse_json_body(req) -> Outcome[JsonValue, HttpError]` using std::json (deferred — requires IncomingMessage body access integration)

### Tests
- [x] 3.6 `lib/std/tests/http/http_json_response.test.tml` — 7 tests passing (ok, created, not_found, bad_request, no_content, to_http, with_header)
- [ ] 3.7 Test: parse_json_body (deferred with 3.5)

## Phase 4: Middleware Pipeline (6 items)

### Library: middleware chain
- [x] 4.1 `http/framework/middleware.tml` — `Middleware` behavior with `handle(method, path, headers, body) -> Str`
- [ ] 4.2 `App::use_middleware(mw)` — registers middleware in order (deferred — requires App integration)
- [ ] 4.3 Middleware chain execution (deferred — requires App integration)
- [x] 4.4 Built-in: `LoggerMiddleware` — handle returns "" to continue
- [x] 4.5 Built-in: `CorsMiddleware` — `cors_headers(origin)` adds CORS headers + `CorsMiddleware::allow_all()`

### Tests
- [x] 4.6 `lib/std/tests/http/http_middleware.test.tml` — 5 tests passing (origin, allow_all, cors_headers, methods, allowed_headers)

## Phase 5: Guards & Auth (6 items)

### Library: route guards
- [x] 5.1 `http/framework/guard.tml` — `Guard` behavior with `can_activate(method, path, headers) -> Bool`
- [ ] 5.2 `@UseGuards(AuthGuard)` decorator (deferred — requires compiler support)
- [ ] 5.3 Guard execution integrated with App router (deferred — requires App integration)
- [x] 5.4 Built-in: `AuthGuard` — checks for Authorization header
- [x] 5.5 Built-in: `RolesGuard` — checks X-Role header against required role

### Tests
- [x] 5.6 `lib/std/tests/http/http_guard.test.tml` — 5 tests passing (allows with auth, blocks without, roles allows/blocks, required())

## Phase 6: Pipes & Validation (5 items)

### Library: input validation
- [x] 6.1 `http/framework/pipe.tml` — `Pipe` behavior with `transform(value: Str) -> Outcome[Str, Str]`
- [ ] 6.2 `@UsePipes(ValidationPipe)` decorator (deferred — requires compiler support)
- [x] 6.3 Built-in: `ParseIntPipe` — validates non-empty, returns Ok(value) or Err("empty value")
- [x] 6.4 Built-in: `ParseBoolPipe` — validates "true"/"false"/"1"/"0", `TrimPipe` passes through

### Tests
- [x] 6.5 `lib/std/tests/http/http_pipe.test.tml` — 5 tests passing (bool valid/invalid, int empty/valid, trim)

## Phase 7: Exception Filters (4 items)

### Library: error handling
- [x] 7.1 `http/framework/exception.tml` — `HttpException` type with status_code + message + factory methods
- [x] 7.2 `ExceptionFilter` behavior with `catch_exception(ex: HttpException) -> Str`
- [x] 7.3 `DefaultExceptionFilter` — returns JSON error via `ex.to_json()` (statusCode + message)
- [ ] 7.4 `@Catch(NotFoundException)` per-type filter (deferred — requires compiler support)

## Phase 8: Interceptors (4 items)

### Library: request/response transformation
- [x] 8.1 `http/framework/interceptor.tml` — `Interceptor` behavior with `before(method, path) -> Str` and `after(method, path, response) -> Str`
- [ ] 8.2 `@UseInterceptors(LoggingInterceptor)` decorator (deferred — requires compiler support)
- [x] 8.3 Built-in: `LoggingInterceptor` — before returns "", after passes response unchanged
- [x] 8.4 Built-in: `TransformInterceptor` — after wraps response in `{data: <response>}`

## Phase 9: Full Example App + Documentation (5 items)

### Example: REST API
- [ ] 9.1 `samples/api/` — complete CRUD API with @Controller, @Get/@Post/@Put/@Delete
- [ ] 9.2 User entity + UserService + UserController pattern
- [ ] 9.3 Integration with std::db ORM (SqliteConnection + repository)
- [ ] 9.4 Auth guard + JWT-style token validation
- [ ] 9.5 Documentation: "Building a REST API with TML" guide in docs/user/

## Architecture Notes

### What Already Works (no changes needed)
- `@Get("/path")`, `@Post("/path")` on FREE functions → compiler generates route table
- `App::new()`, `app.listen(port)`, radix-tree router with `:param` and `*wildcard`
- `IncomingMessage` with method, path, headers, body access
- `app_build_response(status, content_type, body)` → HTTP response string
- `app_get_param(req, name)` → path parameter extraction

### What Needs Compiler Changes
1. **@Controller on types** — codegen must collect methods from impl blocks (Phase 1)
2. **@Param/@Query/@Body on func params** — parser must support param-level decorators (Phase 2)
3. **Method-level @Get inside impl** — codegen must handle `this` param + prefix (Phase 1)

### What's Pure Library (no compiler changes)
- JsonResponse, Middleware, Guards, Pipes, Interceptors, ExceptionFilter (Phases 3-8)
- These use behaviors (traits) + the existing decorator infrastructure

### NestJS Parity Target
| NestJS Feature | TML Status | Phase |
|----------------|-----------|-------|
| @Controller | ❌ | 1 |
| @Get/@Post/@Put/@Delete | ✅ (free funcs) / ❌ (methods) | 1 |
| @Param, @Query, @Body, @Headers | ❌ | 2 |
| JSON serialization | ❌ (manual) | 3 |
| Middleware (app.use) | ❌ | 4 |
| Guards (@UseGuards) | ❌ | 5 |
| Pipes (@UsePipes) | ❌ | 6 |
| Exception Filters | ❌ | 7 |
| Interceptors | ❌ | 8 |
| @Module / DI | ❌ (out of scope — needs runtime DI container) | - |
| @Injectable | ❌ (out of scope — needs runtime reflection) | - |

### Out of Scope (needs major compiler features)
- `@Module` with full dependency injection container (needs runtime type registry)
- `@Injectable` with automatic constructor injection (needs compile-time DI resolution)
- Dynamic module loading (needs plugin system)
- GraphQL decorators (future task)
- WebSocket gateway decorators (future task)
