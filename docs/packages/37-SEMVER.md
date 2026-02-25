# TML Standard Library: Semver

> `std::semver` -- Semantic Versioning 2.0.0 parsing, comparison, and requirement matching.

## Overview

Implements the [SemVer 2.0.0](https://semver.org/) specification. Provides `Version` for representing and comparing semantic versions, and `VersionReq` for matching versions against requirement constraints (caret, tilde, exact, range operators).

Build metadata is ignored for precedence per the SemVer spec. Pre-release versions have lower precedence than the associated normal version. Numeric pre-release identifiers are compared as integers; alphanumeric identifiers are compared lexicographically.

## Import

```tml
use std::semver::{Version, VersionReq}
```

---

## Version

Represents a semantic version: `MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]`.

```tml
pub type Version {
    major: I64,
    minor: I64,
    patch: I64,
    pre: Str,     // Pre-release string (empty if none)
    build: Str    // Build metadata string (empty if none)
}
```

### Construction

```tml
func Version::new(major: I64, minor: I64, patch: I64) -> Version
func Version::new_pre(major: I64, minor: I64, patch: I64, pre: Str) -> Version
func Version::parse(s: Str) -> Outcome[Version, Str]
```

`parse` accepts formats like `"1.2.3"`, `"v1.2.3-alpha.1+build.456"`. Leading `v`/`V` is optional. Leading zeros in numeric components are rejected.

### Methods

```tml
func is_prerelease(this) -> Bool
func eq(this, other: Version) -> Bool          // Equality (ignores build metadata)
func compare(this, other: Version) -> I32      // -1, 0, or 1
func lt(this, other: Version) -> Bool
func gt(this, other: Version) -> Bool
func le(this, other: Version) -> Bool
func ge(this, other: Version) -> Bool
func to_string(this) -> Str                    // "MAJOR.MINOR.PATCH[-pre][+build]"
func bump_major(this) -> Version               // Increment major, reset minor/patch
func bump_minor(this) -> Version               // Increment minor, reset patch
func bump_patch(this) -> Version               // Increment patch
```

---

## VersionReq

A version requirement for matching against versions.

```tml
pub type VersionReq {
    op: I32,          // 0=exact, 1=gt, 2=ge, 3=lt, 4=le, 5=caret, 6=tilde
    version: Version
}
```

### Construction

```tml
func VersionReq::parse(s: Str) -> Outcome[VersionReq, Str]
```

Accepts: `"=1.2.3"`, `">=1.0.0"`, `">1.0.0"`, `"<2.0.0"`, `"<=2.0.0"`, `"^1.2.3"`, `"~1.2.3"`, `"1.2.3"` (defaults to caret).

### Methods

```tml
func matches(this, v: Version) -> Bool
```

Caret (`^`): `^1.2.3` matches `>=1.2.3, <2.0.0`. Tilde (`~`): `~1.2.3` matches `>=1.2.3, <1.3.0`.

---

## Example

```tml
use std::semver::{Version, VersionReq}

func main() {
    let v = Version::parse("1.4.2-beta.1")
    when v {
        Ok(ver) -> {
            println(ver.to_string())         // "1.4.2-beta.1"
            println(ver.is_prerelease())      // true
            println(ver.bump_minor().to_string())  // "1.5.0"
        }
        Err(e) -> println(e)
    }

    let req = VersionReq::parse("^1.2.0")
    when req {
        Ok(r) -> {
            let stable = Version::new(1, 4, 2)
            println(r.matches(stable))  // true
        }
        Err(e) -> println(e)
    }
}
```
