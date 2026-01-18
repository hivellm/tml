// OOP Performance Benchmark for Rust
// Equivalent to oop_bench.cpp and oop_bench.tml
use std::time::Instant;

// =============================================================================
// Benchmark 1: Virtual Dispatch (trait objects)
// =============================================================================

trait Shape {
    fn area(&self) -> f64;
    fn perimeter(&self) -> f64;
}

struct Circle {
    radius: f64,
}

impl Shape for Circle {
    fn area(&self) -> f64 {
        std::f64::consts::PI * self.radius * self.radius
    }
    fn perimeter(&self) -> f64 {
        2.0 * std::f64::consts::PI * self.radius
    }
}

struct Rectangle {
    width: f64,
    height: f64,
}

impl Shape for Rectangle {
    fn area(&self) -> f64 {
        self.width * self.height
    }
    fn perimeter(&self) -> f64 {
        2.0 * (self.width + self.height)
    }
}

struct Triangle {
    base: f64,
    height: f64,
    side1: f64,
    side2: f64,
}

impl Shape for Triangle {
    fn area(&self) -> f64 {
        0.5 * self.base * self.height
    }
    fn perimeter(&self) -> f64 {
        self.base + self.side1 + self.side2
    }
}

fn virtual_dispatch_bench(iterations: i32) -> f64 {
    let circle = Circle { radius: 5.0 };
    let rectangle = Rectangle { width: 4.0, height: 6.0 };
    let triangle = Triangle { base: 3.0, height: 4.0, side1: 5.0, side2: 5.0 };

    let mut result = 0.0;
    for i in 0..iterations {
        let shape: &dyn Shape = match i % 3 {
            0 => &circle,
            1 => &rectangle,
            _ => &triangle,
        };
        result += shape.area();
        result += shape.perimeter();
    }
    result
}

// =============================================================================
// Benchmark 2: Object Creation
// =============================================================================

#[derive(Clone)]
struct Point {
    x: f64,
    y: f64,
}

impl Point {
    fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    fn distance(&self, other: &Point) -> f64 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        (dx * dx + dy * dy).sqrt()
    }
}

fn object_creation_bench(iterations: i32) -> f64 {
    let mut result = 0.0;
    for i in 0..iterations {
        let p1 = Point::new(i as f64, i as f64 * 2.0);
        let p2 = Point::new(i as f64 + 1.0, i as f64 + 3.0);
        result += p1.distance(&p2);
    }
    result
}

// =============================================================================
// Benchmark 3: HTTP Handler Pattern (trait objects)
// =============================================================================

trait HttpHandler {
    fn handle(&self, method: i32, path: i32) -> i32;
}

struct GetHandler {
    route_id: i32,
}

impl HttpHandler for GetHandler {
    fn handle(&self, _method: i32, path: i32) -> i32 {
        self.route_id * 100 + path
    }
}

struct PostHandler {
    route_id: i32,
    requires_auth: bool,
}

impl HttpHandler for PostHandler {
    fn handle(&self, method: i32, path: i32) -> i32 {
        let auth_bonus = if self.requires_auth { 1000 } else { 0 };
        self.route_id * 100 + path + method + auth_bonus
    }
}

struct DeleteHandler {
    route_id: i32,
}

impl HttpHandler for DeleteHandler {
    fn handle(&self, method: i32, path: i32) -> i32 {
        self.route_id * 100 + path - method
    }
}

fn http_handler_bench(iterations: i32) -> i32 {
    let get_handler = GetHandler { route_id: 1 };
    let post_handler = PostHandler { route_id: 2, requires_auth: true };
    let delete_handler = DeleteHandler { route_id: 3 };

    let mut result = 0;
    for i in 0..iterations {
        let handler: &dyn HttpHandler = match i % 3 {
            0 => &get_handler,
            1 => &post_handler,
            _ => &delete_handler,
        };
        result += handler.handle(i % 4, i % 100);
    }
    result
}

// =============================================================================
// Benchmark 4: Game Loop Pattern (trait objects with state)
// =============================================================================

trait Entity {
    fn update(&mut self, delta: f64);
    fn get_x(&self) -> f64;
    fn get_y(&self) -> f64;
}

struct Player {
    x: f64,
    y: f64,
    speed: f64,
    health: i32,
}

impl Entity for Player {
    fn update(&mut self, delta: f64) {
        self.x += self.speed * delta;
        self.y += self.speed * delta * 0.5;
    }
    fn get_x(&self) -> f64 { self.x }
    fn get_y(&self) -> f64 { self.y }
}

struct Enemy {
    x: f64,
    y: f64,
    speed: f64,
    damage: i32,
}

impl Entity for Enemy {
    fn update(&mut self, delta: f64) {
        self.x -= self.speed * delta;
        self.y += (self.x * 0.1).sin() * delta;
    }
    fn get_x(&self) -> f64 { self.x }
    fn get_y(&self) -> f64 { self.y }
}

struct Projectile {
    x: f64,
    y: f64,
    vx: f64,
    vy: f64,
}

impl Entity for Projectile {
    fn update(&mut self, delta: f64) {
        self.x += self.vx * delta;
        self.y += self.vy * delta;
    }
    fn get_x(&self) -> f64 { self.x }
    fn get_y(&self) -> f64 { self.y }
}

