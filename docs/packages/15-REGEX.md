# TML Standard Library: Regular Expressions

> `std.regex` — Pattern matching with regular expressions.

## Overview

The regex package provides regular expression support for pattern matching, searching, and text manipulation. It supports a Perl-compatible regex syntax with Unicode support.

## Import

```tml
import std.regex
import std.regex.{Regex, Captures, Match}
```

---

## Regex Type

### Construction

```tml
/// A compiled regular expression
public type Regex {
    // Internal compiled representation
}

extend Regex {
    /// Compiles a regex pattern
    public func new(pattern: String) -> Result[Regex, RegexError] {
        Regex.with_options(pattern, RegexOptions.default())
    }

    /// Compiles with options
    public func with_options(pattern: String, options: RegexOptions) -> Result[Regex, RegexError]

    /// Returns the original pattern
    public func as_str(this) -> &String {
        &this.pattern
    }

    /// Returns the number of capture groups
    public func captures_len(this) -> U64

    /// Returns the names of capture groups
    public func capture_names(this) -> Vec[Option[String]]
}

/// Regex compilation options
public type RegexOptions {
    case_insensitive: Bool,
    multi_line: Bool,
    dot_matches_newline: Bool,
    unicode: Bool,
    ignore_whitespace: Bool,
}

extend RegexOptions {
    /// Default options
    public func default() -> RegexOptions {
        return RegexOptions {
            case_insensitive: false,
            multi_line: false,
            dot_matches_newline: false,
            unicode: true,
            ignore_whitespace: false,
        }
    }

    /// Case insensitive matching
    public func case_insensitive(mut this, value: Bool) -> RegexOptions {
        this.case_insensitive = value
        return this
    }

    /// ^ and $ match line boundaries
    public func multi_line(mut this, value: Bool) -> RegexOptions {
        this.multi_line = value
        return this
    }

    /// . matches newline
    public func dot_matches_newline(mut this, value: Bool) -> RegexOptions {
        this.dot_matches_newline = value
        return this
    }

    /// Enable Unicode support
    public func unicode(mut this, value: Bool) -> RegexOptions {
        this.unicode = value
        return this
    }

    /// Ignore whitespace and allow comments
    public func ignore_whitespace(mut this, value: Bool) -> RegexOptions {
        this.ignore_whitespace = value
        return this
    }
}

/// Regex compilation error
public type RegexError {
    message: String,
    position: U64,
}
```

---

## Matching

### is_match

```tml
extend Regex {
    /// Returns true if the pattern matches anywhere in the text
    public func is_match(this, text: &String) -> Bool

    /// Returns true if the pattern matches at the start of text
    public func is_match_at(this, text: &String, start: U64) -> Bool
}

// Example:
let re = Regex.new(r"^\d{3}-\d{4}$").unwrap()
assert(re.is_match("123-4567"))
assert(not re.is_match("12-345"))
```

### find

```tml
extend Regex {
    /// Finds the first match in the text
    public func find(this, text: &String) -> Option[Match]

    /// Finds the first match starting at position
    public func find_at(this, text: &String, start: U64) -> Option[Match]

    /// Finds all non-overlapping matches
    public func find_all(this, text: &String) -> Matches
}

/// A single match
public type Match {
    text: String,
    start: U64,
    end: U64,
}

extend Match {
    /// Returns the matched text
    public func as_str(this) -> &String {
        &this.text
    }

    /// Returns the start byte position
    public func start(this) -> U64 {
        this.start
    }

    /// Returns the end byte position
    public func end(this) -> U64 {
        this.end
    }

    /// Returns the length in bytes
    public func len(this) -> U64 {
        this.end - this.start
    }

    /// Returns the byte range
    public func range(this) -> Range[U64] {
        this.start..this.end
    }
}

/// Iterator over matches
public type Matches {
    regex: Regex,
    text: String,
    pos: U64,
}

implement Iterator for Matches {
    type Item = Match

    func next(mut this) -> Option[Match] {
        when this.regex.find_at(&this.text, this.pos) {
            Some(m) -> {
                this.pos = m.end
                return Some(m)
            },
            None -> return None,
        }
    }
}
```

---

## Captures

### Capture Groups

