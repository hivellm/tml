# TML Standard Library: Command Line Arguments

> `std.args` — Command-line argument parsing.

## Overview

The args package provides a declarative, type-safe way to parse command-line arguments. It supports subcommands, flags, options, positional arguments, and automatic help generation.

**Capability**: `io.process.env`

## Import

```tml
import std.args
import std.args.{Command, Arg, parse}
```

---

## Quick Start

```tml
import std.args.{Command, Arg, parse}

/// Command-line arguments
type Args {
    /// Input file path
    @Arg(short = 'i', long = "input", required = true)
    input: String,

    /// Output file path
    @Arg(short = 'o', long = "output", default = "output.txt")
    output: String,

    /// Verbose mode
    @Arg(short = 'v', long = "verbose")
    verbose: Bool,

    /// Number of iterations
    @Arg(short = 'n', long = "count", default = 1)
    count: U32,
}

func main()
    caps: [io.process.env]
{
    let args: Args = parse().unwrap_or_else(do(e) {
        eprintln(e.to_string())
        process.exit(1)
    })

    if args.verbose then {
        println("Input: " + args.input)
        println("Output: " + args.output)
        println("Count: " + args.count.to_string())
    }
}
```

---

## Arg Attribute

```tml
/// Argument configuration attribute
public type Arg {
    /// Short flag (-v)
    short: Maybe[Char],

    /// Long flag (--verbose)
    long: Maybe[String],

    /// Help text
    help: Maybe[String],

    /// Whether the argument is required
    required: Bool,

    /// Default value
    default: Maybe[String],

    /// Environment variable to read from
    env: Maybe[String],

    /// Value name in help (e.g., <FILE>)
    value_name: Maybe[String],

    /// Hide from help
    hidden: Bool,

    /// Possible values
    possible_values: Maybe[Vec[String]],

    /// Allow multiple occurrences
    multiple: Bool,

    /// Global flag (applies to all subcommands)
    global: Bool,
}
```

### Attribute Examples

```tml
type Args {
    /// Required with short and long flag
    @Arg(short = 'c', long = "config", required = true, help = "Config file path")
    config: String,

    /// Optional with default
    @Arg(long = "port", default = 8080, help = "Server port")
    port: U16,

    /// Boolean flag (no value needed)
    @Arg(short = 'v', long = "verbose", help = "Enable verbose output")
    verbose: Bool,

    /// Read from environment
    @Arg(long = "api-key", env = "API_KEY", help = "API key")
    api_key: Maybe[String],

    /// Multiple values
    @Arg(short = 'f', long = "file", multiple = true, help = "Input files")
    files: Vec[String],

    /// Restricted values
    @Arg(long = "format", possible_values = ["json", "yaml", "toml"])
    format: String,

    /// Positional argument (no short/long)
    @Arg(value_name = "COMMAND", help = "Command to run")
    command: String,
}
```

---

## Command Attribute

```tml
/// Command configuration attribute
public type Command {
    /// Command name
    name: Maybe[String],

    /// Version string
    version: Maybe[String],

    /// Author information
    author: Maybe[String],

    /// About/description
    about: Maybe[String],

    /// Long about (detailed description)
    long_about: Maybe[String],

    /// After help text
    after_help: Maybe[String],

    /// Disable automatic help flag
    disable_help: Bool,

    /// Disable automatic version flag
    disable_version: Bool,
}
```

---

## Subcommands

```tml
@Command(name = "myapp", version = "1.0.0", about = "My application")
type Cli {
    /// Global verbose flag
    @Arg(short = 'v', long = "verbose", global = true)
    verbose: Bool,

    /// Subcommand
    command: SubCommand,
}

type SubCommand =
    | Run(RunArgs)
    | Build(BuildArgs)
    | Test(TestArgs)

@Command(about = "Run the application")
type RunArgs {
    @Arg(long = "release", help = "Run in release mode")
    release: Bool,

    @Arg(value_name = "ARGS", multiple = true)
    args: Vec[String],
}

@Command(about = "Build the project")
type BuildArgs {
    @Arg(short = 'j', long = "jobs", default = 4)
    jobs: U32,

    @Arg(long = "target")
    target: Maybe[String],
}

@Command(about = "Run tests")
type TestArgs {
    @Arg(long = "filter", help = "Test name filter")
    filter: Maybe[String],

    @Arg(long = "no-capture", help = "Don't capture stdout")
    no_capture: Bool,
}
```

---

## Parsing Functions

