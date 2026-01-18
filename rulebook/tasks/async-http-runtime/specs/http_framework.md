# HTTP Framework Specification (NestJS-style)

## Overview

The HTTP framework provides a declarative, decorator-based API for building web applications. Inspired by NestJS, it emphasizes:
- **Decorators** for routing and middleware
- **Dependency Injection** for service management
- **Modular architecture** for organization
- **Type safety** throughout

## Decorators

### Controller Decorators

```tml
/// Marks a class as an HTTP controller
///
/// @param path - Base path for all routes in this controller
@Controller(path: Str)

/// Example:
@Controller("/api/users")
pub class UserController {
    // Routes defined here will be prefixed with /api/users
}
```

### HTTP Method Decorators

```tml
/// HTTP GET request
@Get(path: Str = "/")

/// HTTP POST request
@Post(path: Str = "/")

/// HTTP PUT request
@Put(path: Str = "/")

/// HTTP DELETE request
@Delete(path: Str = "/")

/// HTTP PATCH request
@Patch(path: Str = "/")

/// HTTP HEAD request
@Head(path: Str = "/")

/// HTTP OPTIONS request
@Options(path: Str = "/")

/// Match all HTTP methods
@All(path: Str = "/")

/// Example:
@Controller("/users")
pub class UserController {
    @Get("/")
    pub async func list_all(this) -> Response { ... }

    @Get("/:id")
    pub async func get_one(this, @Param("id") id: U64) -> Response { ... }

    @Post("/")
    pub async func create(this, @Body dto: CreateUserDto) -> Response { ... }

    @Put("/:id")
    pub async func update(this, @Param("id") id: U64, @Body dto: UpdateUserDto) -> Response { ... }

    @Delete("/:id")
    pub async func delete(this, @Param("id") id: U64) -> Response { ... }
}
```

### Parameter Decorators

```tml
/// Extract URL path parameter
///
/// @param name - Parameter name from path pattern
@Param(name: Str)

/// Extract query string parameter
///
/// @param name - Query parameter name
/// @param default - Optional default value
@Query(name: Str, default: Maybe[Str] = Nothing)

/// Extract request body (JSON deserialized)
@Body

/// Extract specific header
///
/// @param name - Header name
@Header(name: Str)

/// Get raw request object
@Req

/// Get response builder
@Res

/// Extract cookie value
///
/// @param name - Cookie name
@Cookie(name: Str)

/// Extract session data
@Session

/// Extract IP address
@Ip

/// Example:
@Get("/search")
pub async func search(
    this,
    @Query("q") query: Str,
    @Query("page", default: "1") page: Str,
    @Query("limit", default: "10") limit: Str,
    @Header("Authorization") auth: Maybe[Str],
) -> Response {
    // query, page, limit extracted from ?q=...&page=...&limit=...
    // auth extracted from Authorization header
}
```

### Middleware Decorators

```tml
/// Apply middleware to controller or route
///
/// @param middleware - Middleware class or function
@UseMiddleware(middleware: Type | Func)

/// Apply multiple middlewares in order
@UseMiddlewares(middlewares: Vec[Type | Func])

/// Example:
@UseMiddleware(LoggingMiddleware)
@UseMiddleware(AuthMiddleware)
@Controller("/api/admin")
pub class AdminController {
    @UseMiddleware(RateLimitMiddleware)
    @Get("/stats")
    pub async func get_stats(this) -> Response { ... }
}
```

### Guard Decorators

```tml
/// Apply authorization guard
///
/// @param guard - Guard class
@UseGuard(guard: Type)

/// Apply multiple guards (all must pass)
@UseGuards(guards: Vec[Type])

/// Example:
@UseGuard(JwtAuthGuard)
@Controller("/api/protected")
pub class ProtectedController {
    @UseGuard(AdminRoleGuard)
    @Delete("/:id")
    pub async func admin_delete(this, @Param("id") id: U64) -> Response { ... }
}
```

### Pipe Decorators

```tml
/// Apply validation/transformation pipe
///
/// @param pipe - Pipe class
@UsePipe(pipe: Type)

/// Example:
@Post("/")
pub async func create(
    this,
    @Body @UsePipe(ValidationPipe) dto: CreateUserDto
) -> Response { ... }
```

### Response Decorators