```tml
extend Regex {
    /// Captures the first match and all groups
    public func captures(this, text: &String) -> Option[Captures]

    /// Captures all matches and their groups
    public func captures_all(this, text: &String) -> CapturesIter
}

/// All capture groups from a single match
public type Captures {
    groups: Vec[Option[Match]],
    names: HashMap[String, U64],
}

extend Captures {
    /// Returns the full match (group 0)
    public func get_match(this) -> &Match {
        this.groups[0].as_ref().unwrap()
    }

    /// Returns capture group by index
    public func get(this, index: U64) -> Option[&Match] {
        this.groups.get(index).and_then(|g| g.as_ref())
    }

    /// Returns capture group by name
    public func name(this, name: &String) -> Option[&Match] {
        this.names.get(name).and_then(|i| this.get(*i))
    }

    /// Returns the number of groups
    public func len(this) -> U64 {
        this.groups.len()
    }

    /// Iterates over all groups
    public func iter(this) -> CaptureGroupIter {
        CaptureGroupIter { captures: this, index: 0 }
    }

    /// Expands a replacement template
    public func expand(this, template: &String, dest: &mut String) {
        // $0 = full match, $1 = group 1, $name = named group
    }
}

/// Iterator over captures
public type CapturesIter {
    regex: Regex,
    text: String,
    pos: U64,
}

implement Iterator for CapturesIter {
    type Item = Captures

    func next(mut this) -> Option[Captures] {
        when this.regex.captures_at(&this.text, this.pos) {
            Some(caps) -> {
                this.pos = caps.get_match().end
                return Some(caps)
            },
            None -> return None,
        }
    }
}
```

### Named Groups

```tml
// Named group syntax: (?P<name>pattern) or (?<name>pattern)
let re = Regex.new(r"(?P<year>\d{4})-(?P<month>\d{2})-(?P<day>\d{2})").unwrap()

when re.captures("2024-03-15") {
    Some(caps) -> {
        let year = caps.name("year").unwrap().as_str()
        let month = caps.name("month").unwrap().as_str()
        let day = caps.name("day").unwrap().as_str()
        print(year + "/" + month + "/" + day)
    },
    None -> print("No match"),
}
```

---

## Replacement

### replace

```tml
extend Regex {
    /// Replaces the first match
    public func replace(this, text: &String, rep: &String) -> String

    /// Replaces all matches
    public func replace_all(this, text: &String, rep: &String) -> String

    /// Replaces the first match using a function
    public func replace_fn[F](this, text: &String, f: F) -> String
        where F: Fn(&Captures) -> String

    /// Replaces all matches using a function
    public func replace_all_fn[F](this, text: &String, f: F) -> String
        where F: Fn(&Captures) -> String

    /// Replaces with limit
    public func replacen(this, text: &String, limit: U64, rep: &String) -> String
}

// Replacement syntax:
// $0 or $& = entire match
// $1, $2, ... = numbered groups
// $name = named group
// $$ = literal $

// Examples:
let re = Regex.new(r"(\w+)\s+(\w+)").unwrap()

// Simple replacement
let result = re.replace("hello world", "goodbye universe")
// result: "goodbye universe"

// With capture groups
let result = re.replace("hello world", "$2 $1")
// result: "world hello"

// With function
let result = re.replace_fn("hello world", do(caps) {
    caps.get(1).unwrap().as_str().to_uppercase() + " " +
    caps.get(2).unwrap().as_str().to_uppercase()
})
// result: "HELLO WORLD"
```

---

## Split

```tml
extend Regex {
    /// Splits text by the pattern
    public func split(this, text: &String) -> Split

    /// Splits with limit
    public func splitn(this, text: &String, limit: U64) -> SplitN
}

/// Iterator over split segments
public type Split {
    regex: Regex,
    text: String,
    pos: U64,
    done: Bool,
}

implement Iterator for Split {
    type Item = String

    func next(mut this) -> Option[String] {
        if this.done then return None

        when this.regex.find_at(&this.text, this.pos) {
            Some(m) -> {
                let segment = this.text[this.pos..m.start].to_string()
                this.pos = m.end
                return Some(segment)
            },
            None -> {
                this.done = true
                return Some(this.text[this.pos..].to_string())
            },
        }
    }
}

// Example:
let re = Regex.new(r"\s+").unwrap()
let parts: Vec[String] = re.split("hello   world  foo").collect()
// parts: ["hello", "world", "foo"]
```

