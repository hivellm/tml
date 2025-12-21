# TML Standard Library: Environment

> `std.env` — Environment variables and process information.

## Overview

The env package provides access to environment variables, command-line arguments, and other process environment information.

**Capability**: `io.process.env`

## Import

```tml
import std.env
import std.env.{var, args, current_dir}
```

---

## Environment Variables

### Reading Variables

```tml
/// Gets an environment variable
public func var(key: &String) -> Result[String, VarError]
    caps: [io.process.env]

/// Gets an environment variable or returns a default
public func var_or(key: &String, default: &String) -> String
    caps: [io.process.env]
{
    var(key).unwrap_or_else(|_| default.clone())
}

/// Gets an environment variable as Option
public func var_opt(key: &String) -> Option[String]
    caps: [io.process.env]
{
    var(key).ok()
}

/// Returns true if the variable is set
public func var_exists(key: &String) -> Bool
    caps: [io.process.env]
{
    var(key).is_ok()
}

/// Error when reading environment variable
public type VarError = NotPresent | NotUnicode(String)
```

### Setting Variables

```tml
/// Sets an environment variable
public func set_var(key: &String, value: &String)
    caps: [io.process.env]

/// Removes an environment variable
public func remove_var(key: &String)
    caps: [io.process.env]
```

### Iterating Variables

```tml
/// Returns an iterator over all environment variables
public func vars() -> Vars
    caps: [io.process.env]

/// Iterator over environment variables
public type Vars {
    // Internal
}

implement Iterator for Vars {
    type Item = (String, String)

    func next(mut this) -> Option[(String, String)]
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
public func args() -> Args
    caps: [io.process.env]

/// Returns command-line arguments as a Vec
public func args_vec() -> Vec[String]
    caps: [io.process.env]
{
    args().collect()
}

/// Iterator over command-line arguments
public type Args {
    // Internal
}

implement Iterator for Args {
    type Item = String

    func next(mut this) -> Option[String]
}

implement ExactSizeIterator for Args {
    func len(this) -> U64
}

// Usage
let args: Vec[String] = env.args().collect()
let program_name = args[0].clone()
let arguments = args[1..].to_vec()
```

### OS-Specific Arguments

```tml
/// Returns arguments as OsStrings (platform encoding)
public func args_os() -> ArgsOs
    caps: [io.process.env]

/// Iterator over OsString arguments
public type ArgsOs {
    // Internal
}

implement Iterator for ArgsOs {
    type Item = OsString

    func next(mut this) -> Option[OsString]
}
```

---

## Directories

### Current Directory

```tml
/// Returns the current working directory
public func current_dir() -> Result[PathBuf, IoError]
    caps: [io.process.env]

/// Sets the current working directory
public func set_current_dir(path: &Path) -> Result[Unit, IoError]
    caps: [io.process.env]
```

### Standard Directories

```tml
/// Returns the home directory
public func home_dir() -> Option[PathBuf]
    caps: [io.process.env]

/// Returns the temporary directory
public func temp_dir() -> PathBuf
    caps: [io.process.env]

/// Returns the executable path
public func current_exe() -> Result[PathBuf, IoError]
    caps: [io.process.env]
```

### Platform-Specific Directories

```tml
/// Standard directories for the platform
public module dirs {
    /// User's home directory
    public func home() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's config directory
    /// - Linux: $XDG_CONFIG_HOME or ~/.config
    /// - macOS: ~/Library/Application Support
    /// - Windows: %APPDATA%
    public func config() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's data directory
    /// - Linux: $XDG_DATA_HOME or ~/.local/share
    /// - macOS: ~/Library/Application Support
    /// - Windows: %APPDATA%
    public func data() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's local data directory
    /// - Linux: $XDG_DATA_HOME or ~/.local/share
    /// - macOS: ~/Library/Application Support
    /// - Windows: %LOCALAPPDATA%
    public func data_local() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's cache directory
    /// - Linux: $XDG_CACHE_HOME or ~/.cache
    /// - macOS: ~/Library/Caches
    /// - Windows: %LOCALAPPDATA%
    public func cache() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's downloads directory
    public func downloads() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's documents directory
    public func documents() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's desktop directory
    public func desktop() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's pictures directory
    public func pictures() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's music directory
    public func music() -> Option[PathBuf]
        caps: [io.process.env]

    /// User's videos directory
    public func videos() -> Option[PathBuf]
        caps: [io.process.env]

    /// System's runtime directory
    /// - Linux: $XDG_RUNTIME_DIR
    /// - macOS: None
    /// - Windows: None
    public func runtime() -> Option[PathBuf]
        caps: [io.process.env]
}
```

