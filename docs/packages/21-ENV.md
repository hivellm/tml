# TML Standard Library: Environment

> `std.env` — Environment variables and process information.

## Overview

The env package provides access to environment variables, command-line arguments, and other process environment information.

**Capability**: `io.process.env`

## Import

```tml
use std::env
use std::env.{var, args, current_dir}
```

---

## Environment Variables

### Reading Variables

```tml
/// Gets an environment variable
pub func var(key: ref String) -> Outcome[String, VarError]
    caps: [io.process.env]

/// Gets an environment variable or returns a default
pub func var_or(key: ref String, default: ref String) -> String
    caps: [io.process.env]
{
    var(key).unwrap_or_else(|_| default.duplicate())
}

/// Gets an environment variable as Option
pub func var_opt(key: ref String) -> Maybe[String]
    caps: [io.process.env]
{
    var(key).ok()
}

/// Returns true if the variable is set
pub func var_exists(key: ref String) -> Bool
    caps: [io.process.env]
{
    var(key).is_ok()
}

/// Error when reading environment variable
pub type VarError = NotPresent | NotUnicode(String)
```

### Setting Variables

```tml
/// Sets an environment variable
pub func set_var(key: ref String, value: ref String)
    caps: [io.process.env]

/// Removes an environment variable
pub func remove_var(key: ref String)
    caps: [io.process.env]
```

### Iterating Variables

```tml
/// Returns an iterator over all environment variables
pub func vars() -> Vars
    caps: [io.process.env]

/// Iterator over environment variables
pub type Vars {
    // Internal
}

implement Iterator for Vars {
    type Item = (String, String)

    func next(mut this) -> Maybe[(String, String)]
}

// Usage
loop (key, value) in env.vars() {
    println!("{}={}", key, value)
}
```

---

## Command Line Arguments

```tml
/// Returns an iterator over command-line arguments
pub func args() -> Args
    caps: [io.process.env]

/// Returns command-line arguments as a Vec
pub func args_vec() -> Vec[String]
    caps: [io.process.env]
{
    args().collect()
}

/// Iterator over command-line arguments
pub type Args {
    // Internal
}

implement Iterator for Args {
    type Item = String

    func next(mut this) -> Maybe[String]
}

implement ExactSizeIterator for Args {
    func len(this) -> U64
}

// Usage
let args: Vec[String] = env.args().collect()
let program_name = args[0].duplicate()
let arguments = args[1 to args.len()].to_vec()
```

### OS-Specific Arguments

```tml
/// Returns arguments as OsStrings (platform encoding)
pub func args_os() -> ArgsOs
    caps: [io.process.env]

/// Iterator over OsString arguments
pub type ArgsOs {
    // Internal
}

implement Iterator for ArgsOs {
    type Item = OsString

    func next(mut this) -> Maybe[OsString]
}
```

---

## Directories

### Current Directory

```tml
/// Returns the current working directory
pub func current_dir() -> Outcome[PathBuf, IoError]
    caps: [io.process.env]

/// Sets the current working directory
pub func set_current_dir(path: ref Path) -> Outcome[Unit, IoError]
    caps: [io.process.env]
```

### Standard Directories

```tml
/// Returns the home directory
pub func home_dir() -> Maybe[PathBuf]
    caps: [io.process.env]

/// Returns the temporary directory
pub func temp_dir() -> PathBuf
    caps: [io.process.env]

/// Returns the executable path
pub func current_exe() -> Outcome[PathBuf, IoError]
    caps: [io.process.env]
```

### Platform-Specific Directories

```tml
/// Standard directories for the platform
public module dirs {
    /// User's home directory
    pub func home() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's config directory
    /// - Linux: $XDG_CONFIG_HOME or ~/.config
    /// - macOS: ~/Library/Application Support
    /// - Windows: %APPDATA%
    pub func config() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's data directory
    /// - Linux: $XDG_DATA_HOME or ~/.local/share
    /// - macOS: ~/Library/Application Support
    /// - Windows: %APPDATA%
    pub func data() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's local data directory
    /// - Linux: $XDG_DATA_HOME or ~/.local/share
    /// - macOS: ~/Library/Application Support
    /// - Windows: %LOCALAPPDATA%
    pub func data_local() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's cache directory
    /// - Linux: $XDG_CACHE_HOME or ~/.cache
    /// - macOS: ~/Library/Caches
    /// - Windows: %LOCALAPPDATA%
    pub func cache() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's downloads directory
    pub func downloads() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's documents directory
    pub func documents() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's desktop directory
    pub func desktop() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's pictures directory
    pub func pictures() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's music directory
    pub func music() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// User's videos directory
    pub func videos() -> Maybe[PathBuf]
        caps: [io.process.env]

    /// System's runtime directory
    /// - Linux: $XDG_RUNTIME_DIR
    /// - macOS: None
    /// - Windows: None
    pub func runtime() -> Maybe[PathBuf]
        caps: [io.process.env]
}
```

---

## Platform Information

```tml
/// Target operating system
pub const OS: String = // "linux", "macos", "windows", etc.

/// Target architecture
pub const ARCH: String = // "x86_64", "aarch64", "arm", etc.

/// Target pointer width in bits
pub const POINTER_WIDTH: U32 = // 32 or 64

/// Target endianness
pub const ENDIAN: String = // "little" or "big"

/// Target family
pub const FAMILY: String = // "unix" or "windows"
```

