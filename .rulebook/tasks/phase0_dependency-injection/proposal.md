# Proposal: Dependency Injection — NestJS-Complete IoC Container

## Why
NestJS's power comes from its DI system — `@Injectable`, `@Module`, providers, services,
constructor injection. Without DI, controllers can't use services, services can't use
repositories, and the entire layered architecture collapses. This is the missing piece
that makes the NestJS-style HTTP framework actually usable for real applications.

## How NestJS Does It
1. `@Injectable()` marks a class as a provider that can be injected
2. `@Module({ providers: [UserService], controllers: [UserController] })` defines a module
3. Constructor injection: `constructor(private userService: UserService)` auto-injects
4. The IoC container resolves the dependency graph at startup time
5. Scopes: Singleton (default), Request, Transient
6. Custom providers: useClass, useValue, useFactory

## TML Approach
TML doesn't have runtime reflection or constructor injection. Instead:
1. `@Injectable` on a type → marks it as a service (compile-time metadata)
2. `@Module` on a type → declares providers + controllers + imports
3. **Explicit injection via fields** — the DI container sets fields at startup:
   ```tml
   @Injectable
   type UserService {
       @Inject
       db: ref SqliteConnection
   }
   ```
4. **Container** — a runtime registry that creates instances and resolves dependencies
5. Dependency resolution at startup (topological sort of dependency graph)

## Impact
- Affected code: lib/std/src/di/ (new module), lib/std/src/http/ (integration)
- Breaking change: NO (additive)
- User benefit: Layered architecture with automatic dependency wiring, testable services
