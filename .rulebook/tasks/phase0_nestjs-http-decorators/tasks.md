# Tasks: NestJS-Style HTTP Decorators — Full API Framework

**Status**: Planning. 0% (0/55).
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
- [ ] 3.1 `http/json_response.tml` — `JsonResponse` type with status, headers, body
- [ ] 3.2 `JsonResponse::ok(body)`, `::created(body)`, `::not_found(msg)`, `::bad_request(msg)`, `::internal_error(msg)`
- [ ] 3.3 `JsonResponse::with_header(name, value)` — fluent header builder
- [ ] 3.4 Auto-set `Content-Type: application/json` when returning JsonResponse
- [ ] 3.5 `http/json_body.tml` — `parse_json_body(req) -> Outcome[JsonValue, HttpError]` using std::json

### Tests
- [ ] 3.6 Test: JsonResponse::ok generates correct HTTP response string
- [ ] 3.7 Test: parse_json_body extracts and parses JSON from POST body

## Phase 4: Middleware Pipeline (6 items)

### Library: middleware chain
- [ ] 4.1 `http/middleware.tml` — `Middleware` behavior with `handle(req, next) -> Str`
- [ ] 4.2 `App::use_middleware(mw)` — registers middleware in order
- [ ] 4.3 Middleware chain execution: req → mw1 → mw2 → ... → handler → response
- [ ] 4.4 Built-in: `LoggerMiddleware` — logs method, path, status, duration
- [ ] 4.5 Built-in: `CorsMiddleware` — adds CORS headers (configurable origins)

### Tests
- [ ] 4.6 Test: middleware chain executes in order, can modify request/response

## Phase 5: Guards & Auth (6 items)

### Library: route guards
- [ ] 5.1 `http/guard.tml` — `Guard` behavior with `can_activate(req) -> Bool`
- [ ] 5.2 `@UseGuards(AuthGuard)` decorator on controller or method level
- [ ] 5.3 Guard execution: runs before handler, returns 403 if false
- [ ] 5.4 Built-in: `AuthGuard` — checks for Authorization header (Bearer token)
- [ ] 5.5 Built-in: `RolesGuard` — checks user role from request context

### Tests
- [ ] 5.6 Test: guard blocks unauthorized request, allows authorized

## Phase 6: Pipes & Validation (5 items)

### Library: input validation
- [ ] 6.1 `http/pipe.tml` — `Pipe` behavior with `transform(value: Str) -> Outcome[Str, HttpError]`
- [ ] 6.2 `@UsePipes(ValidationPipe)` decorator
- [ ] 6.3 Built-in: `ParseIntPipe` — converts Str param to I64, returns 400 on failure
- [ ] 6.4 Built-in: `ParseBoolPipe` — converts "true"/"false" to Bool

### Tests
- [ ] 6.5 Test: ParseIntPipe converts valid int, rejects non-numeric

## Phase 7: Exception Filters (4 items)

### Library: error handling
- [ ] 7.1 `http/exception.tml` — `HttpException` type with status + message
- [ ] 7.2 `http/filter.tml` — `ExceptionFilter` behavior with `catch(err) -> Str`
- [ ] 7.3 Default exception filter: catches unhandled errors, returns 500 JSON
- [ ] 7.4 `@Catch(NotFoundException)` — custom filter per exception type

## Phase 8: Interceptors (4 items)

### Library: request/response transformation
- [ ] 8.1 `http/interceptor.tml` — `Interceptor` behavior with `intercept(req, handler) -> Str`
- [ ] 8.2 `@UseInterceptors(LoggingInterceptor)` decorator
- [ ] 8.3 Built-in: `LoggingInterceptor` — logs request duration
- [ ] 8.4 Built-in: `TransformInterceptor` — wraps response in `{data: ..., timestamp: ...}`

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
