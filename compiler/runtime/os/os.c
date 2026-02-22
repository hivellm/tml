/**
 * TML Runtime - OS Module
 *
 * Platform-specific operating system information functions.
 * Modeled after the Node.js os module API.
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
#include <lmcons.h>  /* UNLEN - must come after windows.h */
// clang-format on
#include <process.h> /* _getpid */
#include <userenv.h> /* GetUserProfileDirectoryA */
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#endif
#endif

/* ========================================================================== */
/* Architecture & Platform                                                     */
/* ========================================================================== */

TML_EXPORT const char* tml_os_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "ia32";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#elif defined(__mips__)
    return "mips";
#elif defined(__PPC64__) || defined(__ppc64__)
    return "ppc64";
#elif defined(__s390x__)
    return "s390x";
#elif defined(__loongarch64)
    return "loong64";
#else
    return "unknown";
#endif
}

TML_EXPORT const char* tml_os_platform(void) {
#if defined(_WIN32)
    return "win32";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
#if defined(__ANDROID__)
    return "android";
#else
    return "linux";
#endif
#elif defined(__FreeBSD__)
    return "freebsd";
#elif defined(__OpenBSD__)
    return "openbsd";
#elif defined(__sun)
    return "sunos";
#elif defined(_AIX)
    return "aix";
#else
    return "unknown";
#endif
}

TML_EXPORT const char* tml_os_type(void) {
#ifdef _WIN32
    return "Windows_NT";
#elif defined(__APPLE__)
    return "Darwin";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#else
    static struct utsname info;
    if (uname(&info) == 0)
        return info.sysname;
    return "Unknown";
#endif
}

TML_EXPORT const char* tml_os_machine(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i686";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#elif defined(__mips64)
    return "mips64";
#elif defined(__mips__)
    return "mips";
#elif defined(__PPC64__) || defined(__ppc64__)
    return "ppc64";
#elif defined(__s390x__)
    return "s390x";
#else
    return "unknown";
#endif
}

/* ========================================================================== */
/* OS Release & Version                                                        */
/* ========================================================================== */

TML_EXPORT const char* tml_os_release(void) {
    static char buf[256] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    /* Use RtlGetVersion via ntdll for accurate version on Win10+ */
    typedef LONG(WINAPI * RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        RtlGetVersionFunc rtl_get_version =
            (RtlGetVersionFunc)GetProcAddress(ntdll, "RtlGetVersion");
        if (rtl_get_version) {
            RTL_OSVERSIONINFOW osvi;
            memset(&osvi, 0, sizeof(osvi));
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (rtl_get_version(&osvi) == 0) {
                snprintf(buf, sizeof(buf), "%lu.%lu.%lu", (unsigned long)osvi.dwMajorVersion,
                         (unsigned long)osvi.dwMinorVersion, (unsigned long)osvi.dwBuildNumber);
                return buf;
            }
        }
    }
    snprintf(buf, sizeof(buf), "unknown");
#else
    struct utsname info;
    if (uname(&info) == 0) {
        snprintf(buf, sizeof(buf), "%s", info.release);
    } else {
        snprintf(buf, sizeof(buf), "unknown");
    }
#endif
    return buf;
}

TML_EXPORT const char* tml_os_version(void) {
    static char buf[512] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    typedef LONG(WINAPI * RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        RtlGetVersionFunc rtl_get_version =
            (RtlGetVersionFunc)GetProcAddress(ntdll, "RtlGetVersion");
        if (rtl_get_version) {
            RTL_OSVERSIONINFOW osvi;
            memset(&osvi, 0, sizeof(osvi));
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (rtl_get_version(&osvi) == 0) {
                snprintf(buf, sizeof(buf), "Windows NT %lu.%lu; Build %lu",
                         (unsigned long)osvi.dwMajorVersion, (unsigned long)osvi.dwMinorVersion,
                         (unsigned long)osvi.dwBuildNumber);
                return buf;
            }
        }
    }
    snprintf(buf, sizeof(buf), "Windows");
#else
    struct utsname info;
    if (uname(&info) == 0) {
        snprintf(buf, sizeof(buf), "%s %s %s", info.sysname, info.release, info.version);
    } else {
        snprintf(buf, sizeof(buf), "unknown");
    }
#endif
    return buf;
}

/* ========================================================================== */
/* Hostname                                                                    */
/* ========================================================================== */

TML_EXPORT const char* tml_os_hostname(void) {
    static char buf[256] = {0};

#ifdef _WIN32
    DWORD size = sizeof(buf);
    if (!GetComputerNameA(buf, &size)) {
        snprintf(buf, sizeof(buf), "unknown");
    }
#else
    if (gethostname(buf, sizeof(buf)) != 0) {
        snprintf(buf, sizeof(buf), "unknown");
    }
#endif
    return buf;
}