```tml
/// Set response status code
///
/// @param code - HTTP status code
@HttpCode(code: U16)

/// Add response header
///
/// @param name - Header name
/// @param value - Header value
@Header(name: Str, value: Str)

/// Set redirect
///
/// @param url - Redirect URL
/// @param code - Status code (default 302)
@Redirect(url: Str, code: U16 = 302)

/// Render view template
///
/// @param template - Template name
@Render(template: Str)

/// Example:
@Post("/")
@HttpCode(201)
@Header("X-Custom-Header", "value")
pub async func create(this, @Body dto: CreateUserDto) -> Response { ... }
```

### Module Decorators

```tml
/// Define a module
///
/// @param controllers - Controllers in this module
/// @param providers - Services provided by this module
/// @param imports - Other modules to import
/// @param exports - Services to export to importing modules
@Module(
    controllers: Vec[Type] = [],
    providers: Vec[Type] = [],
    imports: Vec[Type] = [],
    exports: Vec[Type] = [],
)

/// Example:
@Module(
    controllers: [UserController, ProfileController],
    providers: [UserService, UserRepository],
    imports: [DatabaseModule, CacheModule],
    exports: [UserService],
)
pub class UserModule;
```

### Injectable Decorator

```tml
/// Mark a class as injectable (for dependency injection)
@Injectable

/// Example:
@Injectable
pub class UserService {
    repository: UserRepository,  // Injected
    cache: CacheService,         // Injected

    pub async func find_all(this) -> Vec[User] {
        this.repository.find_all().await
    }
}
```

## Core Interfaces

### Middleware

```tml
/// Middleware interface
pub behavior Middleware {
    /// Process request and optionally pass to next handler
    async func handle(
        this,
        request: Request,
        next: Next
    ) -> Response
}

/// Next handler in middleware chain
pub type Next {
    handler: Heap[dyn FnOnce(Request) -> Pin[Heap[dyn Future[Output = Response]]]>,
}

impl Next {
    pub async func call(this, request: Request) -> Response {
        (this.handler)(request).await
    }
}

/// Example middleware:
@Injectable
pub class LoggingMiddleware;

impl Middleware for LoggingMiddleware {
    async func handle(this, request: Request, next: Next) -> Response {
        let start = Instant::now()
        let method = request.method()
        let path = request.uri().path()

        let response = next.call(request).await

        let duration = start.elapsed()
        log::info("{method} {path} {} {:?}", response.status().code, duration)

        response
    }
}
```

### Guard

```tml
/// Authorization guard interface
pub behavior Guard {
    /// Returns true if request should proceed, false to reject
    async func can_activate(this, context: ExecutionContext) -> Bool
}

/// Execution context for guards
pub type ExecutionContext {
    request: ref Request,
    handler: HandlerInfo,
    class_name: Str,
    method_name: Str,
}

/// Example guard:
@Injectable
pub class JwtAuthGuard {
    jwt_service: JwtService,
}

impl Guard for JwtAuthGuard {
    async func can_activate(this, ctx: ExecutionContext) -> Bool {
        let auth = ctx.request.headers().get("Authorization")

        when auth {
            Just(value) => {
                let token = value.to_str().strip_prefix("Bearer ")
                when token {
                    Just(t) => this.jwt_service.verify(t).is_ok()
                    Nothing => false
                }
            }
            Nothing => false
        }
    }
}
```

### Pipe

```tml
/// Validation/transformation pipe interface
pub behavior Pipe[T] {
    /// Transform input value to output type
    func transform(this, value: Str, metadata: ArgumentMetadata) -> Outcome[T, PipeError]
}

/// Metadata about the argument being transformed
pub type ArgumentMetadata {
    arg_type: ArgType,
    data: Maybe[Str],  // e.g., parameter name
    metatype: Maybe[Type],
}

pub type ArgType {
    Body,
    Query,
    Param,
    Custom,
}

/// Example pipes:
pub class ParseIntPipe;

impl Pipe[I64] for ParseIntPipe {
    func transform(this, value: Str, metadata: ArgumentMetadata) -> Outcome[I64, PipeError] {
        value.parse[I64]().map_err(do(_) {
            PipeError::validation("Expected integer value")
        })
    }
}

pub class ValidationPipe[T: Validate + Deserialize];

impl[T: Validate + Deserialize] Pipe[T] for ValidationPipe[T] {
    func transform(this, value: Str, metadata: ArgumentMetadata) -> Outcome[T, PipeError] {
        let obj: T = json::from_str(value)?
        obj.validate()?
        Ok(obj)
    }
}
```

### Exception Filter