---

## Platform Information

```tml
/// Target operating system
public const OS: String = // "linux", "macos", "windows", etc.

/// Target architecture
public const ARCH: String = // "x86_64", "aarch64", "arm", etc.

/// Target pointer width in bits
public const POINTER_WIDTH: U32 = // 32 or 64

/// Target endianness
public const ENDIAN: String = // "little" or "big"

/// Target family
public const FAMILY: String = // "unix" or "windows"
```

### Runtime Checks

```tml
/// Returns the number of logical CPUs
public func num_cpus() -> U64
    caps: [io.process.env]

/// Returns the page size
public func page_size() -> U64
    caps: [io.process.env]

/// Returns the hostname
public func hostname() -> Result[String, IoError]
    caps: [io.process.env]

/// Returns the OS type
public func os_type() -> String
    caps: [io.process.env]
// "Linux", "Darwin", "Windows", etc.

/// Returns the OS release version
public func os_release() -> Result[String, IoError]
    caps: [io.process.env]
```

---

## Path Manipulation

### PATH variable

```tml
/// Joins paths with the platform path separator
public func join_paths[I](paths: I) -> Result[OsString, JoinPathsError]
    where I: Iterator[Item = impl AsRef[Path]]

/// Splits the PATH variable into paths
public func split_paths(path: &OsStr) -> SplitPaths

/// Iterator over PATH components
public type SplitPaths {
    // Internal
}

implement Iterator for SplitPaths {
    type Item = PathBuf

    func next(mut this) -> Option[PathBuf]
}

// Usage
let path = env.var("PATH").unwrap()
loop dir in env.split_paths(&path) {
    println!("{}", dir.display())
}
```

---

## Executable Search

```tml
/// Finds an executable in PATH
public func which(name: &String) -> Option[PathBuf]
    caps: [io.process.env]
{
    let path = var("PATH").ok()?
    loop dir in split_paths(&path) {
        let candidate = dir.join(name)
        if candidate.exists() and candidate.is_executable() then {
            return Some(candidate)
        }
    }
    return None
}

/// Finds all matching executables in PATH
public func which_all(name: &String) -> Vec[PathBuf]
    caps: [io.process.env]
```

---

## Examples

### Reading Configuration from Environment

```tml
import std.env

type Config {
    database_url: String,
    port: U16,
    debug: Bool,
    log_level: String,
}

func load_config() -> Result[Config, ConfigError]
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
import std.env.dirs
import std.fs

type AppDirs {
    config: PathBuf,
    data: PathBuf,
    cache: PathBuf,
}

func get_app_dirs(app_name: &String) -> Result[AppDirs, IoError]
    caps: [io.process.env, io.file]
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
    fs.create_dir_all(&config)?
    fs.create_dir_all(&data)?
    fs.create_dir_all(&cache)?

    Ok(AppDirs { config, data, cache })
}
```

### Platform-Specific Behavior

```tml
import std.env

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
import std.env

func with_env[T](vars: &[(String, String)], f: func() -> T) -> T
    caps: [io.process.env]
{
    // Save original values
    var original: Vec[(String, Option[String])] = Vec.new()
    loop (key, value) in vars.iter() {
        original.push((key.clone(), env.var_opt(key)))
        env.set_var(key, value)
    }

    // Run function
    let result = f()

    // Restore original values
    loop (key, original_value) in original.iter() {
        when original_value {
            Some(v) -> env.set_var(key, v),
            None -> env.remove_var(key),
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
