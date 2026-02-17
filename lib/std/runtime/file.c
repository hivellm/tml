// TML Standard Library - File I/O Runtime Implementation
// Implements: File operations (read, write, append)

#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <limits.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define stat _stat
#define mkdir(path, mode) _mkdir(path)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#include <dirent.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#endif

// ============================================================================
// File Operations
// ============================================================================

TmlFile* file_open(const char* path, int32_t mode) {
    if (!path)
        return NULL;

    const char* fmode;
    if (mode & TML_FILE_APPEND) {
        fmode = (mode & TML_FILE_READ) ? "a+b" : "ab";
    } else if (mode & TML_FILE_WRITE) {
        fmode = (mode & TML_FILE_READ) ? "w+b" : "wb";
    } else {
        fmode = "rb"; // Default to read
    }

    FILE* fp = fopen(path, fmode);
    if (!fp)
        return NULL;

    TmlFile* file = (TmlFile*)malloc(sizeof(TmlFile));
    if (!file) {
        fclose(fp);
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    file->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file->handle = fp;
    file->position = 0;
    file->mode = mode;
    file->is_open = true;

    return file;
}

// Convenience wrappers for codegen (open with specific mode)
TmlFile* file_open_read(const char* path) {
    return file_open(path, TML_FILE_READ);
}

TmlFile* file_open_write(const char* path) {
    return file_open(path, TML_FILE_WRITE);
}

TmlFile* file_open_append(const char* path) {
    return file_open(path, TML_FILE_APPEND);
}

void file_close(TmlFile* file) {
    if (!file)
        return;
    if (file->is_open && file->handle) {
        fclose((FILE*)file->handle);
    }
    file->is_open = false;
    file->handle = NULL;
    free(file);
}

bool file_is_open(TmlFile* file) {
    return file && file->is_open;
}

int64_t file_read(TmlFile* file, uint8_t* buffer, int64_t size) {
    if (!file || !file->is_open || !buffer || size <= 0)
        return 0;
    size_t read = fread(buffer, 1, size, (FILE*)file->handle);
    file->position += read;
    return (int64_t)read;
}

char* file_read_all(const char* path) {
    if (!path)
        return NULL;

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);

    return content;
}

char* file_read_line(TmlFile* file) {
    if (!file || !file->is_open)
        return NULL;

    FILE* fp = (FILE*)file->handle;

    // Initial buffer
    size_t capacity = 256;
    size_t len = 0;
    char* line = (char*)malloc(capacity);
    if (!line)
        return NULL;

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            char* new_line = (char*)realloc(line, capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
        }

        if (c == '\n') {
            break;
        }
        if (c != '\r') { // Skip CR in CRLF
            line[len++] = (char)c;
        }
    }

    if (len == 0 && c == EOF) {
        free(line);
        return NULL;
    }

    line[len] = '\0';
    file->position = ftell(fp);
    return line;
}

int64_t file_write(TmlFile* file, const uint8_t* data, int64_t size) {
    if (!file || !file->is_open || !data || size <= 0)
        return 0;
    if (!(file->mode & (TML_FILE_WRITE | TML_FILE_APPEND)))
        return 0;

    size_t written = fwrite(data, 1, size, (FILE*)file->handle);
    file->position += written;
    if (file->position > file->size) {
        file->size = file->position;
    }
    return (int64_t)written;
}

bool file_write_str(TmlFile* file, const char* str) {
    if (!str)
        return false;
    size_t len = strlen(str);
    return file_write(file, (const uint8_t*)str, len) == (int64_t)len;
}

bool file_write_all(const char* path, const char* content) {
    if (!path || !content)
        return false;

    FILE* fp = fopen(path, "wb");
    if (!fp)
        return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return written == len;
}

bool file_append_all(const char* path, const char* content) {
    if (!path || !content)
        return false;

    FILE* fp = fopen(path, "ab");
    if (!fp)
        return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return written == len;
}

bool file_flush(TmlFile* file) {
    if (!file || !file->is_open || !file->handle)
        return false;
    return fflush((FILE*)file->handle) == 0;
}

int64_t file_size(TmlFile* file) {
    return file ? file->size : 0;
}

int64_t file_position(TmlFile* file) {
    return file ? file->position : 0;
}

bool file_seek(TmlFile* file, int64_t position) {
    if (!file || !file->is_open)
        return false;
    if (fseek((FILE*)file->handle, (long)position, SEEK_SET) != 0)
        return false;
    file->position = position;
    return true;
}

bool file_seek_end(TmlFile* file) {
    if (!file || !file->is_open)
        return false;
    if (fseek((FILE*)file->handle, 0, SEEK_END) != 0)
        return false;
    file->position = file->size;
    return true;
}

