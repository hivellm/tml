# std.encoding — Text and Binary Encodings

## 1. Overview

The `std.encoding` package provides text encoding/decoding (UTF-8, UTF-16, ASCII, Latin-1) and binary encoding (Base64, Hex).

```tml
import std.encoding
import std.encoding.{base64, hex, utf8, utf16}
```

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. Text Encodings

### 3.1 UTF-8

```tml
module utf8

/// Check if bytes are valid UTF-8
public func is_valid(bytes: &[U8]) -> Bool

/// Decode bytes to string
public func decode(bytes: &[U8]) -> Result[String, Utf8Error]

/// Decode bytes, replacing invalid sequences with replacement char
public func decode_lossy(bytes: &[U8]) -> String

/// Encode string to bytes (always succeeds - String is UTF-8)
public func encode(s: &str) -> &[U8] {
    return s.as_bytes()
}

/// Count characters (not bytes)
public func char_count(s: &str) -> U64

/// Get byte length of char at position
public func char_len(byte: U8) -> U64 {
    if (byte & 0x80) == 0 { return 1 }
    if (byte & 0xE0) == 0xC0 { return 2 }
    if (byte & 0xF0) == 0xE0 { return 3 }
    if (byte & 0xF8) == 0xF0 { return 4 }
    return 1  // Invalid, treat as single byte
}

public type Utf8Error {
    valid_up_to: U64,
    error_len: Option[U64],
}

extend Utf8Error {
    public func valid_up_to(this) -> U64 { this.valid_up_to }
    public func error_len(this) -> Option[U64] { this.error_len }
}
```

### 3.2 UTF-16

```tml
module utf16

/// Decode UTF-16 LE bytes to string
public func decode_le(bytes: &[U8]) -> Result[String, DecodeError]

/// Decode UTF-16 BE bytes to string
public func decode_be(bytes: &[U8]) -> Result[String, DecodeError]

/// Decode with BOM detection
public func decode(bytes: &[U8]) -> Result[String, DecodeError]

/// Encode string to UTF-16 LE
public func encode_le(s: &str) -> List[U8]

/// Encode string to UTF-16 BE
public func encode_be(s: &str) -> List[U8]

/// Encode with BOM
public func encode_with_bom(s: &str, endian: Endian) -> List[U8]

public type Endian = Little | Big

public type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

public type DecodeErrorKind =
    | InvalidLength      // Odd number of bytes
    | InvalidSurrogate   // Unpaired surrogate
    | InvalidCodePoint   // Code point out of range
```

### 3.3 ASCII

```tml
module ascii

/// Check if all bytes are ASCII (< 128)
public func is_ascii(bytes: &[U8]) -> Bool

/// Check if string is ASCII
public func is_ascii_str(s: &str) -> Bool

/// Decode ASCII bytes to string
public func decode(bytes: &[U8]) -> Result[String, AsciiError]

/// Decode, replacing non-ASCII with '?'
public func decode_lossy(bytes: &[U8]) -> String

/// Encode string to ASCII (fails if non-ASCII chars)
public func encode(s: &str) -> Result[List[U8], AsciiError]

/// Convert to uppercase ASCII
public func to_uppercase(bytes: &mut [U8])

/// Convert to lowercase ASCII
public func to_lowercase(bytes: &mut [U8])

public type AsciiError {
    position: U64,
    byte: U8,
}
```

### 3.4 Latin-1 (ISO-8859-1)

```tml
module latin1

/// Decode Latin-1 bytes to string
public func decode(bytes: &[U8]) -> String  // Always succeeds

/// Encode string to Latin-1
public func encode(s: &str) -> Result[List[U8], EncodeError]

/// Encode, replacing unencodable chars
public func encode_lossy(s: &str) -> List[U8]

public type EncodeError {
    position: U64,
    char: Char,
}
```

## 4. Binary Encodings

### 4.1 Base64

