// OOP Benchmark - C++ version for comparison with TML
// Compile: clang++ -O3 -o oop_bench_cpp oop_bench.cpp
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>

// ============================================================================
// 1. Virtual Dispatch Benchmark - Shape hierarchy
// ============================================================================

class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
};

class Circle : public Shape {
    double radius;

public:
    explicit Circle(double r) : radius(r) {}
    double area() const override {
        return 3.14159265359 * radius * radius;
    }
    double perimeter() const override {
        return 2.0 * 3.14159265359 * radius;
    }
};

class Rectangle : public Shape {
    double width, height;

public:
    Rectangle(double w, double h) : width(w), height(h) {}
    double area() const override {
        return width * height;
    }
    double perimeter() const override {
        return 2.0 * (width + height);
    }
};

class Triangle : public Shape {
    double a, b, c, h;

public:
    Triangle(double a_, double b_, double c_, double h_) : a(a_), b(b_), c(c_), h(h_) {}
    double area() const override {
        return 0.5 * a * h;
    }
    double perimeter() const override {
        return a + b + c;
    }
};

double virtual_dispatch_bench(int iterations) {
    Circle circle(5.0);
    Rectangle rect(4.0, 6.0);
    Triangle tri(3.0, 4.0, 5.0, 5.0);

    double total = 0.0;
    for (int i = 0; i < iterations; ++i) {
        int mod = i % 3;
        if (mod == 0) {
            total += circle.area() + circle.perimeter();
        } else if (mod == 1) {
            total += rect.area() + rect.perimeter();
        } else {
            total += tri.area() + tri.perimeter();
        }
    }
    return total;
}

// ============================================================================
// 2. Object Creation Benchmark
// ============================================================================