void file_rewind(TmlFile* file) {
    if (file && file->is_open) {
        rewind((FILE*)file->handle);
        file->position = 0;
    }
}

// ============================================================================
// Path Operations
// ============================================================================

bool path_exists(const char* path) {
    if (!path)
        return false;
    struct stat st;
    return stat(path, &st) == 0;
}

bool path_is_file(const char* path) {
    if (!path)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISREG(st.st_mode);
}

bool path_is_dir(const char* path) {
    if (!path)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

bool path_create_dir(const char* path) {
    if (!path)
        return false;
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

bool path_create_dir_all(const char* path) {
    if (!path)
        return false;

    // Make a copy to modify
    size_t len = strlen(path);
    char* tmp = (char*)malloc(len + 1);
    if (!tmp)
        return false;
    strcpy(tmp, path);

    // Create each directory component
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            tmp[i] = '\0';
            if (!path_exists(tmp)) {
                if (!path_create_dir(tmp)) {
                    free(tmp);
                    return false;
                }
            }
            tmp[i] = '/';
        }
    }

    // Create final directory
    bool result = path_exists(path) || path_create_dir(path);
    free(tmp);
    return result;
}

bool path_remove(const char* path) {
    if (!path)
        return false;
    return remove(path) == 0;
}

bool path_remove_dir(const char* path) {
    if (!path)
        return false;
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

bool path_rename(const char* from, const char* to) {
    if (!from || !to)
        return false;
    return rename(from, to) == 0;
}

bool path_copy(const char* from, const char* to) {
    if (!from || !to)
        return false;

    FILE* src = fopen(from, "rb");
    if (!src)
        return false;

    FILE* dst = fopen(to, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    char buffer[8192];
    size_t bytes;
    bool success = true;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            success = false;
            break;
        }
    }

    fclose(src);
    fclose(dst);
    return success;
}

char* path_join(const char* base, const char* child) {
    if (!base || !child)
        return NULL;

    size_t base_len = strlen(base);
    size_t child_len = strlen(child);

    // Remove trailing separator from base
    while (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
        base_len--;
    }

    // Remove leading separator from child
    while (*child == '/' || *child == '\\') {
        child++;
        child_len--;
    }

    char* result = (char*)malloc(base_len + 1 + child_len + 1);
    if (!result)
        return NULL;

    memcpy(result, base, base_len);
#ifdef _WIN32
    result[base_len] = '\\';
#else
    result[base_len] = '/';
#endif
    memcpy(result + base_len + 1, child, child_len);
    result[base_len + 1 + child_len] = '\0';

    return result;
}

char* path_parent(const char* path) {
    if (!path)
        return NULL;

    size_t len = strlen(path);
    if (len == 0)
        return NULL;

    // Skip trailing separators
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }

    // Find last separator
    size_t i = len;
    while (i > 0 && path[i - 1] != '/' && path[i - 1] != '\\') {
        i--;
    }

    if (i == 0)
        return NULL; // No parent

    // Skip the separator
    i--;

    // Handle root path
    if (i == 0)
        i = 1;

    char* result = (char*)malloc(i + 1);
    if (!result)
        return NULL;
    memcpy(result, path, i);
    result[i] = '\0';

    return result;
}

char* path_filename(const char* path) {
    if (!path)
        return NULL;

    size_t len = strlen(path);
    if (len == 0)
        return NULL;

    // Skip trailing separators
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }

    // Find last separator
    size_t start = len;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\') {
        start--;
    }

    size_t name_len = len - start;
    char* result = (char*)malloc(name_len + 1);
    if (!result)
        return NULL;
    memcpy(result, path + start, name_len);
    result[name_len] = '\0';

    return result;
}

char* path_extension(const char* path) {
    if (!path)
        return NULL;

    const char* filename = path;
    const char* sep = strrchr(path, '/');
    if (!sep)
        sep = strrchr(path, '\\');
    if (sep)
        filename = sep + 1;

    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return NULL;

    size_t len = strlen(dot);
    char* result = (char*)malloc(len + 1);
    if (!result)
        return NULL;
    strcpy(result, dot);

    return result;
}

char* path_absolute(const char* path) {
    if (!path)
        return NULL;

#ifdef _WIN32
    char* result = (char*)malloc(MAX_PATH);
    if (!result)
        return NULL;
    if (_fullpath(result, path, MAX_PATH) == NULL) {
        free(result);
        return NULL;
    }
    return result;
#else
    char* result = realpath(path, NULL);
    return result;
#endif
}