```tml
module base64

public type Config {
    alphabet: Alphabet,
    padding: Bool,
}

public type Alphabet = Standard | UrlSafe

public const STANDARD: Config = Config { alphabet: Standard, padding: true }
public const URL_SAFE: Config = Config { alphabet: UrlSafe, padding: false }
public const URL_SAFE_PADDED: Config = Config { alphabet: UrlSafe, padding: true }

/// Encode bytes to base64 string
public func encode(bytes: &[U8]) -> String {
    return encode_config(bytes, STANDARD)
}

/// Encode with custom config
public func encode_config(bytes: &[U8], config: Config) -> String

/// Decode base64 string to bytes
public func decode(s: &str) -> Result[List[U8], DecodeError]

/// Decode with custom config
public func decode_config(s: &str, config: Config) -> Result[List[U8], DecodeError]

/// Calculate encoded length
public func encoded_len(input_len: U64, padding: Bool) -> U64 {
    let base_len = (input_len + 2) / 3 * 4
    if padding {
        base_len
    } else {
        base_len - (3 - input_len % 3) % 3
    }
}

/// Calculate decoded length
public func decoded_len(input_len: U64) -> U64 {
    return input_len / 4 * 3
}

public type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

public type DecodeErrorKind =
    | InvalidChar(Char)
    | InvalidLength
    | InvalidPadding
```

### 4.2 Base32

```tml
module base32

public type Config {
    alphabet: Alphabet,
    padding: Bool,
}

public type Alphabet = Standard | Hex

public const STANDARD: Config = Config { alphabet: Standard, padding: true }
public const HEX: Config = Config { alphabet: Hex, padding: true }

public func encode(bytes: &[U8]) -> String
public func encode_config(bytes: &[U8], config: Config) -> String
public func decode(s: &str) -> Result[List[U8], DecodeError]
public func decode_config(s: &str, config: Config) -> Result[List[U8], DecodeError]
```

### 4.3 Hexadecimal

```tml
module hex

/// Encode bytes to lowercase hex string
public func encode(bytes: &[U8]) -> String

/// Encode bytes to uppercase hex string
public func encode_upper(bytes: &[U8]) -> String

/// Decode hex string to bytes
public func decode(s: &str) -> Result[List[U8], DecodeError]

/// Encode single byte to hex chars
public func encode_byte(b: U8) -> (Char, Char)

/// Decode two hex chars to byte
public func decode_byte(high: Char, low: Char) -> Result[U8, DecodeError]

public type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

public type DecodeErrorKind =
    | InvalidChar(Char)
    | InvalidLength
```

## 5. Percent Encoding (URL)

```tml
module percent

public type EncodeSet =
    | Path          // Encode except unreserved + /
    | Query         // Encode except unreserved + ?&=
    | Fragment      // Encode except unreserved
    | UserInfo      // Encode except unreserved + :@
    | Component     // Encode all except unreserved
    | Custom(func(U8) -> Bool)

/// Percent-encode string
public func encode(s: &str, set: EncodeSet) -> String

/// Percent-decode string
public func decode(s: &str) -> Result[String, DecodeError]

/// Encode for URL path
public func encode_path(s: &str) -> String {
    return encode(s, Path)
}

/// Encode for URL query
public func encode_query(s: &str) -> String {
    return encode(s, Query)
}

/// Encode for form data (space as +)
public func encode_form(s: &str) -> String

/// Decode form data
public func decode_form(s: &str) -> Result[String, DecodeError]
```

## 6. Charset Detection

```tml
module detect

public type Encoding =
    | Utf8
    | Utf16Le
    | Utf16Be
    | Utf32Le
    | Utf32Be
    | Ascii
    | Latin1
    | Unknown

/// Detect encoding from BOM
public func from_bom(bytes: &[U8]) -> Option[Encoding]

/// Detect encoding heuristically
public func detect(bytes: &[U8]) -> Encoding

/// Confidence-based detection
public func detect_with_confidence(bytes: &[U8]) -> (Encoding, F64)
```

## 7. Streaming Encoder/Decoder

### 7.1 Streaming Base64

```tml
module base64.stream

public type Encoder[W: Write] {
    writer: W,
    config: Config,
    buffer: [U8; 3],
    buffer_len: U8,
}

extend Encoder[W: Write] {
    public func new(writer: W) -> This {
        return This.with_config(writer, STANDARD)
    }

    public func with_config(writer: W, config: Config) -> This {
        return This {
            writer: writer,
            config: config,
            buffer: [0; 3],
            buffer_len: 0,
        }
    }

    public func write(this, data: &[U8]) -> Result[Unit, IoError]
    public func finish(this) -> Result[W, IoError]
}

extend Encoder[W: Write] with Write { ... }

public type Decoder[R: Read] {
    reader: R,
    config: Config,
    buffer: [U8; 4],
    buffer_len: U8,
}

extend Decoder[R: Read] {
    public func new(reader: R) -> This
    public func with_config(reader: R, config: Config) -> This
}

extend Decoder[R: Read] with Read { ... }
```