```tml
/// Exception handling filter
pub behavior ExceptionFilter {
    /// Handle an exception and return response
    func catch(this, exception: HttpException, context: ExecutionContext) -> Response
}

/// Built-in HTTP exception types
pub type HttpException {
    status: StatusCode,
    message: Str,
    cause: Maybe[Heap[dyn Error>>,
}

impl HttpException {
    pub func bad_request(msg: Str) -> HttpException {
        HttpException { status: StatusCode::BAD_REQUEST, message: msg, cause: Nothing }
    }

    pub func unauthorized(msg: Str) -> HttpException {
        HttpException { status: StatusCode::UNAUTHORIZED, message: msg, cause: Nothing }
    }

    pub func forbidden(msg: Str) -> HttpException {
        HttpException { status: StatusCode::FORBIDDEN, message: msg, cause: Nothing }
    }

    pub func not_found(msg: Str) -> HttpException {
        HttpException { status: StatusCode::NOT_FOUND, message: msg, cause: Nothing }
    }

    pub func internal_error(msg: Str) -> HttpException {
        HttpException { status: StatusCode::INTERNAL_SERVER_ERROR, message: msg, cause: Nothing }
    }
}

/// Example filter:
pub class GlobalExceptionFilter;

impl ExceptionFilter for GlobalExceptionFilter {
    func catch(this, exception: HttpException, context: ExecutionContext) -> Response {
        let body = json!({
            "statusCode": exception.status.code,
            "message": exception.message,
            "path": context.request.uri().path(),
            "timestamp": Instant::now().to_iso8601(),
        })

        Response::builder()
            .status(exception.status)
            .header(HeaderName::CONTENT_TYPE, "application/json")
            .body(body.to_string())
    }
}
```

## Dependency Injection

### Container

```tml
/// DI container
pub type Container {
    providers: HashMap[TypeId, Provider],
    instances: HashMap[TypeId, Sync[dyn Any>>,
}

pub type Provider {
    Singleton(fn(ref Container) -> Sync[dyn Any]),
    Factory(fn(ref Container) -> Sync[dyn Any]),
    Value(Sync[dyn Any]),
}

impl Container {
    pub func new() -> Container {
        Container {
            providers: HashMap::new(),
            instances: HashMap::new(),
        }
    }

    /// Register a provider
    pub func register[T: 'static](mut this, provider: Provider) {
        this.providers.insert(TypeId::of[T](), provider)
    }

    /// Resolve a dependency
    pub func resolve[T: 'static](this) -> Sync[T] {
        let type_id = TypeId::of[T]()

        // Check for existing singleton instance
        if let Just(instance) = this.instances.get(type_id) {
            return instance.downcast[T]().unwrap()
        }

        // Create new instance
        let provider = this.providers.get(type_id)
            .expect("No provider registered for type")

        let instance = when provider {
            Provider::Singleton(factory) => {
                let inst = factory(this)
                this.instances.insert(type_id, inst.duplicate())
                inst
            }
            Provider::Factory(factory) => factory(this)
            Provider::Value(value) => value.duplicate()
        }

        instance.downcast[T]().unwrap()
    }
}
```

### Auto-injection

The framework automatically injects constructor parameters:

```tml
@Injectable
pub class UserService {
    // These are automatically resolved from container
    repository: UserRepository,
    cache: CacheService,
    logger: Logger,
}

// Equivalent to:
impl UserService {
    pub func new(container: ref Container) -> UserService {
        UserService {
            repository: container.resolve[UserRepository](),
            cache: container.resolve[CacheService](),
            logger: container.resolve[Logger](),
        }
    }
}
```

## Router

### Route Matching

```tml
pub type Router {
    routes: Vec[Route],
    middlewares: Vec[Heap[dyn Middleware>>,
}

pub type Route {
    method: Method,
    pattern: RoutePattern,
    handler: RouteHandler,
    guards: Vec[Heap[dyn Guard>>,
    pipes: Vec[Heap[dyn Pipe[()>>>,
    middlewares: Vec[Heap[dyn Middleware>>,
}

pub type RoutePattern {
    segments: Vec[Segment],
    regex: Regex,
}

pub type Segment {
    Static(Str),
    Param(Str),
    Wildcard,
}

impl Router {
    pub func add_route(mut this, route: Route) {
        this.routes.push(route)
    }

    pub func match_route(this, method: Method, path: Str) -> Maybe[(ref Route, PathParams)] {
        for route in this.routes.iter() {
            if route.method == method {
                if let Just(params) = route.pattern.match_path(path) {
                    return Just((route, params))
                }
            }
        }
        Nothing
    }
}

pub type PathParams {
    params: HashMap[Str, Str],
}
```