---

## Regex Syntax

### Basic Patterns

```
.           Any character (except newline, unless s flag)
^           Start of string (or line with m flag)
$           End of string (or line with m flag)
\A          Start of string only
\z          End of string only
\b          Word boundary
\B          Not word boundary
```

### Character Classes

```
[abc]       Match a, b, or c
[^abc]      Match anything except a, b, or c
[a-z]       Match any lowercase letter
[a-zA-Z]    Match any letter
\d          Digit [0-9]
\D          Not digit [^0-9]
\w          Word character [a-zA-Z0-9_]
\W          Not word character
\s          Whitespace [ \t\n\r\f]
\S          Not whitespace
```

### Unicode Categories

```
\p{L}       Any letter
\p{Lu}      Uppercase letter
\p{Ll}      Lowercase letter
\p{N}       Any number
\p{P}       Punctuation
\p{S}       Symbol
\P{L}       Not a letter (negated)
```

### Quantifiers

```
*           0 or more (greedy)
+           1 or more (greedy)
?           0 or 1 (greedy)
{n}         Exactly n
{n,}        n or more
{n,m}       Between n and m

*?          0 or more (lazy)
+?          1 or more (lazy)
??          0 or 1 (lazy)
{n,}?       n or more (lazy)
{n,m}?      Between n and m (lazy)

*+          0 or more (possessive)
++          1 or more (possessive)
```

### Groups

```
(pattern)           Capturing group
(?:pattern)         Non-capturing group
(?P<name>pattern)   Named capturing group
(?<name>pattern)    Named capturing group (alternate)
(?=pattern)         Positive lookahead
(?!pattern)         Negative lookahead
(?<=pattern)        Positive lookbehind
(?<!pattern)        Negative lookbehind
```

### Flags

```
(?i)        Case insensitive
(?m)        Multi-line mode
(?s)        Dot matches newline
(?x)        Ignore whitespace, allow comments
(?-i)       Turn off case insensitivity
```

---

## RegexSet

Match multiple patterns efficiently.

```tml
/// A set of compiled regexes
public type RegexSet {
    patterns: Vec[Regex],
}

extend RegexSet {
    /// Creates a regex set from patterns
    public func new(patterns: &[String]) -> Result[RegexSet, RegexError]

    /// Returns true if any pattern matches
    public func is_match(this, text: &String) -> Bool

    /// Returns indices of matching patterns
    public func matches(this, text: &String) -> RegexSetMatches

    /// Returns the number of patterns
    public func len(this) -> U64
}

/// Set match results
public type RegexSetMatches {
    matches: Vec[Bool],
}

extend RegexSetMatches {
    /// Returns true if pattern at index matched
    public func matched(this, index: U64) -> Bool {
        this.matches.get(index).copied().unwrap_or(false)
    }

    /// Iterates over matching indices
    public func iter(this) -> impl Iterator[Item = U64] {
        this.matches.iter()
            .enumerate()
            .filter(do((_, m)) *m)
            .map(do((i, _)) i)
    }

    /// Returns the number of matches
    public func len(this) -> U64 {
        this.matches.iter().filter(do(m) *m).count()
    }
}

// Example:
let set = RegexSet.new(&[
    r"^\d+$",      // all digits
    r"^[a-z]+$",   // all lowercase
    r"^[A-Z]+$",   // all uppercase
]).unwrap()

let text = "hello"
let matches = set.matches(text)

if matches.matched(1) then {
    print("Text is all lowercase")
}
```

---

## RegexBuilder

Build complex regexes programmatically.

