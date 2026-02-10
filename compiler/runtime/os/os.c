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

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <lmcons.h>  /* UNLEN */
#include <process.h> /* _getpid */
#include <userenv.h> /* GetUserProfileDirectoryA */
#include <windows.h>
#pragma comment(lib, "userenv.lib")
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <errno.h>
#include <pwd.h>
#include <sys/utsname.h>
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