### Route Handler

```tml
pub type RouteHandler {
    /// Type-erased handler function
    handler: Heap[dyn Fn(Request, PathParams) -> Pin[Heap[dyn Future[Output = Response]]]>,
}
```

## Application Bootstrap

```tml
/// Application factory
pub type NestFactory;

impl NestFactory {
    /// Create application from root module
    pub async func create[M: Module](module: Type) -> Application {
        let container = Container::new()

        // Register module providers
        M::register_providers(mut container)

        // Import dependencies
        for import in M::imports() {
            import.register_providers(mut container)
        }

        // Create controllers
        var router = Router::new()
        for controller_type in M::controllers() {
            let controller = container.resolve(controller_type)
            register_controller_routes(mut router, controller)
        }

        Application {
            container,
            router,
            global_middlewares: Vec::new(),
            global_guards: Vec::new(),
            global_filters: Vec::new(),
        }
    }
}

pub type Application {
    container: Container,
    router: Router,
    global_middlewares: Vec[Heap[dyn Middleware>>,
    global_guards: Vec[Heap[dyn Guard>>,
    global_filters: Vec[Heap[dyn ExceptionFilter>>,
}

impl Application {
    pub func use_global_middleware[M: Middleware>(mut this, middleware: M) {
        this.global_middlewares.push(Heap::new(middleware))
    }

    pub func use_global_guard[G: Guard>(mut this, guard: G) {
        this.global_guards.push(Heap::new(guard))
    }

    pub func use_global_filter[F: ExceptionFilter>(mut this, filter: F) {
        this.global_filters.push(Heap::new(filter))
    }

    pub func enable_cors(mut this, options: CorsOptions) {
        this.use_global_middleware(CorsMiddleware::new(options))
    }

    pub async func listen(this, port: U16) -> Outcome[(), IoError] {
        let addr = SocketAddr::new(Ipv4Addr::UNSPECIFIED(), port)
        let listener = TcpListener::bind(addr).await?

        log::info("Server listening on port {port}")

        loop {
            let (stream, addr) = listener.accept().await?

            spawn(async move {
                this.handle_connection(stream, addr).await
            })
        }
    }

    async func handle_connection(this, stream: TcpStream, addr: SocketAddr) {
        // Parse HTTP request
        // Route to handler
        // Execute middleware chain
        // Return response
    }
}
```

## Usage Example

```tml
// main.tml

use http::{NestFactory, Module, Controller, Get, Post, Body, Param}
use http::{Injectable, UseGuard, UseMiddleware}

// DTOs
pub type CreateUserDto {
    name: Str,
    email: Str,
}

pub type User {
    id: U64,
    name: Str,
    email: Str,
}

// Services
@Injectable
pub class UserService {
    users: Vec[User],
    next_id: U64,

    pub func new() -> UserService {
        UserService { users: Vec::new(), next_id: 1 }
    }

    pub async func find_all(this) -> Vec[User] {
        this.users.duplicate()
    }

    pub async func find_one(this, id: U64) -> Maybe[User] {
        this.users.iter().find(do(u) u.id == id).map(do(u) u.duplicate())
    }

    pub async func create(mut this, dto: CreateUserDto) -> User {
        let user = User {
            id: this.next_id,
            name: dto.name,
            email: dto.email,
        }
        this.next_id = this.next_id + 1
        this.users.push(user.duplicate())
        user
    }
}

// Controllers
@Controller("/api/users")
pub class UserController {
    user_service: UserService,

    @Get("/")
    pub async func list_users(this) -> Response {
        let users = this.user_service.find_all().await
        Response::json(users)
    }

    @Get("/:id")
    pub async func get_user(this, @Param("id") id: U64) -> Response {
        when this.user_service.find_one(id).await {
            Just(user) => Response::json(user)
            Nothing => Response::not_found()
        }
    }

    @Post("/")
    @HttpCode(201)
    pub async func create_user(this, @Body dto: CreateUserDto) -> Response {
        let user = this.user_service.create(dto).await
        Response::json(user)
    }
}

// Module
@Module(
    controllers: [UserController],
    providers: [UserService],
)
pub class AppModule;

// Main
pub async func main() {
    let app = NestFactory::create(AppModule).await

    app.use_global_middleware(LoggingMiddleware)
    app.enable_cors(CorsOptions::default())

    app.listen(3000).await.expect("Failed to start server")
}
```