fn game_loop_bench(iterations: i32) -> f64 {
    let mut player = Player { x: 0.0, y: 0.0, speed: 10.0, health: 100 };
    let mut enemy = Enemy { x: 100.0, y: 50.0, speed: 5.0, damage: 10 };
    let mut projectile = Projectile { x: 0.0, y: 0.0, vx: 20.0, vy: 15.0 };

    let delta = 0.016; // 60 FPS
    let mut result = 0.0;

    for i in 0..iterations {
        let entity: &mut dyn Entity = match i % 3 {
            0 => &mut player,
            1 => &mut enemy,
            _ => &mut projectile,
        };
        entity.update(delta);
        result += entity.get_x() + entity.get_y();
    }
    result
}

// =============================================================================
// Benchmark 5: Deep Inheritance (simulated with composition)
// =============================================================================

struct Base {
    value: i32,
}

impl Base {
    fn compute(&self) -> i32 {
        self.value
    }
}

struct Derived1 {
    value: i32,
    bonus1: i32,
}

impl Derived1 {
    fn compute(&self) -> i32 {
        self.value + self.bonus1
    }
}

struct Derived2 {
    value: i32,
    bonus1: i32,
    bonus2: i32,
}

impl Derived2 {
    fn compute(&self) -> i32 {
        self.value + self.bonus1 + self.bonus2
    }
}

struct Derived3 {
    value: i32,
    bonus1: i32,
    bonus2: i32,
    bonus3: i32,
}

impl Derived3 {
    fn compute(&self) -> i32 {
        self.value + self.bonus1 + self.bonus2 + self.bonus3
    }
}

struct Derived4 {
    value: i32,
    bonus1: i32,
    bonus2: i32,
    bonus3: i32,
    bonus4: i32,
}

impl Derived4 {
    fn compute(&self) -> i32 {
        self.value + self.bonus1 + self.bonus2 + self.bonus3 + self.bonus4
    }
}

fn deep_inheritance_bench(iterations: i32) -> i32 {
    let base = Base { value: 10 };
    let d1 = Derived1 { value: 10, bonus1: 5 };
    let d2 = Derived2 { value: 10, bonus1: 5, bonus2: 3 };
    let d3 = Derived3 { value: 10, bonus1: 5, bonus2: 3, bonus3: 2 };
    let d4 = Derived4 { value: 10, bonus1: 5, bonus2: 3, bonus3: 2, bonus4: 1 };

    let mut result = 0;
    for i in 0..iterations {
        result += match i % 5 {
            0 => base.compute(),
            1 => d1.compute(),
            2 => d2.compute(),
            3 => d3.compute(),
            _ => d4.compute(),
        };
    }
    result
}

// =============================================================================
// Benchmark 6: Method Chaining / Builder Pattern
// =============================================================================

#[derive(Clone, Default)]
struct Builder {
    value: i32,
}

impl Builder {
    fn new() -> Self {
        Builder { value: 0 }
    }

    fn add(mut self, n: i32) -> Self {
        self.value += n;
        self
    }

    fn multiply(mut self, n: i32) -> Self {
        self.value *= n;
        self
    }

    fn subtract(mut self, n: i32) -> Self {
        self.value -= n;
        self
    }

    fn build(self) -> i32 {
        self.value
    }
}

fn method_chaining_bench(iterations: i32) -> i32 {
    let mut result = 0;
    for i in 0..iterations {
        let value = Builder::new()
            .add(i)
            .multiply(2)
            .subtract(1)
            .add(i % 10)
            .multiply(3)
            .build();
        result += value;
    }
    result
}

// =============================================================================
// Benchmark Runner
// =============================================================================

fn run_benchmark<F, R>(name: &str, iterations: i32, f: F) -> u64
where
    F: Fn(i32) -> R,
{
    // Warmup
    let warmup = iterations / 10;
    let _ = f(warmup);

    // Timed run
    let start = Instant::now();
    let _ = f(iterations);
    let elapsed = start.elapsed();

    elapsed.as_micros() as u64
}

fn main() {
    println!("============================================");
    println!("Rust OOP Performance Benchmarks");
    println!("============================================");
    println!();

    let iterations = 100000;
    println!("Running {} iterations per benchmark...", iterations);
    println!();

    let t1 = run_benchmark("Virtual Dispatch", iterations, virtual_dispatch_bench);
    let t2 = run_benchmark("Object Creation", iterations, object_creation_bench);
    let t3 = run_benchmark("HTTP Handler", iterations, http_handler_bench);
    let t4 = run_benchmark("Game Loop", iterations, game_loop_bench);
    let t5 = run_benchmark("Deep Inheritance", iterations, deep_inheritance_bench);
    let t6 = run_benchmark("Method Chaining", iterations, method_chaining_bench);

    println!("Virtual Dispatch:     {} us", t1);
    println!("Object Creation:      {} us", t2);
    println!("HTTP Handler:         {} us", t3);
    println!("Game Loop:            {} us", t4);
    println!("Deep Inheritance:     {} us", t5);
    println!("Method Chaining:      {} us", t6);

    println!();
    println!("============================================");
    println!("Benchmarks complete!");
    println!("============================================");
}