## 8. Unicode Normalization

```tml
module unicode

public type NormalizationForm =
    | Nfd   // Canonical Decomposition
    | Nfc   // Canonical Decomposition + Composition
    | Nfkd  // Compatibility Decomposition
    | Nfkc  // Compatibility Decomposition + Composition

/// Normalize string
public func normalize(s: &str, form: NormalizationForm) -> String

/// Check if string is normalized
public func is_normalized(s: &str, form: NormalizationForm) -> Bool

/// Iterator over grapheme clusters
public func graphemes(s: &str) -> Graphemes

/// Iterator over words
public func words(s: &str) -> Words

public type Graphemes { ... }
extend Graphemes with Iterator { type Item = &str }

public type Words { ... }
extend Words with Iterator { type Item = &str }
```

## 9. Character Properties

```tml
module unicode.props

/// Unicode general category
public type Category =
    | Letter(LetterCategory)
    | Mark(MarkCategory)
    | Number(NumberCategory)
    | Punctuation(PunctuationCategory)
    | Symbol(SymbolCategory)
    | Separator(SeparatorCategory)
    | Other(OtherCategory)

public func category(c: Char) -> Category
public func is_alphabetic(c: Char) -> Bool
public func is_numeric(c: Char) -> Bool
public func is_alphanumeric(c: Char) -> Bool
public func is_whitespace(c: Char) -> Bool
public func is_control(c: Char) -> Bool
public func is_uppercase(c: Char) -> Bool
public func is_lowercase(c: Char) -> Bool
public func to_uppercase(c: Char) -> Chars
public func to_lowercase(c: Char) -> Chars
public func to_titlecase(c: Char) -> Chars

/// Script detection
public type Script = Latin | Cyrillic | Greek | Arabic | Hebrew | Han | Hiragana | ...
public func script(c: Char) -> Script
```

## 10. Examples

### 10.1 Base64 Encoding

```tml
module example
import std.encoding.base64

func main() {
    // Encode
    let data = b"Hello, World!"
    let encoded = base64.encode(data)
    println(encoded)  // "SGVsbG8sIFdvcmxkIQ=="

    // Decode
    let decoded = base64.decode("SGVsbG8sIFdvcmxkIQ==").unwrap()
    println(String.from_utf8(decoded).unwrap())  // "Hello, World!"

    // URL-safe encoding
    let url_encoded = base64.encode_config(data, base64.URL_SAFE)
}
```

### 10.2 URL Encoding

```tml
module url_example
import std.encoding.percent

func build_url(base: &str, params: Map[String, String]) -> String {
    var url = String.from(base)
    url.push('?')

    var first = true
    loop (key, value) in params.entries() {
        if not first { url.push('&') }
        first = false

        url.push_str(percent.encode_query(key))
        url.push('=')
        url.push_str(percent.encode_query(value))
    }

    return url
}
```

### 10.3 Character Set Conversion

```tml
module charset
import std.encoding.{utf8, utf16, latin1}

func convert_to_utf8(data: &[U8], source_encoding: &str) -> Result[String, Error] {
    when source_encoding {
        "utf-8" -> utf8.decode(data).map_err(Error.from),
        "utf-16" -> utf16.decode(data).map_err(Error.from),
        "utf-16le" -> utf16.decode_le(data).map_err(Error.from),
        "utf-16be" -> utf16.decode_be(data).map_err(Error.from),
        "iso-8859-1" -> Ok(latin1.decode(data)),
        "latin1" -> Ok(latin1.decode(data)),
        _ -> Err(Error.new("unsupported encoding: " + source_encoding)),
    }
}
```

---

*Previous: [03-BUFFER.md](./03-BUFFER.md)*
*Next: [05-CRYPTO.md](./05-CRYPTO.md) — Cryptographic Primitives*
