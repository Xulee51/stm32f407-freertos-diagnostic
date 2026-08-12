#include "main.h"
#include <sys/stat.h>
#include <errno.h>

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, 100U) != HAL_OK) {
        errno = EIO;
        return -1;
    }
    return len;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
void *_sbrk(ptrdiff_t incr)
{
    extern char _end;
    extern char _estack;
    static char *heap_end;
    char *prev;
    if (heap_end == 0) heap_end = &_end;
    prev = heap_end;
    if ((heap_end + incr) > (&_estack - 1024)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return prev;
}
