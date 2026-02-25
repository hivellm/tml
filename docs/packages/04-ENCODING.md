# std::encoding — Text and Binary Encodings

## 1. Overview

The `std::encoding` package provides text encoding/decoding (UTF-8, UTF-16, ASCII, Latin-1) and binary encoding (Base64, Hex).

```tml
use std::encoding
use std::encoding.{base64, hex, utf8, utf16}
```

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. Text Encodings

### 3.1 UTF-8

```tml
mod utf8

/// Check if bytes are valid UTF-8
pub func is_valid(bytes: ref [U8]) -> Bool

/// Decode bytes to string
pub func decode(bytes: ref [U8]) -> Outcome[String, Utf8Error]

/// Decode bytes, replacing invalid sequences with replacement char
pub func decode_lossy(bytes: ref [U8]) -> String

/// Encode string to bytes (always succeeds - String is UTF-8)
pub func encode(s: ref str) -> ref [U8] {
    return s.as_bytes()
}

/// Count characters (not bytes)
pub func char_count(s: ref str) -> U64

/// Get byte length of char at position
pub func char_len(byte: U8) -> U64 {
    if (byte & 0x80) == 0 { return 1 }
    if (byte & 0xE0) == 0xC0 { return 2 }
    if (byte & 0xF0) == 0xE0 { return 3 }
    if (byte & 0xF8) == 0xF0 { return 4 }
    return 1  // Invalid, treat as single byte
}

pub type Utf8Error {
    valid_up_to: U64,
    error_len: Maybe[U64],
}

extend Utf8Error {
    pub func valid_up_to(this) -> U64 { this.valid_up_to }
    pub func error_len(this) -> Maybe[U64] { this.error_len }
}
```

### 3.2 UTF-16

```tml
mod utf16

/// Decode UTF-16 LE bytes to string
pub func decode_le(bytes: ref [U8]) -> Outcome[String, DecodeError]

/// Decode UTF-16 BE bytes to string
pub func decode_be(bytes: ref [U8]) -> Outcome[String, DecodeError]

/// Decode with BOM detection
pub func decode(bytes: ref [U8]) -> Outcome[String, DecodeError]

/// Encode string to UTF-16 LE
pub func encode_le(s: ref str) -> List[U8]

/// Encode string to UTF-16 BE
pub func encode_be(s: ref str) -> List[U8]

/// Encode with BOM
pub func encode_with_bom(s: ref str, endian: Endian) -> List[U8]

pub type Endian = Little | Big

pub type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

pub type DecodeErrorKind =
    | InvalidLength      // Odd number of bytes
    | InvalidSurrogate   // Unpaired surrogate
    | InvalidCodePoint   // Code point out of range
```

### 3.3 ASCII

```tml
mod ascii

/// Check if all bytes are ASCII (< 128)
pub func is_ascii(bytes: ref [U8]) -> Bool

/// Check if string is ASCII
pub func is_ascii_str(s: ref str) -> Bool

/// Decode ASCII bytes to string
pub func decode(bytes: ref [U8]) -> Outcome[String, AsciiError]

/// Decode, replacing non-ASCII with '?'
pub func decode_lossy(bytes: ref [U8]) -> String

/// Encode string to ASCII (fails if non-ASCII chars)
pub func encode(s: ref str) -> Outcome[List[U8], AsciiError]

/// Convert to uppercase ASCII
pub func to_uppercase(bytes: mut ref [U8])

/// Convert to lowercase ASCII
pub func to_lowercase(bytes: mut ref [U8])

pub type AsciiError {
    position: U64,
    byte: U8,
}
```

### 3.4 Latin-1 (ISO-8859-1)

```tml
mod latin1

/// Decode Latin-1 bytes to string
pub func decode(bytes: ref [U8]) -> String  // Always succeeds

/// Encode string to Latin-1
pub func encode(s: ref str) -> Outcome[List[U8], EncodeError]

/// Encode, replacing unencodable chars
pub func encode_lossy(s: ref str) -> List[U8]

pub type EncodeError {
    position: U64,
    char: Char,
}
```

