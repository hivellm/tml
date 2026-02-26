# Spec: Environment Module

## Overview

Access to environment variables and process environment.

## Functions

```tml
mod std::env

/// Get environment variable by name
pub func var(name: Str) -> Maybe[Str]

/// Set environment variable
pub func set_var(name: Str, value: Str)

/// Remove environment variable
pub func remove_var(name: Str)

/// Iterate over all environment variables
pub func vars() -> Vars

pub type Vars { ... }

extend Vars with Iterator {
    type Item = (Str, Str)
}

/// Get current working directory
pub func current_dir() -> Outcome[PathBuf, IoError]

/// Set current working directory
pub func set_current_dir(path: impl AsRef[Path]) -> Outcome[Unit, IoError]

/// Get user's home directory
pub func home_dir() -> Maybe[PathBuf]

/// Get system temp directory
pub func temp_dir() -> PathBuf

/// Get executable path
pub func current_exe() -> Outcome[PathBuf, IoError]
```

## Platform Implementation

### Windows

```c
// var()
GetEnvironmentVariableW(name, buffer, size)

// set_var()
SetEnvironmentVariableW(name, value)

// current_dir()
GetCurrentDirectoryW(size, buffer)

// home_dir()
SHGetKnownFolderPath(FOLDERID_Profile, ...)

// temp_dir()
GetTempPathW(size, buffer)
```

### Unix

```c
// var()
getenv(name)

// set_var()
setenv(name, value, 1)

// current_dir()
getcwd(buffer, size)

// home_dir()
getenv("HOME") or getpwuid(getuid())->pw_dir

// temp_dir()
getenv("TMPDIR") or "/tmp"
```

## Example

```tml
use std::env

func main() {
    // Read environment variable
    when env::var("HOME") {
        Just(home) => println("Home: {home}"),
        Nothing => println("HOME not set"),
    }

    // Set environment variable
    env::set_var("MY_VAR", "my_value")

    // Iterate all variables
    for (key, value) in env::vars() {
        println("{key}={value}")
    }

    // Working directory
    let cwd = env::current_dir()!
    println("CWD: {cwd}")
}
```