class Point {
    double x, y;

public:
    Point(double x_, double y_) : x(x_), y(y_) {}
    double distance(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

double object_creation_bench(int iterations) {
    double total = 0.0;
    for (int i = 0; i < iterations; ++i) {
        double fi = static_cast<double>(i);
        Point p1(fi, fi * 2.0);
        Point p2(fi + 1.0, fi + 3.0);
        total += p1.distance(p2);
    }
    return total;
}

// ============================================================================
// 3. HTTP Handler Benchmark
// ============================================================================

class HttpHandler {
public:
    virtual ~HttpHandler() = default;
    virtual int handle(int method, int path_id) = 0;
};

class GetHandler : public HttpHandler {
    int resource_id;

public:
    explicit GetHandler(int id) : resource_id(id) {}
    int handle(int method, int path_id) override {
        return resource_id + method + path_id;
    }
};

class PostHandler : public HttpHandler {
    int resource_id;
    bool validate;

public:
    PostHandler(int id, bool v) : resource_id(id), validate(v) {}
    int handle(int method, int path_id) override {
        int result = resource_id + method + path_id;
        if (validate)
            result += 100;
        return result;
    }
};

class DeleteHandler : public HttpHandler {
    int resource_id;

public:
    explicit DeleteHandler(int id) : resource_id(id) {}
    int handle(int method, int path_id) override {
        return resource_id - method + path_id;
    }
};

int http_handler_bench(int iterations) {
    GetHandler get_h(1);
    PostHandler post_h(2, true);
    DeleteHandler del_h(3);

    int total = 0;
    for (int i = 0; i < iterations; ++i) {
        int mod = i % 3;
        if (mod == 0) {
            total += get_h.handle(1, 0);
        } else if (mod == 1) {
            total += post_h.handle(2, 1);
        } else {
            total += del_h.handle(3, 2);
        }
    }
    return total;
}

// ============================================================================
// 4. Game Loop Benchmark
// ============================================================================

class GameObject {
public:
    virtual ~GameObject() = default;
    virtual double update(double dt) = 0;
};

class Player : public GameObject {
    double x, y, speed;

public:
    Player(double x_, double y_, double s) : x(x_), y(y_), speed(s) {}
    double update(double dt) override {
        x += speed * dt;
        y += speed * dt * 0.5;
        return x + y;
    }
};

class Enemy : public GameObject {
    double x, y, speed, target_x, target_y;

public:
    Enemy(double x_, double y_, double s, double tx, double ty)
        : x(x_), y(y_), speed(s), target_x(tx), target_y(ty) {}
    double update(double dt) override {
        double dx = target_x - x;
        double dy = target_y - y;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.001) {
            x += (dx / dist) * speed * dt;
            y += (dy / dist) * speed * dt;
        }
        return x + y;
    }
};

class Projectile : public GameObject {
    double x, y, vx, vy;

public:
    Projectile(double x_, double y_, double vx_, double vy_) : x(x_), y(y_), vx(vx_), vy(vy_) {}
    double update(double dt) override {
        x += vx * dt;
        y += vy * dt;
        return x + y;
    }
};

double game_loop_bench(int iterations) {
    Player player(0.0, 0.0, 5.0);
    Enemy enemy(100.0, 100.0, 3.0, 0.0, 0.0);
    Projectile proj(0.0, 0.0, 10.0, 10.0);

    double total = 0.0;
    double dt = 0.016;
    for (int i = 0; i < iterations; ++i) {
        total += player.update(dt);
        total += enemy.update(dt);
        total += proj.update(dt);
    }
    return total;
}

// ============================================================================
// 5. Deep Inheritance Benchmark
// ============================================================================

class Base {
protected:
    int a;

public:
    explicit Base(int a_) : a(a_) {}
    virtual ~Base() = default;
    virtual int compute() {
        return a;
    }
};

class Derived1 : public Base {
protected:
    int b;

public:
    Derived1(int a_, int b_) : Base(a_), b(b_) {}
    int compute() override {
        return Base::compute() + b;
    }
};

class Derived2 : public Derived1 {
protected:
    int c;

public:
    Derived2(int a_, int b_, int c_) : Derived1(a_, b_), c(c_) {}
    int compute() override {
        return Derived1::compute() + c;
    }
};

class Derived3 : public Derived2 {
protected:
    int d;

public:
    Derived3(int a_, int b_, int c_, int d_) : Derived2(a_, b_, c_), d(d_) {}
    int compute() override {
        return Derived2::compute() + d;
    }
};

class Derived4 : public Derived3 {
protected:
    int e;

public:
    Derived4(int a_, int b_, int c_, int d_, int e_) : Derived3(a_, b_, c_, d_), e(e_) {}
    int compute() override {
        return Derived3::compute() + e;
    }
};

int deep_inheritance_bench(int iterations) {
    Base base(10);
    Derived1 d1(10, 5);
    Derived2 d2(10, 5, 3);
    Derived3 d3(10, 5, 3, 2);
    Derived4 d4(10, 5, 3, 2, 1);

    int total = 0;
    for (int i = 0; i < iterations; ++i) {
        int mod = i % 5;
        if (mod == 0)
            total += base.compute();
        else if (mod == 1)
            total += d1.compute();
        else if (mod == 2)
            total += d2.compute();
        else if (mod == 3)
            total += d3.compute();
        else
            total += d4.compute();
    }
    return total;
}

// ============================================================================
// 6. Method Chaining Benchmark
// ============================================================================

class Builder {
    int val;

public:
    Builder() : val(0) {}
    Builder with_add(int n) {
        Builder b;
        b.val = val + n;
        return b;
    }
    Builder with_multiply(int n) {
        Builder b;
        b.val = val * n;
        return b;
    }
    Builder with_subtract(int n) {
        Builder b;
        b.val = val - n;
        return b;
    }
    int build() const {
        return val;
    }
};

int method_chaining_bench(int iterations) {
    int total = 0;
    for (int i = 0; i < iterations; ++i) {
        int result = Builder().with_add(10).with_multiply(2).with_subtract(5).with_add(i).build();
        total += result;
    }
    return total;
}

// ============================================================================
// Main
// ============================================================================

int64_t time_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

// Prevent compiler from optimizing away results
volatile double sink_double = 0;
volatile int sink_int = 0;

template <typename F> int64_t run_benchmark_d(F func, int iterations) {
    int warmup = iterations / 10;
    sink_double = func(warmup);

    int64_t start = time_ns();
    sink_double = func(iterations);
    return time_ns() - start;
}

template <typename F> int64_t run_benchmark_i(F func, int iterations) {
    int warmup = iterations / 10;
    sink_int = func(warmup);

    int64_t start = time_ns();
    sink_int = func(iterations);
    return time_ns() - start;
}

int main() {
    std::cout << "============================================\n";
    std::cout << "C++ OOP Performance Benchmarks\n";
    std::cout << "============================================\n\n";

    int iterations = 100000;
    std::cout << "Running " << iterations << " iterations per benchmark...\n\n";

    int64_t t1 = run_benchmark_d(virtual_dispatch_bench, iterations);
    int64_t t2 = run_benchmark_d(object_creation_bench, iterations);
    int64_t t3 = run_benchmark_i(http_handler_bench, iterations);
    int64_t t4 = run_benchmark_d(game_loop_bench, iterations);
    int64_t t5 = run_benchmark_i(deep_inheritance_bench, iterations);
    int64_t t6 = run_benchmark_i(method_chaining_bench, iterations);

    std::cout << "Virtual Dispatch:     " << t1 / 1000 << " us\n";
    std::cout << "Object Creation:      " << t2 / 1000 << " us\n";
    std::cout << "HTTP Handler:         " << t3 / 1000 << " us\n";
    std::cout << "Game Loop:            " << t4 / 1000 << " us\n";
    std::cout << "Deep Inheritance:     " << t5 / 1000 << " us\n";
    std::cout << "Method Chaining:      " << t6 / 1000 << " us\n";

    std::cout << "\n============================================\n";
    std::cout << "Benchmarks complete!\n";
    std::cout << "============================================\n";

    return 0;
}