/* ========================================================================== */
/* Home Directory                                                              */
/* ========================================================================== */

TML_EXPORT const char* tml_os_homedir(void) {
    static char buf[1024] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    /* Try USERPROFILE first */
    const char* profile = getenv("USERPROFILE");
    if (profile && profile[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s", profile);
        return buf;
    }
    /* Fallback to HOMEDRIVE + HOMEPATH */
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) {
        snprintf(buf, sizeof(buf), "%s%s", drive, path);
        return buf;
    }
    buf[0] = '\0';
#else
    const char* home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s", home);
        return buf;
    }
    /* Fallback to passwd entry */
    struct passwd pw;
    struct passwd* result = NULL;
    char pwbuf[4096];
    if (getpwuid_r(getuid(), &pw, pwbuf, sizeof(pwbuf), &result) == 0 && result) {
        snprintf(buf, sizeof(buf), "%s", pw.pw_dir);
        return buf;
    }
    buf[0] = '\0';
#endif
    return buf;
}

/* ========================================================================== */
/* Temp Directory                                                              */
/* ========================================================================== */

TML_EXPORT const char* tml_os_tmpdir(void) {
    static char buf[1024] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    DWORD len = GetTempPathA((DWORD)sizeof(buf), buf);
    if (len > 0 && len < sizeof(buf)) {
        /* Remove trailing backslash if present (unless root) */
        if (len > 1 && buf[len - 1] == '\\') {
            buf[len - 1] = '\0';
        }
        return buf;
    }
    snprintf(buf, sizeof(buf), "C:\\Windows\\Temp");
#else
    const char* dirs[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR", NULL};
    for (int i = 0; dirs[i]; i++) {
        const char* val = getenv(dirs[i]);
        if (val && val[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s", val);
            return buf;
        }
    }
    snprintf(buf, sizeof(buf), "/tmp");
#endif
    return buf;
}

/* ========================================================================== */
/* Memory Information                                                          */
/* ========================================================================== */

TML_EXPORT uint64_t tml_os_totalmem(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return (uint64_t)status.ullTotalPhys;
    }
    return 0;
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (uint64_t)si.totalram * (uint64_t)si.mem_unit;
    }
    return 0;
#elif defined(__APPLE__)
    int64_t mem = 0;
    size_t len = sizeof(mem);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &mem, &len, NULL, 0) == 0) {
        return (uint64_t)mem;
    }
    return 0;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return (uint64_t)pages * (uint64_t)page_size;
    }
    return 0;
#endif
}

TML_EXPORT uint64_t tml_os_freemem(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return (uint64_t)status.ullAvailPhys;
    }
    return 0;
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (uint64_t)si.freeram * (uint64_t)si.mem_unit;
    }
    return 0;
#elif defined(__APPLE__)
    mach_port_t host = mach_host_self();
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_page_size(host, &page_size);
    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        return (uint64_t)vm_stat.free_count * (uint64_t)page_size;
    }
    return 0;
#else
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return (uint64_t)pages * (uint64_t)page_size;
    }
    return 0;
#endif
}

/* ========================================================================== */
/* Uptime                                                                      */
/* ========================================================================== */

TML_EXPORT int64_t tml_os_uptime(void) {
#ifdef _WIN32
    return (int64_t)(GetTickCount64() / 1000);
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (int64_t)si.uptime;
    }
    return -1;
#elif defined(__APPLE__)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0) {
        time_t now;
        time(&now);
        return (int64_t)(now - boottime.tv_sec);
    }
    return -1;
#else
    /* Generic POSIX fallback */
    return -1;
#endif
}

/* ========================================================================== */
/* Endianness                                                                  */
/* ========================================================================== */

TML_EXPORT const char* tml_os_endianness(void) {
    static const uint16_t test = 0x0102;
    const uint8_t* ptr = (const uint8_t*)&test;
    return (ptr[0] == 0x01) ? "BE" : "LE";
}

/* ========================================================================== */
/* CPU Information                                                             */
/* ========================================================================== */

TML_EXPORT int32_t tml_os_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int32_t)si.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return (count > 0) ? (int32_t)count : 1;
#endif
}

TML_EXPORT const char* tml_os_cpu_model(int32_t index) {
    static char buf[256] = {0};
    (void)index; /* Currently returns same model for all cores */

#ifdef _WIN32
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,
                      KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD size = sizeof(buf);
        DWORD type;
        if (RegQueryValueExA(hkey, "ProcessorNameString", NULL, &type, (LPBYTE)buf, &size) ==
            ERROR_SUCCESS) {
            RegCloseKey(hkey);
            return buf;
        }
        RegCloseKey(hkey);
    }
    snprintf(buf, sizeof(buf), "unknown");