```tml
/// Builder for complex regex patterns
public type RegexBuilder {
    pattern: String,
    options: RegexOptions,
}

extend RegexBuilder {
    /// Creates a new builder
    public func new(pattern: String) -> RegexBuilder {
        return RegexBuilder {
            pattern: pattern,
            options: RegexOptions.default(),
        }
    }

    /// Sets case insensitive
    public func case_insensitive(mut this, value: Bool) -> RegexBuilder {
        this.options.case_insensitive = value
        return this
    }

    /// Sets multi-line mode
    public func multi_line(mut this, value: Bool) -> RegexBuilder {
        this.options.multi_line = value
        return this
    }

    /// Sets dot-matches-newline
    public func dot_matches_newline(mut this, value: Bool) -> RegexBuilder {
        this.options.dot_matches_newline = value
        return this
    }

    /// Sets unicode mode
    public func unicode(mut this, value: Bool) -> RegexBuilder {
        this.options.unicode = value
        return this
    }

    /// Sets ignore-whitespace mode
    public func ignore_whitespace(mut this, value: Bool) -> RegexBuilder {
        this.options.ignore_whitespace = value
        return this
    }

    /// Sets size limit for compiled regex
    public func size_limit(mut this, limit: U64) -> RegexBuilder

    /// Builds the regex
    public func build(this) -> Result[Regex, RegexError] {
        Regex.with_options(this.pattern, this.options)
    }
}
```

---

## Escape

```tml
/// Escapes special regex characters in a string
public func escape(text: &String) -> String {
    var result = String.new()
    loop c in text.chars() {
        when c {
            '\\' | '.' | '+' | '*' | '?' | '(' | ')' | '|' |
            '[' | ']' | '{' | '}' | '^' | '$' -> {
                result.push('\\')
                result.push(c)
            },
            _ -> result.push(c),
        }
    }
    return result
}

// Example:
let user_input = "hello (world)"
let safe = regex.escape(user_input)
// safe: "hello \\(world\\)"

let re = Regex.new(&safe).unwrap()
assert(re.is_match("hello (world)"))
```

---

## Examples

### Email Validation

```tml
import std.regex.Regex

func validate_email(email: &String) -> Bool {
    let re = Regex.new(
        r"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$"
    ).unwrap()
    return re.is_match(email)
}
```

### Parse Log File

```tml
import std.regex.{Regex, Captures}

type LogEntry {
    timestamp: String,
    level: String,
    message: String,
}

func parse_log_line(line: &String) -> Option[LogEntry] {
    let re = Regex.new(
        r"^\[(?P<time>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] \[(?P<level>\w+)\] (?P<msg>.*)$"
    ).unwrap()

    re.captures(line).map(do(caps) {
        LogEntry {
            timestamp: caps.name("time").unwrap().as_str().clone(),
            level: caps.name("level").unwrap().as_str().clone(),
            message: caps.name("msg").unwrap().as_str().clone(),
        }
    })
}
```

### Extract URLs

```tml
import std.regex.Regex

func extract_urls(text: &String) -> Vec[String] {
    let re = Regex.new(
        r"https?://[^\s<>\[\]{}|\\^`\"]+"
    ).unwrap()

    re.find_all(text)
        .map(do(m) m.as_str().clone())
        .collect()
}
```

### Template Processing

```tml
import std.regex.Regex

func process_template(template: &String, vars: &HashMap[String, String]) -> String {
    let re = Regex.new(r"\{\{(\w+)\}\}").unwrap()

    re.replace_all_fn(template, do(caps) {
        let name = caps.get(1).unwrap().as_str()
        vars.get(name).cloned().unwrap_or("".to_string())
    })
}

// Usage:
var vars = HashMap.new()
vars.insert("name", "World")
vars.insert("greeting", "Hello")

let result = process_template("{{greeting}}, {{name}}!", &vars)
// result: "Hello, World!"
```

---

## Performance

### Compiled Regex

Always compile regex patterns once and reuse:

```tml
// Good: compile once
let EMAIL_RE: Lazy[Regex] = Lazy.new(do() {
    Regex.new(r"^[\w.+-]+@[\w.-]+\.\w+$").unwrap()
})

func is_email(s: &String) -> Bool {
    EMAIL_RE.is_match(s)
}

// Bad: recompiles every call
func is_email_slow(s: &String) -> Bool {
    Regex.new(r"^[\w.+-]+@[\w.-]+\.\w+$").unwrap().is_match(s)
}
```

### Avoid Catastrophic Backtracking

```tml
// Bad: exponential backtracking on certain inputs
let bad = Regex.new(r"(a+)+$").unwrap()

// Good: use possessive quantifiers or atomic groups
let good = Regex.new(r"(a++)$").unwrap()
```

---

## See Also

- [std.string](./17-STRING.md) — String operations
- [04-ENCODING.md](./04-ENCODING.md) — Text encodings