```tml
/// Parses command-line arguments into a type
public func parse[T: FromArgs]() -> Outcome[T, ParseError]
    caps: [io.process.env]
{
    let args = env.args().collect()
    parse_from(args)
}

/// Parses from a custom argument list
public func parse_from[T: FromArgs](args: Vec[String]) -> Outcome[T, ParseError]

/// Tries to parse, returns None on error
public func try_parse[T: FromArgs]() -> Maybe[T]
    caps: [io.process.env]

/// Trait for types that can be parsed from arguments
public behavior FromArgs {
    func from_args(args: ref ArgMatches) -> Outcome[This, ParseError]
}
```

---

## ArgMatches

```tml
/// Parsed argument matches
public type ArgMatches {
    values: HashMap[String, Vec[String]],
    flags: HashSet[String],
    subcommand: Maybe[(String, Box[ArgMatches])],
}

extend ArgMatches {
    /// Gets a single value
    public func get_one[T: FromStr](this, id: ref String) -> Maybe[T]

    /// Gets multiple values
    public func get_many[T: FromStr](this, id: ref String) -> Maybe[Vec[T]]

    /// Returns true if flag is present
    public func get_flag(this, id: ref String) -> Bool

    /// Returns the number of occurrences
    public func get_count(this, id: ref String) -> U64

    /// Gets the subcommand name and matches
    public func subcommand(this) -> Maybe[(ref String, ref ArgMatches)]

    /// Returns true if a subcommand was used
    public func subcommand_name(this) -> Maybe[ref String]
}
```

---

## Error Handling

```tml
/// Parse error
public type ParseError =
    | MissingRequired(String)
    | InvalidValue { arg: String, value: String, expected: String }
    | UnknownArgument(String)
    | UnexpectedValue(String)
    | TooManyValues(String)
    | ConflictingArguments(String, String)
    | MissingSubcommand
    | InvalidSubcommand(String)
    | HelpRequested
    | VersionRequested

extend ParseError {
    /// Returns user-friendly error message
    public func to_string(this) -> String {
        when this {
            MissingRequired(arg) ->
                "error: required argument '" + arg + "' not provided",
            InvalidValue { arg, value, expected } ->
                "error: invalid value '" + value + "' for '" + arg + "': expected " + expected,
            UnknownArgument(arg) ->
                "error: unknown argument '" + arg + "'",
            UnexpectedValue(arg) ->
                "error: unexpected value for '" + arg + "'",
            TooManyValues(arg) ->
                "error: too many values for '" + arg + "'",
            ConflictingArguments(a, b) ->
                "error: '" + a + "' cannot be used with '" + b + "'",
            MissingSubcommand ->
                "error: subcommand required",
            InvalidSubcommand(cmd) ->
                "error: unknown subcommand '" + cmd + "'",
            HelpRequested -> "",
            VersionRequested -> "",
        }
    }

    /// Prints error and exits
    public func exit(this) -> ! {
        when this {
            HelpRequested -> {
                // Help already printed
                process.exit(0)
            },
            VersionRequested -> {
                // Version already printed
                process.exit(0)
            },
            _ -> {
                eprintln(this.to_string())
                process.exit(1)
            },
        }
    }
}
```

---

## Help Generation

Automatic help generation based on attributes:

```
myapp 1.0.0
Author Name <author@example.com>

A description of the application

USAGE:
    myapp [OPTIONS] <COMMAND>

OPTIONS:
    -c, --config <FILE>     Config file path [required]
    -v, --verbose           Enable verbose output
    -h, --help              Print help information
    -V, --version           Print version information

COMMANDS:
    run       Run the application
    build     Build the project
    test      Run tests
    help      Print this message or the help of a subcommand
```

### Custom Help

```tml
@Command(
    name = "myapp",
    about = "Short description",
    long_about = "
A longer, more detailed description of the application.

This can span multiple lines and include examples:

    $ myapp run --release
    $ myapp build -j 8
",
    after_help = "
EXAMPLES:
    Run with config:
        myapp -c config.toml run

    Build for release:
        myapp build --release
"
)
type Cli {
    // ...
}
```

---

## Value Parsing

Built-in parsers for common types:

```tml
implement FromStr for types:
    - String
    - Bool (true, false, yes, no, 1, 0)
    - I8, I16, I32, I64, I128
    - U8, U16, U32, U64, U128
    - F32, F64
    - PathBuf
    - IpAddr, Ipv4Addr, Ipv6Addr
    - SocketAddr
    - Duration (1s, 100ms, 5m, 2h)
```

### Custom Value Parser

