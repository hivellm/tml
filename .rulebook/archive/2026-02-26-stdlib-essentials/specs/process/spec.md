# Spec: Process Module

## Overview

Subprocess spawning and management.

## Types

### Command

Builder for configuring and spawning processes.

```tml
pub type Command {
    program: Str,
    args: Vec[Str],
    envs: HashMap[Str, Str],
    current_dir: Maybe[PathBuf],
    stdin: Stdio,
    stdout: Stdio,
    stderr: Stdio,
}

pub type Stdio = Inherit | Piped | Null

extend Command {
    /// Create new command for program
    pub func new(program: Str) -> Command

    /// Add single argument
    pub func arg(this, arg: Str) -> Command

    /// Add multiple arguments
    pub func args[I: IntoIterator[Item = Str]](this, args: I) -> Command

    /// Set environment variable
    pub func env(this, key: Str, val: Str) -> Command

    /// Set multiple environment variables
    pub func envs[I: IntoIterator[Item = (Str, Str)]](this, envs: I) -> Command

    /// Clear all environment variables
    pub func env_clear(this) -> Command

    /// Set working directory
    pub func current_dir(this, dir: impl AsRef[Path]) -> Command

    /// Configure stdin
    pub func stdin(this, cfg: Stdio) -> Command

    /// Configure stdout
    pub func stdout(this, cfg: Stdio) -> Command

    /// Configure stderr
    pub func stderr(this, cfg: Stdio) -> Command

    /// Spawn process and return handle
    pub func spawn(this) -> Outcome[Child, IoError]

    /// Run and wait for output
    pub func output(this) -> Outcome[Output, IoError]

    /// Run and wait for status
    pub func status(this) -> Outcome[ExitStatus, IoError]
}
```

### Child

Handle to a spawned process.

```tml
pub type Child {
    handle: RawHandle,
    stdin: Maybe[ChildStdin],
    stdout: Maybe[ChildStdout],
    stderr: Maybe[ChildStderr],
}

extend Child {
    /// Get stdin pipe (if Piped)
    pub func stdin(this) -> Maybe[mut ref ChildStdin]

    /// Get stdout pipe (if Piped)
    pub func stdout(this) -> Maybe[mut ref ChildStdout]

    /// Get stderr pipe (if Piped)
    pub func stderr(this) -> Maybe[mut ref ChildStderr]

    /// Take stdin pipe
    pub func take_stdin(this) -> Maybe[ChildStdin]

    /// Wait for process to exit
    pub func wait(this) -> Outcome[ExitStatus, IoError]

    /// Wait and collect output
    pub func wait_with_output(this) -> Outcome[Output, IoError]

    /// Try to get exit status without blocking
    pub func try_wait(this) -> Outcome[Maybe[ExitStatus], IoError]

    /// Kill the process
    pub func kill(this) -> Outcome[Unit, IoError]

    /// Get process ID
    pub func id(this) -> U32
}

pub type ChildStdin { ... }
pub type ChildStdout { ... }
pub type ChildStderr { ... }

extend ChildStdin with Write { ... }
extend ChildStdout with Read { ... }
extend ChildStderr with Read { ... }
```

### Output

Collected output from a process.

```tml
pub type Output {
    status: ExitStatus,
    stdout: Vec[U8],
    stderr: Vec[U8],
}
```

### ExitStatus

Process exit status.

```tml
pub type ExitStatus {
    code: Maybe[I32],
}

extend ExitStatus {
    pub func success(this) -> Bool
    pub func code(this) -> Maybe[I32]
}
```

## Platform Implementation

### Windows

```c
// spawn()
CreateProcessW(program, args, ..., &process_info)

// wait()
WaitForSingleObject(handle, INFINITE)
GetExitCodeProcess(handle, &code)

// kill()
TerminateProcess(handle, 1)
```

### Unix

```c
// spawn()
fork()
// child: execve(program, args, envp)

// wait()
waitpid(pid, &status, 0)

// kill()
kill(pid, SIGKILL)
```

## Example

```tml
use std::process::Command

func main() -> Outcome[Unit, Error] {
    // Simple execution
    let status = Command::new("ls")
        .arg("-la")
        .status()!

    println("Exit code: {status.code()}")

    // Capture output
    let output = Command::new("echo")
        .arg("Hello")
        .output()!

    println("stdout: {String::from_utf8(output.stdout)!}")

    // Piped I/O
    let mut child = Command::new("cat")
        .stdin(Stdio::Piped)
        .stdout(Stdio::Piped)
        .spawn()!

    child.stdin().unwrap().write_all(b"Hello")!
    child.take_stdin()  // Close stdin

    let output = child.wait_with_output()!
    println("Output: {String::from_utf8(output.stdout)!}")

    Ok(unit)
}
```
