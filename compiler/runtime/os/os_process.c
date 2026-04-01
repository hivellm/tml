/**
 * TML Runtime - OS Process Module
 *
 * Subprocess management, signal handling, and anonymous pipes.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Use mem_alloc/mem_realloc/mem_free so the memory tracker can track string allocations
extern void* mem_alloc(int64_t);
extern void* mem_realloc(void*, int64_t);
extern void mem_free(void*);

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
// clang-format on
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// ============================================================================
// Subprocess Management
// ============================================================================

/**
 * Process handle structure (opaque to TML).
 * On Windows: uses CreateProcess with pipes.
 * On Unix: uses fork/exec with pipes.
 */
typedef struct {
#ifdef _WIN32
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    HANDLE hStdinWrite; // parent writes to child's stdin
    HANDLE hStdoutRead; // parent reads child's stdout
    HANDLE hStderrRead; // parent reads child's stderr
#else
    pid_t pid;
    int stdin_fd;  // parent writes to child's stdin (-1 if not piped)
    int stdout_fd; // parent reads child's stdout (-1 if not piped)
    int stderr_fd; // parent reads child's stderr (-1 if not piped)
#endif
    int exited;
    int exit_code;
} tml_process_t;

// Stdio modes: 0 = Inherit, 1 = Piped, 2 = Null
#define STDIO_INHERIT 0
#define STDIO_PIPED 1
#define STDIO_NULL 2

/**
 * Spawn a new process.
 *
 * @param program   Path to executable
 * @param args      Space-separated argument string (or empty)
 * @param cwd       Working directory (or empty for current)
 * @param stdout_mode  0=Inherit, 1=Piped, 2=Null
 * @param stderr_mode  0=Inherit, 1=Piped, 2=Null
 * @return opaque handle (cast to I64 in TML), or 0 on failure
 */
TML_EXPORT int64_t tml_process_spawn(const char* program, const char* args, const char* cwd,
                                     int32_t stdout_mode, int32_t stderr_mode) {
    tml_process_t* proc = (tml_process_t*)mem_alloc(sizeof(tml_process_t));
    if (!proc)
        return 0;
    memset(proc, 0, sizeof(tml_process_t));
    proc->exited = 0;
    proc->exit_code = -1;

#ifdef _WIN32
    proc->hStdinWrite = INVALID_HANDLE_VALUE;
    proc->hStdoutRead = INVALID_HANDLE_VALUE;
    proc->hStderrRead = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdoutWrite = INVALID_HANDLE_VALUE;
    HANDLE hStderrWrite = INVALID_HANDLE_VALUE;

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    // stdout pipe
    if (stdout_mode == STDIO_PIPED) {
        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            mem_free(proc);
            return 0;
        }
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        proc->hStdoutRead = hRead;
        hStdoutWrite = hWrite;
        si.hStdOutput = hWrite;
    } else if (stdout_mode == STDIO_NULL) {
        HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
        si.hStdOutput = hNull;
    } else {
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    // stderr pipe
    if (stderr_mode == STDIO_PIPED) {
        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            mem_free(proc);
            return 0;
        }
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        proc->hStderrRead = hRead;
        hStderrWrite = hWrite;
        si.hStdError = hWrite;
    } else if (stderr_mode == STDIO_NULL) {
        HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
        si.hStdError = hNull;
    } else {
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }

    // Build command line: "program args"
    size_t plen = strlen(program);
    size_t alen = args ? strlen(args) : 0;
    size_t cmdlen = plen + 1 + alen + 1;
    char* cmdline = (char*)mem_alloc((int64_t)cmdlen);
    if (!cmdline) {
        mem_free(proc);
        return 0;
    }
    if (alen > 0) {
        snprintf(cmdline, cmdlen, "%s %s", program, args);
    } else {
        snprintf(cmdline, cmdlen, "%s", program);
    }

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, (cwd && cwd[0]) ? cwd : NULL,
                             &si, &pi);
    mem_free(cmdline);

    // Close the write ends of pipes in the parent
    if (hStdoutWrite != INVALID_HANDLE_VALUE)
        CloseHandle(hStdoutWrite);
    if (hStderrWrite != INVALID_HANDLE_VALUE)
        CloseHandle(hStderrWrite);

    if (!ok) {
        if (proc->hStdoutRead != INVALID_HANDLE_VALUE)
            CloseHandle(proc->hStdoutRead);
        if (proc->hStderrRead != INVALID_HANDLE_VALUE)
            CloseHandle(proc->hStderrRead);
        mem_free(proc);
        return 0;
    }

    proc->hProcess = pi.hProcess;
    proc->hThread = pi.hThread;
    proc->dwProcessId = pi.dwProcessId;

