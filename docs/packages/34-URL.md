# TML Standard Library: URL

> `std::url` — URL parsing, building, and manipulation per RFC 3986.

## Overview

The `std::url` module provides a complete URL parser and builder conforming to RFC 3986. URLs can be parsed from strings, inspected by component, resolved against base URLs, and constructed incrementally with `UrlBuilder`.

## Import

```tml
use std::url
use std::url::{Url, UrlBuilder, QueryPair}
```

---

## Types

### Url

A parsed URL with individual components.

```tml
pub type Url {
    scheme: Str,
    userinfo: Str,
    host: Str,
    port: Str,
    path: Str,
    raw_query: Str,
    fragment: Str,
}
```

#### Methods

```tml
extend Url {
    /// Parses a URL string. Returns Err with a description on invalid input.
    pub func parse(input: Str) -> Outcome[Url, Str]

    /// Returns the scheme (e.g., "https").
    pub func get_scheme(this) -> Str

    /// Returns the host (e.g., "example.com").
    pub func get_host(this) -> Str

    /// Returns the port (e.g., "8080"), or empty string if absent.
    pub func get_port(this) -> Str

    /// Returns the path (e.g., "/api/v1/users").
    pub func get_path(this) -> Str

    /// Returns the raw query string (e.g., "key=value&foo=bar").
    pub func get_query(this) -> Str

    /// Returns the fragment (e.g., "section1").
    pub func get_fragment(this) -> Str

    /// Returns the authority component: [userinfo@]host[:port].
    pub func authority(this) -> Str

    /// Returns host:port (or just host if no port).
    pub func host_port(this) -> Str

    /// Reconstructs the full URL string.
    pub func to_string(this) -> Str

    /// Parses the query string into a list of key-value pairs.
    pub func query_pairs(this) -> List[QueryPair]

    /// Resolves a relative URL reference against this URL as the base.
    pub func join(this, relative: Str) -> Outcome[Url, Str]
}
```

### QueryPair

A single key-value pair from a URL query string.

```tml
pub type QueryPair {
    key: Str,
    value: Str,
}
```

### UrlBuilder

Incrementally constructs a URL from individual components.

```tml
extend UrlBuilder {
    /// Creates a new empty builder.
    pub func new() -> UrlBuilder

    /// Sets the scheme (e.g., "https").
    pub func set_scheme(mut this, scheme: Str) -> mut ref UrlBuilder

    /// Sets the host (e.g., "example.com").
    pub func set_host(mut this, host: Str) -> mut ref UrlBuilder

    /// Sets the port (e.g., "443").
    pub func set_port(mut this, port: Str) -> mut ref UrlBuilder

    /// Sets the path (e.g., "/api/v1").
    pub func set_path(mut this, path: Str) -> mut ref UrlBuilder

    /// Sets the raw query string.
    pub func set_query(mut this, query: Str) -> mut ref UrlBuilder

    /// Sets the fragment.
    pub func set_fragment(mut this, fragment: Str) -> mut ref UrlBuilder

    /// Adds a query parameter (key=value pair).
    pub func add_query(mut this, key: Str, value: Str) -> mut ref UrlBuilder

    /// Builds and validates the URL.
    pub func build(this) -> Outcome[Url, Str]
}
```

---

## Example

```tml
use std::url::{Url, UrlBuilder}

func main() {
    // Parse a URL
    let url = Url::parse("https://user:pass@example.com:8080/path?q=hello#top")!
    print("Scheme: {url.get_scheme()}\n")     // "https"
    print("Host: {url.get_host()}\n")         // "example.com"
    print("Port: {url.get_port()}\n")         // "8080"
    print("Path: {url.get_path()}\n")         // "/path"
    print("Authority: {url.authority()}\n")   // "user:pass@example.com:8080"

    // Query pairs
    let pairs = url.query_pairs()
    // pairs[0] = QueryPair { key: "q", value: "hello" }

    // Resolve relative URL
    let abs = url.join("/other/page")!
    print("Joined: {abs.to_string()}\n")  // "https://example.com:8080/other/page"

    // Build a URL from scratch
    var builder = UrlBuilder::new()
    builder.set_scheme("https")
    builder.set_host("api.example.com")
    builder.set_path("/v2/data")
    builder.add_query("format", "json")
    builder.add_query("limit", "10")
    let built = builder.build()!
    print("{built.to_string()}\n")  // "https://api.example.com/v2/data?format=json&limit=10"
}
```

---

## See Also

- [std::http](./07-HTTP.md) — HTTP client using URL types
- [core::str](./01-STR.md) — String operations