#elif defined(__linux__)
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ' || *colon == '\t')
                        colon++;
                    /* Trim trailing newline */
                    char* nl = strchr(colon, '\n');
                    if (nl)
                        *nl = '\0';
                    snprintf(buf, sizeof(buf), "%s", colon);
                    fclose(f);
                    return buf;
                }
            }
        }
        fclose(f);
    }
    snprintf(buf, sizeof(buf), "unknown");
#elif defined(__APPLE__)
    size_t len = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &len, NULL, 0) != 0) {
        snprintf(buf, sizeof(buf), "unknown");
    }
#else
    snprintf(buf, sizeof(buf), "unknown");
#endif
    return buf;
}

TML_EXPORT int64_t tml_os_cpu_speed(int32_t index) {
    (void)index;

#ifdef _WIN32
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,
                      KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD speed = 0;
        DWORD size = sizeof(speed);
        DWORD type;
        if (RegQueryValueExA(hkey, "~MHz", NULL, &type, (LPBYTE)&speed, &size) == ERROR_SUCCESS) {
            RegCloseKey(hkey);
            return (int64_t)speed;
        }
        RegCloseKey(hkey);
    }
    return 0;
#elif defined(__linux__)
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    double mhz = strtod(colon + 1, NULL);
                    fclose(f);
                    return (int64_t)mhz;
                }
            }
        }
        fclose(f);
    }
    return 0;
#elif defined(__APPLE__)
    int64_t freq = 0;
    size_t len = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency", &freq, &len, NULL, 0) == 0) {
        return freq / 1000000; /* Hz to MHz */
    }
    return 0;
#else
    return 0;
#endif
}

/* ========================================================================== */
/* Load Average (POSIX only, returns 0 on Windows)                             */
/* ========================================================================== */

TML_EXPORT double tml_os_loadavg_1(void) {
#ifdef _WIN32
    return 0.0;
#else
    double avg[3];
    if (getloadavg(avg, 3) >= 1)
        return avg[0];
    return 0.0;
#endif
}

TML_EXPORT double tml_os_loadavg_5(void) {
#ifdef _WIN32
    return 0.0;
#else
    double avg[3];
    if (getloadavg(avg, 3) >= 2)
        return avg[1];
    return 0.0;
#endif
}

TML_EXPORT double tml_os_loadavg_15(void) {
#ifdef _WIN32
    return 0.0;
#else
    double avg[3];
    if (getloadavg(avg, 3) >= 3)
        return avg[2];
    return 0.0;
#endif
}

/* ========================================================================== */
/* User Information                                                            */
/* ========================================================================== */

TML_EXPORT const char* tml_os_username(void) {
    static char buf[256] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    DWORD size = sizeof(buf);
    if (!GetUserNameA(buf, &size)) {
        const char* user = getenv("USERNAME");
        if (user)
            snprintf(buf, sizeof(buf), "%s", user);
        else
            snprintf(buf, sizeof(buf), "unknown");
    }
#else
    struct passwd pw;
    struct passwd* result = NULL;
    char pwbuf[4096];
    if (getpwuid_r(getuid(), &pw, pwbuf, sizeof(pwbuf), &result) == 0 && result) {
        snprintf(buf, sizeof(buf), "%s", pw.pw_name);
    } else {
        const char* user = getenv("USER");
        if (user)
            snprintf(buf, sizeof(buf), "%s", user);
        else
            snprintf(buf, sizeof(buf), "unknown");
    }
#endif
    return buf;
}

TML_EXPORT int64_t tml_os_uid(void) {
#ifdef _WIN32
    return -1; /* No UID concept on Windows */
#else
    return (int64_t)getuid();
#endif
}

TML_EXPORT int64_t tml_os_gid(void) {
#ifdef _WIN32
    return -1; /* No GID concept on Windows */
#else
    return (int64_t)getgid();
#endif
}

