/**
 * OOP Benchmarks (C++)
 *
 * Tests object-oriented programming overhead: class creation, virtual dispatch,
 * inheritance, method calls.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <memory>

volatile int64_t sink = 0;

// ============================================================================
// Simple class (no inheritance)
// ============================================================================

class Point {
public:
    double x, y;

    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}

    double distance_squared() const {
        return x * x + y * y;
    }

    Point add(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }
};

// ============================================================================
// Virtual dispatch hierarchy
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
    Circle(double r) : radius(r) {}
    double area() const override {
        return 3.14159265359 * radius * radius;
    }
    double perimeter() const override {
        return 2 * 3.14159265359 * radius;
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
        return 2 * (width + height);
    }
};

class Triangle : public Shape {
    double a, b, c;

public:
    Triangle(double a, double b, double c) : a(a), b(b), c(c) {}
    double area() const override {
        double s = (a + b + c) / 2;
        return std::sqrt(s * (s - a) * (s - b) * (s - c));
    }
    double perimeter() const override {
        return a + b + c;
    }
};

// ============================================================================
// Deep inheritance
// ============================================================================

class Base {
public:
    virtual ~Base() = default;
    virtual int64_t compute(int64_t x) const {
        return x;
    }
};

class Level1 : public Base {
public:
    int64_t compute(int64_t x) const override {
        return x + 1;
    }
};

class Level2 : public Level1 {
public:
    int64_t compute(int64_t x) const override {
        return Level1::compute(x) + 1;
    }
};

class Level3 : public Level2 {
public:
    int64_t compute(int64_t x) const override {
        return Level2::compute(x) + 1;
    }
};

class Level4 : public Level3 {
public:
    int64_t compute(int64_t x) const override {
        return Level3::compute(x) + 1;
    }
};

// ============================================================================
// Multiple inheritance (interface-like)
// ============================================================================

class Drawable {
public:
    virtual ~Drawable() = default;
    virtual void draw() const = 0;
};

class Movable {
public:
    virtual ~Movable() = default;
    virtual void move(double dx, double dy) = 0;
};

class Sprite : public Drawable, public Movable {
    double x, y;

public:
    Sprite() : x(0), y(0) {}
    void draw() const override { /* noop for benchmark */ }
    void move(double dx, double dy) override {
        x += dx;
        y += dy;
    }
    double get_x() const {
        return x;
    }
};

// ============================================================================
// Benchmarks
// ============================================================================

void bench_object_creation(int64_t iterations) {
    double sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        Point p(static_cast<double>(i), static_cast<double>(i + 1));
        sum += p.distance_squared();
    }
    sink = static_cast<int64_t>(sum);
}

void bench_method_call(int64_t iterations) {
    Point p(3.0, 4.0);
    double sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += p.distance_squared();
    }
    sink = static_cast<int64_t>(sum);
}

void bench_method_chaining(int64_t iterations) {
    double sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        Point p(1.0, 2.0);
        Point result = p.add(Point(2.0, 3.0)).add(Point(3.0, 4.0));
        sum += result.distance_squared();
    }
    sink = static_cast<int64_t>(sum);
}

void bench_virtual_dispatch(int64_t iterations) {
    Circle c(5.0);
    Rectangle r(3.0, 4.0);
    Triangle t(3.0, 4.0, 5.0);

    Shape* shapes[3] = {&c, &r, &t};
    double sum = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        Shape* s = shapes[i % 3];
        sum += s->area() + s->perimeter();
    }
    sink = static_cast<int64_t>(sum);
}

void bench_virtual_single_type(int64_t iterations) {
    Circle c(5.0);
    Shape* s = &c;
    double sum = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        sum += s->area();
    }
    sink = static_cast<int64_t>(sum);
}

void bench_deep_inheritance(int64_t iterations) {
    Level4 obj;
    Base* b = &obj;
    int64_t sum = 0;

    for (int64_t i = 0; i < iterations; ++i) {
        sum += b->compute(i % 100);
    }
    sink = sum;
}

void bench_multiple_inheritance(int64_t iterations) {
    Sprite sprite;
    Drawable* d = &sprite;
    Movable* m = &sprite;

    for (int64_t i = 0; i < iterations; ++i) {
        d->draw();
        m->move(1.0, 1.0);
    }
    sink = static_cast<int64_t>(sprite.get_x());
}

void bench_stack_allocation(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        Point p(static_cast<double>(i), static_cast<double>(i));
        sum += static_cast<int64_t>(p.x + p.y);
    }
    sink = sum;
}

void bench_heap_allocation(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto p = std::make_unique<Point>(static_cast<double>(i), static_cast<double>(i));
        sum += static_cast<int64_t>(p->x + p->y);
    }
    sink = sum;
}

void bench_shared_ptr(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto p = std::make_shared<Point>(static_cast<double>(i), static_cast<double>(i));
        sum += static_cast<int64_t>(p->x + p->y);
    }
    sink = sum;
}

int main() {
    bench::Benchmark b("OOP");

    const int64_t ITERATIONS = 10000000; // 10M

    b.run_with_iter("Object Creation (stack)", ITERATIONS, bench_object_creation, 10);
    b.run_with_iter("Method Call (non-virtual)", ITERATIONS, bench_method_call, 10);
    b.run_with_iter("Method Chaining", ITERATIONS, bench_method_chaining, 10);
    b.run_with_iter("Virtual Dispatch (3 types)", ITERATIONS, bench_virtual_dispatch, 10);
    b.run_with_iter("Virtual Dispatch (single type)", ITERATIONS, bench_virtual_single_type, 10);
    b.run_with_iter("Deep Inheritance (4 levels)", ITERATIONS, bench_deep_inheritance, 10);
    b.run_with_iter("Multiple Inheritance", ITERATIONS, bench_multiple_inheritance, 10);
    b.run_with_iter("Stack Allocation", ITERATIONS, bench_stack_allocation, 10);
    b.run_with_iter("Heap Allocation (unique_ptr)", ITERATIONS, bench_heap_allocation, 10);
    b.run_with_iter("Shared Pointer (shared_ptr)", ITERATIONS, bench_shared_ptr, 10);

    b.print_results();
    b.save_json("../results/oop_cpp.json");

    return 0;
}
