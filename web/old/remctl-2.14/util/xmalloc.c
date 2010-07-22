/*
 * malloc routines with failure handling.
 *
 * Usage:
 *
 *      extern xmalloc_handler_t memory_error;
 *      extern const char *string;
 *      char *buffer;
 *      va_list args;
 *
 *      xmalloc_error_handler = memory_error;
 *      buffer = xmalloc(1024);
 *      xrealloc(buffer, 2048);
 *      free(buffer);
 *      buffer = xcalloc(1024);
 *      free(buffer);
 *      buffer = xstrdup(string);
 *      free(buffer);
 *      buffer = xstrndup(string, 25);
 *      free(buffer);
 *      xasprintf(&buffer, "%s", "some string");
 *      free(buffer);
 *      xvasprintf(&buffer, "%s", args);
 *
 * xmalloc, xcalloc, xrealloc, and xstrdup behave exactly like their C library
 * counterparts without the leading x except that they will never return NULL.
 * Instead, on error, they call xmalloc_error_handler, passing it the name of
 * the function whose memory allocation failed, the amount of the allocation,
 * and the file and line number where the allocation function was invoked
 * (from __FILE__ and __LINE__).  This function may do whatever it wishes,
 * such as some action to free up memory or a call to sleep to hope that
 * system resources return.  If the handler returns, the interrupted memory
 * allocation function will try its allocation again (calling the handler
 * again if it still fails).
 *
 * xstrndup behaves like xstrdup but only copies the given number of
 * characters.  It allocates an additional byte over its second argument and
 * always nul-terminates the string.
 *
 * xasprintf and xvasprintf behave just like their GNU glibc library
 * implementations except that they do the same checking as described above.
 * xasprintf will only be able to provide accurate file and line information
 * on systems that support variadic macros.
 *
 * The default error handler, if none is set by the caller, prints an error
 * message to stderr and exits with exit status 1.  An error handler must take
 * a const char * (function name), size_t (bytes allocated), const char *
 * (file), and int (line).
 *
 * xmalloc will return a pointer to a valid memory region on an xmalloc of 0
 * bytes, ensuring this by allocating space for one character instead of 0
 * bytes.
 *
 * The functions defined here are actually x_malloc, x_realloc, etc.  The
 * header file defines macros named xmalloc, etc. that pass the file name and
 * line number to these functions.
 *
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <errno.h>

#include <util/util.h>


/*
 * The default error handler.
 */
void
xmalloc_fail(const char *function, size_t size, const char *file, int line)
{
    sysdie("failed to %s %lu bytes at %s line %d", function,
           (unsigned long) size, file, line);
}

/* Assign to this variable to choose a handler other than the default. */
xmalloc_handler_type xmalloc_error_handler = xmalloc_fail;


void *
x_malloc(size_t size, const char *file, int line)
{
    void *p;
    size_t real_size;

    real_size = (size > 0) ? size : 1;
    p = malloc(real_size);
    while (p == NULL) {
        (*xmalloc_error_handler)("malloc", size, file, line);
        p = malloc(real_size);
    }
    return p;
}


void *
x_calloc(size_t n, size_t size, const char *file, int line)
{
    void *p;

    n = (n > 0) ? n : 1;
    size = (size > 0) ? size : 1;
    p = calloc(n, size);
    while (p == NULL) {
        (*xmalloc_error_handler)("calloc", n * size, file, line);
        p = calloc(n, size);
    }
    return p;
}


void *
x_realloc(void *p, size_t size, const char *file, int line)
{
    void *newp;

    newp = realloc(p, size);
    while (newp == NULL && size > 0) {
        (*xmalloc_error_handler)("realloc", size, file, line);
        newp = realloc(p, size);
    }
    return newp;
}


char *
x_strdup(const char *s, const char *file, int line)
{
    char *p;
    size_t len;

    len = strlen(s) + 1;
    p = malloc(len);
    while (p == NULL) {
        (*xmalloc_error_handler)("strdup", len, file, line);
        p = malloc(len);
    }
    memcpy(p, s, len);
    return p;
}


char *
x_strndup(const char *s, size_t size, const char *file, int line)
{
    char *p;

    p = malloc(size + 1);
    while (p == NULL) {
        (*xmalloc_error_handler)("strndup", size + 1, file, line);
        p = malloc(size + 1);
    }
    memcpy(p, s, size);
    p[size] = '\0';
    return p;
}


int
x_vasprintf(char **strp, const char *fmt, va_list args, const char *file,
            int line)
{
    va_list args_copy;
    int status;

    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0 && errno == ENOMEM) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        (*xmalloc_error_handler)("vasprintf", status + 1, file, line);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
    return status;
}


#if HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS
int
x_asprintf(char **strp, const char *file, int line, const char *fmt, ...)
{
    va_list args, args_copy;
    int status;

    va_start(args, fmt);
    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0 && errno == ENOMEM) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        (*xmalloc_error_handler)("asprintf", status + 1, file, line);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
    return status;
}
#else /* !(HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS) */
int
x_asprintf(char **strp, const char *fmt, ...)
{
    va_list args, args_copy;
    int status;

    va_start(args, fmt);
    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0 && errno == ENOMEM) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        (*xmalloc_error_handler)("asprintf", status + 1, __FILE__, __LINE__);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
    return status;
}
#endif /* !(HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS) */