#else
    // Unix: fork/exec
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (stdout_mode == STDIO_PIPED) {
        if (pipe(stdout_pipe) < 0) {
            mem_free(proc);
            return 0;
        }
    }
    if (stderr_mode == STDIO_PIPED) {
        if (pipe(stderr_pipe) < 0) {
            if (stdout_pipe[0] >= 0) {
                close(stdout_pipe[0]);
                close(stdout_pipe[1]);
            }
            mem_free(proc);
            return 0;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0) {
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        }
        mem_free(proc);
        return 0;
    }

    if (pid == 0) {
        // Child process
        if (stdout_mode == STDIO_PIPED) {
            close(stdout_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[1]);
        } else if (stdout_mode == STDIO_NULL) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        if (stderr_mode == STDIO_PIPED) {
            close(stderr_pipe[0]);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
        } else if (stderr_mode == STDIO_NULL) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        if (cwd && cwd[0])
            chdir(cwd);

        // Use /bin/sh -c "program args" for simplicity
        size_t plen2 = strlen(program);
        size_t alen2 = args ? strlen(args) : 0;
        size_t cmdlen2 = plen2 + 1 + alen2 + 1;
        char* cmd = (char*)malloc(cmdlen2);
        if (alen2 > 0) {
            snprintf(cmd, cmdlen2, "%s %s", program, args);
        } else {
            snprintf(cmd, cmdlen2, "%s", program);
        }
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127); // exec failed
    }

    // Parent process
    if (stdout_pipe[1] >= 0)
        close(stdout_pipe[1]);
    if (stderr_pipe[1] >= 0)
        close(stderr_pipe[1]);

    proc->pid = pid;
    proc->stdin_fd = -1;
    proc->stdout_fd = stdout_pipe[0];
    proc->stderr_fd = stderr_pipe[0];
#endif

    return (int64_t)(uintptr_t)proc;
}

/**
 * Wait for a process to complete and return its exit code.
 */
TML_EXPORT int32_t tml_process_wait(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc)
        return -1;
    if (proc->exited)
        return proc->exit_code;

#ifdef _WIN32
    WaitForSingleObject(proc->hProcess, INFINITE);
    DWORD code;
    GetExitCodeProcess(proc->hProcess, &code);
    proc->exit_code = (int32_t)code;
    proc->exited = 1;
#else
    int status;
    waitpid(proc->pid, &status, 0);
    if (WIFEXITED(status)) {
        proc->exit_code = WEXITSTATUS(status);
    } else {
        proc->exit_code = -1;
    }
    proc->exited = 1;
#endif
    return proc->exit_code;
}

/**
 * Kill a running process. Returns 1 on success, 0 on failure.
 */
TML_EXPORT int32_t tml_process_kill(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc || proc->exited)
        return 0;

#ifdef _WIN32
    return TerminateProcess(proc->hProcess, 1) ? 1 : 0;
#else
    return kill(proc->pid, SIGKILL) == 0 ? 1 : 0;
#endif
}

/**
 * Get the process ID.
 */
TML_EXPORT int32_t tml_process_id(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc)
        return -1;
#ifdef _WIN32
    return (int32_t)proc->dwProcessId;
#else
    return (int32_t)proc->pid;
#endif
}

/**
 * Read all stdout from a piped process. Returns heap-allocated string.
 */
TML_EXPORT const char* tml_process_read_stdout(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc)
        return "";

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)mem_alloc((int64_t)cap);
    if (!buf)
        return "";