## 4. Binary Encodings

### 4.1 Base64

```tml
mod base64

pub type Config {
    alphabet: Alphabet,
    padding: Bool,
}

pub type Alphabet = Standard | UrlSafe

pub const STANDARD: Config = Config { alphabet: Standard, padding: true }
pub const URL_SAFE: Config = Config { alphabet: UrlSafe, padding: false }
pub const URL_SAFE_PADDED: Config = Config { alphabet: UrlSafe, padding: true }

/// Encode bytes to base64 string
pub func encode(bytes: ref [U8]) -> String {
    return encode_config(bytes, STANDARD)
}

/// Encode with custom config
pub func encode_config(bytes: ref [U8], config: Config) -> String

/// Decode base64 string to bytes
pub func decode(s: ref str) -> Outcome[List[U8], DecodeError]

/// Decode with custom config
pub func decode_config(s: ref str, config: Config) -> Outcome[List[U8], DecodeError]

/// Calculate encoded length
pub func encoded_len(input_len: U64, padding: Bool) -> U64 {
    let base_len = (input_len + 2) / 3 * 4
    if padding {
        base_len
    } else {
        base_len - (3 - input_len % 3) % 3
    }
}

/// Calculate decoded length
pub func decoded_len(input_len: U64) -> U64 {
    return input_len / 4 * 3
}

pub type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

pub type DecodeErrorKind =
    | InvalidChar(Char)
    | InvalidLength
    | InvalidPadding
```

### 4.2 Base32

```tml
mod base32

pub type Config {
    alphabet: Alphabet,
    padding: Bool,
}

pub type Alphabet = Standard | Hex

pub const STANDARD: Config = Config { alphabet: Standard, padding: true }
pub const HEX: Config = Config { alphabet: Hex, padding: true }

pub func encode(bytes: ref [U8]) -> String
pub func encode_config(bytes: ref [U8], config: Config) -> String
pub func decode(s: ref str) -> Outcome[List[U8], DecodeError]
pub func decode_config(s: ref str, config: Config) -> Outcome[List[U8], DecodeError]
```

### 4.3 Hexadecimal

```tml
mod hex

/// Encode bytes to lowercase hex string
pub func encode(bytes: ref [U8]) -> String

/// Encode bytes to uppercase hex string
pub func encode_upper(bytes: ref [U8]) -> String

/// Decode hex string to bytes
pub func decode(s: ref str) -> Outcome[List[U8], DecodeError]

/// Encode single byte to hex chars
pub func encode_byte(b: U8) -> (Char, Char)

/// Decode two hex chars to byte
pub func decode_byte(high: Char, low: Char) -> Outcome[U8, DecodeError]

pub type DecodeError {
    kind: DecodeErrorKind,
    position: U64,
}

pub type DecodeErrorKind =
    | InvalidChar(Char)
    | InvalidLength
```

## 5. Percent Encoding (URL)

```tml
mod percent

pub type EncodeSet =
    | Path          // Encode except unreserved + /
    | Query         // Encode except unreserved + ?&=
    | Fragment      // Encode except unreserved
    | UserInfo      // Encode except unreserved + :@
    | Component     // Encode all except unreserved
    | Custom(func(U8) -> Bool)

/// Percent-encode string
pub func encode(s: ref str, set: EncodeSet) -> String

/// Percent-decode string
pub func decode(s: ref str) -> Outcome[String, DecodeError]

/// Encode for URL path
pub func encode_path(s: ref str) -> String {
    return encode(s, Path)
}

/// Encode for URL query
pub func encode_query(s: ref str) -> String {
    return encode(s, Query)
}

/// Encode for form data (space as +)
pub func encode_form(s: ref str) -> String

/// Decode form data
pub func decode_form(s: ref str) -> Outcome[String, DecodeError]
```