TML_EXPORT const char* tml_os_shell(void) {
    static char buf[512] = {0};
    if (buf[0] != '\0')
        return buf;

#ifdef _WIN32
    const char* comspec = getenv("COMSPEC");
    if (comspec && comspec[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s", comspec);
    } else {
        snprintf(buf, sizeof(buf), "C:\\Windows\\System32\\cmd.exe");
    }
#else
    struct passwd pw;
    struct passwd* result = NULL;
    char pwbuf[4096];
    if (getpwuid_r(getuid(), &pw, pwbuf, sizeof(pwbuf), &result) == 0 && result) {
        snprintf(buf, sizeof(buf), "%s", pw.pw_shell);
    } else {
        const char* shell = getenv("SHELL");
        if (shell)
            snprintf(buf, sizeof(buf), "%s", shell);
        else
            snprintf(buf, sizeof(buf), "/bin/sh");
    }
#endif
    return buf;
}

/* ========================================================================== */
/* Process ID                                                                  */
/* ========================================================================== */

TML_EXPORT int32_t tml_os_pid(void) {
#ifdef _WIN32
    return (int32_t)GetCurrentProcessId();
#else
    return (int32_t)getpid();
#endif
}

/* ========================================================================== */
/* Environment Variables                                                       */
/* ========================================================================== */

TML_EXPORT int32_t tml_os_env_has(const char* name) {
    if (!name)
        return 0;
    return getenv(name) != NULL ? 1 : 0;
}

TML_EXPORT const char* tml_os_env_get(const char* name) {
    if (!name)
        return "";
    const char* val = getenv(name);
    return val ? val : "";
}

TML_EXPORT int32_t tml_os_env_set(const char* name, const char* value) {
    if (!name)
        return -1;
#ifdef _WIN32
    return SetEnvironmentVariableA(name, value) ? 0 : -1;
#else
    if (value) {
        return setenv(name, value, 1);
    } else {
        return unsetenv(name);
    }
#endif
}

TML_EXPORT int32_t tml_os_env_unset(const char* name) {
    if (!name)
        return -1;
#ifdef _WIN32
    return SetEnvironmentVariableA(name, NULL) ? 0 : -1;
#else
    return unsetenv(name);
#endif
}

/* ========================================================================== */
/* Process Priority                                                            */
/* ========================================================================== */

TML_EXPORT int32_t tml_os_get_priority(int32_t pid) {
#ifdef _WIN32
    HANDLE process;
    if (pid == 0) {
        process = GetCurrentProcess();
    } else {
        process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
        if (!process)
            return -1;
    }
    DWORD pclass = GetPriorityClass(process);
    if (pid != 0)
        CloseHandle(process);

    switch (pclass) {
    case IDLE_PRIORITY_CLASS:
        return 19;
    case BELOW_NORMAL_PRIORITY_CLASS:
        return 10;
    case NORMAL_PRIORITY_CLASS:
        return 0;
    case ABOVE_NORMAL_PRIORITY_CLASS:
        return -7;
    case HIGH_PRIORITY_CLASS:
        return -14;
    case REALTIME_PRIORITY_CLASS:
        return -20;
    default:
        return -1;
    }
#else
    errno = 0;
    int prio = getpriority(PRIO_PROCESS, (id_t)pid);
    if (prio == -1 && errno != 0)
        return -100;
    return (int32_t)prio;
#endif
}

TML_EXPORT int32_t tml_os_set_priority(int32_t pid, int32_t priority) {
#ifdef _WIN32
    HANDLE process;
    if (pid == 0) {
        process = GetCurrentProcess();
    } else {
        process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, (DWORD)pid);
        if (!process)
            return -1;
    }

    DWORD pclass;
    if (priority >= 15)
        pclass = IDLE_PRIORITY_CLASS;
    else if (priority >= 5)
        pclass = BELOW_NORMAL_PRIORITY_CLASS;
    else if (priority >= -4)
        pclass = NORMAL_PRIORITY_CLASS;
    else if (priority >= -10)
        pclass = ABOVE_NORMAL_PRIORITY_CLASS;
    else if (priority >= -17)
        pclass = HIGH_PRIORITY_CLASS;
    else
        pclass = REALTIME_PRIORITY_CLASS;

    BOOL ok = SetPriorityClass(process, pclass);
    if (pid != 0)
        CloseHandle(process);
    return ok ? 0 : -1;
#else
    return setpriority(PRIO_PROCESS, (id_t)pid, priority);
#endif
}

/* ========================================================================== */
/* Process Control                                                             */
/* ========================================================================== */

TML_EXPORT void tml_os_exit(int32_t code) {
    exit(code);
}

/* ========================================================================== */
/* Command-Line Arguments                                                      */
/* ========================================================================== */

#ifndef _WIN32
static int s_args_argc = -1;
static char** s_args_argv = NULL;
#ifdef __linux__
static char s_args_cmdline[65536];
#endif