```tml
type LogLevel = Trace | Debug | Info | Warn | Error

implement FromStr for LogLevel {
    type Err = String

    func from_str(s: ref String) -> Outcome[LogLevel, String] {
        when s.to_lowercase().as_str() {
            "trace" -> Ok(Trace),
            "debug" -> Ok(Debug),
            "info" -> Ok(Info),
            "warn" -> Ok(Warn),
            "error" -> Ok(Error),
            _ -> Err("invalid log level: " + s),
        }
    }
}

type Args {
    @Arg(long = "log-level", default = "info")
    log_level: LogLevel,
}
```

---

## Validation

```tml
/// Validator trait
public behavior Validator {
    func validate(value: ref String) -> Outcome[Unit, String]
}

/// File exists validator
public type FileExists {}

implement Validator for FileExists {
    func validate(value: ref String) -> Outcome[Unit, String] {
        if Path.new(value).exists() then {
            Ok(())
        } else {
            Err("file not found: " + value)
        }
    }
}

/// Range validator
public type InRange[T: Ord] {
    min: T,
    max: T,
}

implement Validator for InRange[T] where T: Ord + FromStr {
    func validate(value: ref String) -> Outcome[Unit, String] {
        let v: T = value.parse().map_err(|e| e.to_string())?
        if v >= this.min and v <= this.max then {
            Ok(())
        } else {
            Err("value must be between " + this.min.to_string() + " and " + this.max.to_string())
        }
    }
}

// Usage
type Args {
    @Arg(long = "config", validator = FileExists)
    config: String,

    @Arg(long = "port", validator = InRange { min: 1, max: 65535 })
    port: U16,
}
```

---

## Examples

### Git-like CLI

```tml
@Command(name = "git", about = "Version control system")
type GitCli {
    @Arg(short = 'C', long = "dir", global = true, help = "Run as if started in <path>")
    dir: Maybe[String],

    command: GitCommand,
}

type GitCommand =
    | Clone(CloneArgs)
    | Add(AddArgs)
    | Commit(CommitArgs)
    | Push(PushArgs)
    | Pull(PullArgs)

@Command(about = "Clone a repository")
type CloneArgs {
    @Arg(value_name = "URL", required = true)]
    url: String,

    @Arg(value_name = "DIR")]
    directory: Maybe[String],

    @Arg(long = "depth", help = "Create a shallow clone")
    depth: Maybe[U32],

    @Arg(short = 'b', long = "branch", help = "Checkout <branch>")
    branch: Maybe[String],
}

@Command(about = "Add files to staging")
type AddArgs {
    @Arg(value_name = "PATH", multiple = true, required = true)
    paths: Vec[String],

    @Arg(short = 'A', long = "all", help = "Add all changes")
    all: Bool,

    @Arg(short = 'p', long = "patch", help = "Interactively select hunks")
    patch: Bool,
}

@Command(about = "Record changes")
type CommitArgs {
    @Arg(short = 'm', long = "message", required = true)
    message: String,

    @Arg(long = "amend", help = "Amend previous commit")
    amend: Bool,

    @Arg(short = 'a', long = "all", help = "Commit all changed files")
    all: Bool,
}

func main()
    caps: [io.process.env]
{
    let cli: GitCli = parse().unwrap_or_else(|e| e.exit())

    when cli.command {
        Clone(args) -> git_clone(args),
        Add(args) -> git_add(args),
        Commit(args) -> git_commit(args),
        Push(args) -> git_push(args),
        Pull(args) -> git_pull(args),
    }
}
```

### Server Configuration

```tml
@Command(name = "server", version = "1.0.0")
type ServerArgs {
    @Arg(short = 'h', long = "host", default = "127.0.0.1", env = "SERVER_HOST")
    host: String,

    @Arg(short = 'p', long = "port", default = 8080, env = "SERVER_PORT")
    port: U16,

    @Arg(long = "workers", default = 4, env = "SERVER_WORKERS")
    workers: U32,

    @Arg(long = "log-level", default = "info", env = "LOG_LEVEL",
         possible_values = ["trace", "debug", "info", "warn", "error"])
    log_level: String,

    @Arg(long = "config", short = 'c', help = "Config file path")
    config: Maybe[String],

    @Arg(long = "daemon", short = 'd', help = "Run as daemon")
    daemon: Bool,
}

func main()
    caps: [io.process.env]
{
    let args: ServerArgs = parse().unwrap_or_else(|e| e.exit())

    let config = when args.config {
        Just(path) -> Config.load(ref path).unwrap(),
        Nothing -> Config.default(),
    }

    let server = Server.new()
        .bind(&args.host, args.port)
        .workers(args.workers)
        .build()

    if args.daemon then {
        server.daemonize()
    }

    server.run()
}
```

---

## See Also

- [std.env](./20-ENV.md) — Environment variables
- [std.fs](./01-FS.md) — File paths
- [std.process](./21-PROCESS.md) — Process control