### Runtime Checks

```tml
/// Returns the number of logical CPUs
pub func num_cpus() -> U64
    caps: [io.process.env]

/// Returns the page size
pub func page_size() -> U64
    caps: [io.process.env]

/// Returns the hostname
pub func hostname() -> Outcome[String, IoError]
    caps: [io.process.env]

/// Returns the OS type
pub func os_type() -> String
    caps: [io.process.env]
// "Linux", "Darwin", "Windows", etc.

/// Returns the OS release version
pub func os_release() -> Outcome[String, IoError]
    caps: [io.process.env]
```

---

## Path Manipulation

### PATH variable

```tml
/// Joins paths with the platform path separator
pub func join_paths[I](paths: I) -> Outcome[OsString, JoinPathsError]
    where I: Iterator[Item = impl AsRef[Path]]

/// Splits the PATH variable into paths
pub func split_paths(path: ref OsStr) -> SplitPaths

/// Iterator over PATH components
pub type SplitPaths {
    // Internal
}

implement Iterator for SplitPaths {
    type Item = PathBuf

    func next(mut this) -> Maybe[PathBuf]
}

// Usage
let path = env.var("PATH").unwrap()
loop dir in env.split_paths(ref path) {
    println!("{}", dir.display())
}
```

---

## Executable Search

```tml
/// Finds an executable in PATH
pub func which(name: ref String) -> Maybe[PathBuf]
    caps: [io.process.env]
{
    let path = var("PATH").ok()?
    loop dir in split_paths(ref path) {
        let candidate = dir.join(name)
        if candidate.exists() and candidate.is_executable() then {
            return Just(candidate)
        }
    }
    return Nothing
}

/// Finds all matching executables in PATH
pub func which_all(name: ref String) -> Vec[PathBuf]
    caps: [io.process.env]
```

---

## Examples

### Reading Configuration from Environment

```tml
use std::env

type Config {
    database_url: String,
    port: U16,
    debug: Bool,
    log_level: String,
}

func load_config() -> Outcome[Config, ConfigError]
    caps: [io.process.env]
{
    Ok(Config {
        database_url: env.var("DATABASE_URL")?,
        port: env.var("PORT")
            .unwrap_or("8080")
            .parse()
            .map_err(|_| ConfigError.InvalidPort)?,
        debug: env.var("DEBUG")
            .map(|v| v == "true" or v == "1")
            .unwrap_or(false),
        log_level: env.var_or("LOG_LEVEL", "info"),
    })
}
```

### Application Directories

```tml
use std::env.dirs
use std::fs

type AppDirs {
    config: PathBuf,
    data: PathBuf,
    cache: PathBuf,
}

func get_app_dirs(app_name: ref String) -> Outcome[AppDirs, IoError]
    caps: [io.process.env, io::file]
{
    let config = dirs.config()
        .ok_or(IoError.NotFound)?
        .join(app_name)

    let data = dirs.data()
        .ok_or(IoError.NotFound)?
        .join(app_name)

    let cache = dirs.cache()
        .ok_or(IoError.NotFound)?
        .join(app_name)

    // Ensure directories exist
    fs.create_dir_all(ref config)?
    fs.create_dir_all(ref data)?
    fs.create_dir_all(ref cache)?

    Ok(AppDirs { config, data, cache })
}
```

### Platform-Specific Behavior

```tml
use std::env

func get_config_path() -> PathBuf
    caps: [io.process.env]
{
    if env.OS == "windows" then {
        env.var("APPDATA")
            .map(|p| PathBuf.from(p).join("MyApp"))
            .unwrap_or_else(|_| PathBuf.from("C:\\MyApp"))
    } else if env.OS == "macos" then {
        env.home_dir()
            .map(|h| h.join("Library").join("Application Support").join("MyApp"))
            .unwrap_or_else(|| PathBuf.from("/tmp/MyApp"))
    } else {
        // Linux and others
        env.var("XDG_CONFIG_HOME")
            .map(|p| PathBuf.from(p).join("myapp"))
            .unwrap_or_else(|_| {
                env.home_dir()
                    .map(|h| h.join(".config").join("myapp"))
                    .unwrap_or_else(|| PathBuf.from("/etc/myapp"))
            })
    }
}
```

### Setting Up Test Environment

```tml
use std::env

func with_env[T](vars: ref [(String, String)], f: func() -> T) -> T
    caps: [io.process.env]
{
    // Save original values
    var original: Vec[(String, Maybe[String])] = Vec.new()
    loop (key, value) in vars.iter() {
        original.push((key.duplicate(), env.var_opt(key)))
        env.set_var(key, value)
    }

    // Run function
    let result = f()

    // Restore original values
    loop (key, original_value) in original.iter() {
        when original_value {
            Just(v) -> env.set_var(key, v),
            Nothing -> env.remove_var(key),
        }
    }

    return result
}

// Usage in tests
#[test]
func test_config()
    caps: [io.process.env]
{
    with_env(&[
        ("DATABASE_URL", "postgres://test"),
        ("DEBUG", "true"),
    ], do() {
        let config = load_config().unwrap()
        assert(config.debug)
    })
}
```

---

## See Also

- [std.args](./19-ARGS.md) — Command-line argument parsing
- [std.fs](./01-FS.md) — File system operations
- [std.process](./22-PROCESS.md) — Process management