#ifdef _WIN32
    if (proc->hStdoutRead == INVALID_HANDLE_VALUE) {
        mem_free(buf);
        return "";
    }
    DWORD nread;
    char tmp[1024];
    while (ReadFile(proc->hStdoutRead, tmp, sizeof(tmp), &nread, NULL) && nread > 0) {
        if (len + nread + 1 > cap) {
            cap = (len + nread + 1) * 2;
            char* newbuf = (char*)mem_realloc(buf, (int64_t)cap);
            if (!newbuf) {
                mem_free(buf);
                return "";
            }
            buf = newbuf;
        }
        memcpy(buf + len, tmp, nread);
        len += nread;
    }
#else
    if (proc->stdout_fd < 0) {
        mem_free(buf);
        return "";
    }
    char tmp[1024];
    ssize_t nread;
    while ((nread = read(proc->stdout_fd, tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)nread + 1 > cap) {
            cap = (len + (size_t)nread + 1) * 2;
            char* newbuf = (char*)mem_realloc(buf, (int64_t)cap);
            if (!newbuf) {
                mem_free(buf);
                return "";
            }
            buf = newbuf;
        }
        memcpy(buf + len, tmp, (size_t)nread);
        len += (size_t)nread;
    }
#endif

    buf[len] = '\0';
    const char* result = _strdup(buf);
    mem_free(buf);
    return result ? result : "";
}

/**
 * Read all stderr from a piped process. Returns heap-allocated string.
 */
TML_EXPORT const char* tml_process_read_stderr(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc)
        return "";

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)mem_alloc((int64_t)cap);
    if (!buf)
        return "";

#ifdef _WIN32
    if (proc->hStderrRead == INVALID_HANDLE_VALUE) {
        mem_free(buf);
        return "";
    }
    DWORD nread;
    char tmp[1024];
    while (ReadFile(proc->hStderrRead, tmp, sizeof(tmp), &nread, NULL) && nread > 0) {
        if (len + nread + 1 > cap) {
            cap = (len + nread + 1) * 2;
            char* newbuf = (char*)mem_realloc(buf, (int64_t)cap);
            if (!newbuf) {
                mem_free(buf);
                return "";
            }
            buf = newbuf;
        }
        memcpy(buf + len, tmp, nread);
        len += nread;
    }
#else
    if (proc->stderr_fd < 0) {
        mem_free(buf);
        return "";
    }
    char tmp[1024];
    ssize_t nread;
    while ((nread = read(proc->stderr_fd, tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)nread + 1 > cap) {
            cap = (len + (size_t)nread + 1) * 2;
            char* newbuf = (char*)mem_realloc(buf, (int64_t)cap);
            if (!newbuf) {
                mem_free(buf);
                return "";
            }
            buf = newbuf;
        }
        memcpy(buf + len, tmp, (size_t)nread);
        len += (size_t)nread;
    }
#endif

    buf[len] = '\0';
    const char* result = _strdup(buf);
    mem_free(buf);
    return result ? result : "";
}

/**
 * Destroy a process handle and close all associated handles/fds.
 */
TML_EXPORT void tml_process_destroy(int64_t handle) {
    tml_process_t* proc = (tml_process_t*)(uintptr_t)handle;
    if (!proc)
        return;

#ifdef _WIN32
    if (proc->hStdinWrite != INVALID_HANDLE_VALUE)
        CloseHandle(proc->hStdinWrite);
    if (proc->hStdoutRead != INVALID_HANDLE_VALUE)
        CloseHandle(proc->hStdoutRead);
    if (proc->hStderrRead != INVALID_HANDLE_VALUE)
        CloseHandle(proc->hStderrRead);
    if (proc->hProcess)
        CloseHandle(proc->hProcess);
    if (proc->hThread)
        CloseHandle(proc->hThread);
#else
    if (proc->stdin_fd >= 0)
        close(proc->stdin_fd);
    if (proc->stdout_fd >= 0)
        close(proc->stdout_fd);
    if (proc->stderr_fd >= 0)
        close(proc->stderr_fd);
#endif

    mem_free(proc);
}

// ============================================================================
// Signal Handling
// ============================================================================