## 6. Charset Detection

```tml
mod detect

pub type Encoding =
    | Utf8
    | Utf16Le
    | Utf16Be
    | Utf32Le
    | Utf32Be
    | Ascii
    | Latin1
    | Unknown

/// Detect encoding from BOM
pub func from_bom(bytes: ref [U8]) -> Maybe[Encoding]

/// Detect encoding heuristically
pub func detect(bytes: ref [U8]) -> Encoding

/// Confidence-based detection
pub func detect_with_confidence(bytes: ref [U8]) -> (Encoding, F64)
```

## 7. Streaming Encoder/Decoder

### 7.1 Streaming Base64

```tml
mod base64.stream

pub type Encoder[W: Write] {
    writer: W,
    config: Config,
    buffer: [U8; 3],
    buffer_len: U8,
}

extend Encoder[W: Write] {
    pub func new(writer: W) -> This {
        return This.with_config(writer, STANDARD)
    }

    pub func with_config(writer: W, config: Config) -> This {
        return This {
            writer: writer,
            config: config,
            buffer: [0; 3],
            buffer_len: 0,
        }
    }

    pub func write(this, data: ref [U8]) -> Outcome[Unit, IoError]
    pub func finish(this) -> Outcome[W, IoError]
}

extend Encoder[W: Write] with Write { ... }

pub type Decoder[R: Read] {
    reader: R,
    config: Config,
    buffer: [U8; 4],
    buffer_len: U8,
}

extend Decoder[R: Read] {
    pub func new(reader: R) -> This
    pub func with_config(reader: R, config: Config) -> This
}

extend Decoder[R: Read] with Read { ... }
```

## 8. Unicode Normalization

```tml
mod unicode

pub type NormalizationForm =
    | Nfd   // Canonical Decomposition
    | Nfc   // Canonical Decomposition + Composition
    | Nfkd  // Compatibility Decomposition
    | Nfkc  // Compatibility Decomposition + Composition

/// Normalize string
pub func normalize(s: ref str, form: NormalizationForm) -> String

/// Check if string is normalized
pub func is_normalized(s: ref str, form: NormalizationForm) -> Bool

/// Iterator over grapheme clusters
pub func graphemes(s: ref str) -> Graphemes

/// Iterator over words
pub func words(s: ref str) -> Words

pub type Graphemes { ... }
extend Graphemes with Iterator { type Item = ref str }

pub type Words { ... }
extend Words with Iterator { type Item = ref str }
```

## 9. Character Properties

```tml
mod unicode.props

/// Unicode general category
pub type Category =
    | Letter(LetterCategory)
    | Mark(MarkCategory)
    | Number(NumberCategory)
    | Punctuation(PunctuationCategory)
    | Symbol(SymbolCategory)
    | Separator(SeparatorCategory)
    | Other(OtherCategory)

pub func category(c: Char) -> Category
pub func is_alphabetic(c: Char) -> Bool
pub func is_numeric(c: Char) -> Bool
pub func is_alphanumeric(c: Char) -> Bool
pub func is_whitespace(c: Char) -> Bool
pub func is_control(c: Char) -> Bool
pub func is_uppercase(c: Char) -> Bool
pub func is_lowercase(c: Char) -> Bool
pub func to_uppercase(c: Char) -> Chars
pub func to_lowercase(c: Char) -> Chars
pub func to_titlecase(c: Char) -> Chars

/// Script detection
pub type Script = Latin | Cyrillic | Greek | Arabic | Hebrew | Han | Hiragana | ...
pub func script(c: Char) -> Script
```

## 10. Examples

### 10.1 Base64 Encoding

```tml
mod example
use std::encoding.base64

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
mod url_example
use std::encoding.percent

func build_url(base: ref str, params: Map[String, String]) -> String {
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
mod charset
use std::encoding.{utf8, utf16, latin1}

func convert_to_utf8(data: ref [U8], source_encoding: ref str) -> Outcome[String, Error] {
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