static void s_args_init(void) {
    if (s_args_argc >= 0)
        return;
#ifdef __linux__
    FILE* f = fopen("/proc/self/cmdline", "r");
    if (f) {
        size_t len = fread(s_args_cmdline, 1, sizeof(s_args_cmdline) - 1, f);
        fclose(f);
        s_args_cmdline[len] = '\0';
        /* Count null-separated args */
        int count = 0;
        for (size_t i = 0; i < len; i++) {
            if (s_args_cmdline[i] == '\0')
                count++;
        }
        s_args_argc = count;
        s_args_argv = (char**)malloc(count * sizeof(char*));
        int idx = 0;
        s_args_argv[0] = s_args_cmdline;
        for (size_t i = 0; i < len && idx < count - 1; i++) {
            if (s_args_cmdline[i] == '\0') {
                idx++;
                s_args_argv[idx] = s_args_cmdline + i + 1;
            }
        }
    } else {
        s_args_argc = 0;
    }
#elif defined(__APPLE__)
    extern int* _NSGetArgc(void);
    extern char*** _NSGetArgv(void);
    s_args_argc = *_NSGetArgc();
    s_args_argv = *_NSGetArgv();
#else
    s_args_argc = 0;
#endif
}
#endif /* !_WIN32 */

TML_EXPORT int32_t tml_os_args_count(void) {
#ifdef _WIN32
    return (int32_t)__argc;
#else
    s_args_init();
    return (int32_t)s_args_argc;
#endif
}

TML_EXPORT const char* tml_os_args_get(int32_t index) {
#ifdef _WIN32
    if (index < 0 || index >= __argc)
        return "";
    return __argv[index];
#else
    s_args_init();
    if (index < 0 || index >= s_args_argc || !s_args_argv)
        return "";
    return s_args_argv[index];
#endif
}

/* ========================================================================== */
/* Current Working Directory                                                   */
/* ========================================================================== */

TML_EXPORT const char* tml_os_current_dir(void) {
    char buf[4096];
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA((DWORD)sizeof(buf), buf);
    if (len == 0 || len >= sizeof(buf)) {
        buf[0] = '\0';
    }
#else
    if (!getcwd(buf, sizeof(buf))) {
        buf[0] = '\0';
    }
#endif
    return _strdup(buf);
}

TML_EXPORT int32_t tml_os_set_current_dir(const char* path) {
    if (!path)
        return -1;
#ifdef _WIN32
    return SetCurrentDirectoryA(path) ? 0 : -1;
#else
    return chdir(path);
#endif
}

/* ========================================================================== */
/* System Time (wall clock)                                                    */
/* ========================================================================== */

TML_EXPORT int64_t tml_os_system_time_secs(void) {
#ifdef _WIN32
    /* Windows FILETIME: 100ns intervals since 1601-01-01 */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert to Unix epoch: subtract 116444736000000000 (Jan 1 1601 -> Jan 1 1970) */
    return (int64_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec;
#endif
}

TML_EXPORT int64_t tml_os_system_time_nanos(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert to Unix epoch nanos */
    uint64_t unix_100ns = uli.QuadPart - 116444736000000000ULL;
    return (int64_t)(unix_100ns * 100ULL);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

/* ========================================================================== */
/* Process Execution                                                           */
/* ========================================================================== */

/**
 * Execute a shell command and return its stdout output.
 * Returns a heap-allocated string (caller must not free — GC or TML manages it).
 * On error, returns empty string.
 */
TML_EXPORT const char* tml_os_exec(const char* command) {
    if (!command)
        return "";

#ifdef _WIN32
    FILE* fp = _popen(command, "r");
#else
    FILE* fp = popen(command, "r");
#endif
    if (!fp)
        return "";

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)mem_alloc((int64_t)cap);
    if (!buf) {
#ifdef _WIN32
        _pclose(fp);
#else
        pclose(fp);
#endif
        return "";
    }

    char tmp[1024];
    while (fgets(tmp, sizeof(tmp), fp)) {
        size_t n = strlen(tmp);
        if (len + n + 1 > cap) {
            cap = (len + n + 1) * 2;
            char* newbuf = (char*)mem_realloc(buf, (int64_t)cap);
            if (!newbuf) {
                mem_free(buf);
                break;
            }
            buf = newbuf;
        }
        memcpy(buf + len, tmp, n);
        len += n;
    }
    buf[len] = '\0';

#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif

    /* Return as a TML-compatible string via _strdup */
    const char* result = _strdup(buf);
    free(buf);
    return result ? result : "";
}

/**
 * Execute a shell command and return the exit code.
 */
TML_EXPORT int32_t tml_os_exec_status(const char* command) {
    if (!command)
        return -1;
#ifdef _WIN32
    return system(command);
#else
    int status = system(command);
    if (status == -1)
        return -1;
    return WEXITSTATUS(status);
#endif
}

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
