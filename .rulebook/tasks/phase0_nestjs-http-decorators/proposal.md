# Proposal: NestJS-Style HTTP Decorators — Full API Framework

## Why
TML already has `@Get`/`@Post` route decorators and a working HTTP server with radix-tree router,
but the developer experience is far from NestJS. Writing an API requires manual IncomingMessage
parsing, string-based responses, no parameter extraction decorators, no middleware pipeline, no
validation, no guards, no interceptors, and no module system. This makes TML unusable for real
API development despite having the underlying infrastructure.

## What Changes
Complete NestJS-style decorator-driven HTTP framework:
- `@Controller("/prefix")` on types — groups routes under a prefix
- `@Get`/`@Post`/`@Put`/`@Delete`/`@Patch` on methods — route handlers
- `@Param("id")`, `@Query("page")`, `@Body`, `@Headers` — parameter extraction decorators
- `@UseGuards(AuthGuard)`, `@UseInterceptors(LogInterceptor)` — cross-cutting concerns
- `@UsePipes(ValidationPipe)` — input validation
- `@Module` — dependency injection and module organization
- `@Injectable` — service classes with DI
- JSON request/response auto-serialization
- Exception filters with `@Catch`
- Middleware pipeline (`app.use()`)

## Impact
- Affected code: lib/std/src/http/ (controller, middleware, guards, pipes, interceptors, module)
- Affected compiler: parser (already supports field decorators), codegen (route registration)
- Breaking change: NO (additive — existing @Get/@Post on free functions still works)
- User benefit: Write production APIs in TML with the same DX as NestJS/Express