// Global volatile flag array for signal notifications (one per signal number)
// TML code polls these flags. Signals 0-31 supported.
static volatile int32_t tml_signal_flags[32] = {0};

#ifdef _WIN32
static BOOL WINAPI tml_console_ctrl_handler(DWORD type) {
    switch (type) {
    case CTRL_C_EVENT:
        tml_signal_flags[2] = 1;
        return TRUE; // SIGINT
    case CTRL_BREAK_EVENT:
        tml_signal_flags[15] = 1;
        return TRUE; // SIGTERM
    case CTRL_CLOSE_EVENT:
        tml_signal_flags[15] = 1;
        return TRUE; // SIGTERM
    default:
        return FALSE;
    }
}
static int tml_ctrl_handler_installed = 0;
#else
static void tml_signal_handler(int sig) {
    if (sig >= 0 && sig < 32) {
        tml_signal_flags[sig] = 1;
    }
}
#endif

/**
 * Register interest in a signal. Sets up a handler that sets a flag.
 * @param signum Signal number (2=SIGINT, 15=SIGTERM, etc.)
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_signal_register(int32_t signum) {
    if (signum < 0 || signum >= 32)
        return 0;
    tml_signal_flags[signum] = 0; // Clear any pending

#ifdef _WIN32
    if (!tml_ctrl_handler_installed) {
        SetConsoleCtrlHandler(tml_console_ctrl_handler, TRUE);
        tml_ctrl_handler_installed = 1;
    }
    return 1;
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tml_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return sigaction(signum, &sa, NULL) == 0 ? 1 : 0;
#endif
}

/**
 * Reset a signal to its default behavior.
 */
TML_EXPORT int32_t tml_signal_reset(int32_t signum) {
    if (signum < 0 || signum >= 32)
        return 0;
    tml_signal_flags[signum] = 0;

#ifdef _WIN32
    return 1; // Windows ctrl handler stays installed
#else
    signal(signum, SIG_DFL);
    return 1;
#endif
}

/**
 * Ignore a signal.
 */
TML_EXPORT int32_t tml_signal_ignore(int32_t signum) {
    if (signum < 0 || signum >= 32)
        return 0;
    tml_signal_flags[signum] = 0;

#ifdef _WIN32
    return 1;
#else
    signal(signum, SIG_IGN);
    return 1;
#endif
}

/**
 * Check if a signal flag is set. Returns 1 if set (and clears it), 0 otherwise.
 */
TML_EXPORT int32_t tml_signal_check(int32_t signum) {
    if (signum < 0 || signum >= 32)
        return 0;
    if (tml_signal_flags[signum]) {
        tml_signal_flags[signum] = 0;
        return 1;
    }
    return 0;
}

/**
 * Send a signal to the current process (raise).
 */
TML_EXPORT int32_t tml_signal_raise(int32_t signum) {
#ifdef _WIN32
    // On Windows, we can only raise SIGINT/SIGTERM via GenerateConsoleCtrlEvent
    if (signum == 2) {
        tml_signal_flags[2] = 1;
        return 1;
    }
    return 0;
#else
    return raise(signum) == 0 ? 1 : 0;
#endif
}

// ============================================================================
// Pipes
// ============================================================================

/**
 * Create an anonymous pipe. Returns two I64 values via out parameters.
 * @param read_fd  Output: read end of pipe
 * @param write_fd Output: write end of pipe
 * @return 1 on success, 0 on failure
 */
TML_EXPORT int32_t tml_pipe_create(int64_t* read_fd, int64_t* write_fd) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = NULL;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return 0;
    *read_fd = (int64_t)(uintptr_t)hRead;
    *write_fd = (int64_t)(uintptr_t)hWrite;
    return 1;
#else
    int fds[2];
    if (pipe(fds) < 0)
        return 0;
    *read_fd = (int64_t)fds[0];
    *write_fd = (int64_t)fds[1];
    return 1;
#endif
}

/**
 * Close a pipe end.
 */
TML_EXPORT void tml_pipe_close(int64_t fd) {
#ifdef _WIN32
    CloseHandle((HANDLE)(uintptr_t)fd);
#else
    close((int)fd);
#endif
}
