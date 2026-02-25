# TML Standard Library: MIME

> `std::mime` — MIME type parsing, comparison, and extension lookup per RFC 2045/2046.

## Overview

The `std::mime` module provides MIME type handling for HTTP content negotiation, file type detection, and media type manipulation. Supports parsing from strings, comparing types, and looking up MIME types by file extension.

## Import

```tml
use std::mime
use std::mime::{Mime, from_extension, TEXT_PLAIN, APPLICATION_JSON}
```

---

## Types

### Mime

A parsed MIME type with type, subtype, optional suffix, and parameters.

```tml
pub type Mime {
    type_name: Str,
    subtype: Str,
    suffix: Str,
    param_str: Str,
}
```

#### Methods

```tml
extend Mime {
    /// Creates a MIME type from type and subtype (e.g., "text", "html").
    pub func new(type_name: Str, subtype: Str) -> Mime

    /// Creates a MIME type with parameters (e.g., "charset=utf-8").
    pub func with_params(type_name: Str, subtype: Str, params: Str) -> Mime

    /// Parses a MIME string (e.g., "text/html; charset=utf-8").
    pub func parse(s: Str) -> Outcome[Mime, Str]

    /// Returns the top-level type (e.g., "text").
    pub func get_type(this) -> Str

    /// Returns the subtype (e.g., "html").
    pub func get_subtype(this) -> Str

    /// Returns the suffix (e.g., "xml" from "application/atom+xml").
    pub func get_suffix(this) -> Str

    /// Returns the essence "type/subtype" without parameters.
    pub func essence(this) -> Str

    /// Returns the full parameter string.
    pub func get_params(this) -> Str

    /// Returns the value of a specific parameter, or Nothing.
    pub func get_param(this, key: Str) -> Maybe[Str]

    /// Type category checks.
    pub func is_text(this) -> Bool
    pub func is_image(this) -> Bool
    pub func is_audio(this) -> Bool
    pub func is_video(this) -> Bool
    pub func is_application(this) -> Bool

    /// Compares two MIME types for equality (type + subtype).
    pub func eq(this, other: Mime) -> Bool

    /// Returns the full MIME string with parameters.
    pub func to_string(this) -> Str
}
```

---

## Constants

Common MIME types are available as functions that return pre-built `Mime` values:

| Function | Value |
|----------|-------|
| `TEXT_PLAIN()` | `text/plain` |
| `TEXT_HTML()` | `text/html` |
| `TEXT_CSS()` | `text/css` |
| `TEXT_JAVASCRIPT()` | `text/javascript` |
| `TEXT_XML()` | `text/xml` |
| `TEXT_CSV()` | `text/csv` |
| `APPLICATION_JSON()` | `application/json` |
| `APPLICATION_XML()` | `application/xml` |
| `APPLICATION_OCTET_STREAM()` | `application/octet-stream` |
| `APPLICATION_PDF()` | `application/pdf` |
| `APPLICATION_FORM_URLENCODED()` | `application/x-www-form-urlencoded` |
| `MULTIPART_FORM_DATA()` | `multipart/form-data` |
| `IMAGE_PNG()` | `image/png` |
| `IMAGE_JPEG()` | `image/jpeg` |
| `IMAGE_GIF()` | `image/gif` |
| `IMAGE_SVG()` | `image/svg+xml` |
| `IMAGE_WEBP()` | `image/webp` |
| `AUDIO_MPEG()` | `audio/mpeg` |
| `VIDEO_MP4()` | `video/mp4` |
| `APPLICATION_WASM()` | `application/wasm` |

---

## Free Functions

### from_extension

Looks up a MIME type by file extension.

```tml
/// Returns the MIME type for a file extension, or Nothing if unknown.
/// The extension should be provided without the leading dot.
pub func from_extension(ext: Str) -> Maybe[Mime]
```

---

## Example

```tml
use std::mime::{Mime, from_extension, APPLICATION_JSON, TEXT_HTML}

func main() {
    // Parse a MIME string
    let mime = Mime::parse("text/html; charset=utf-8")!
    print("Type: {mime.get_type()}\n")       // "text"
    print("Subtype: {mime.get_subtype()}\n") // "html"
    print("Essence: {mime.essence()}\n")     // "text/html"
    assert(mime.is_text())

    let charset = mime.get_param("charset")
    // charset == Just("utf-8")

    // Use predefined constants
    let json = APPLICATION_JSON()
    assert(json.is_application())
    assert(json.essence() == "application/json")

    // Compare MIME types
    let html = TEXT_HTML()
    assert(html.eq(Mime::new("text", "html")))

    // Lookup by file extension
    when from_extension("png") {
        Just(m) -> print("PNG is: {m.to_string()}\n"),  // "image/png"
        Nothing -> print("Unknown extension\n"),
    }
}
```

---

## See Also

- [std::http](./07-HTTP.md) — HTTP client with content-type handling
- [std::url](./34-URL.md) — URL parsing and building
