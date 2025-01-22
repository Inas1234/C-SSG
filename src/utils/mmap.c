#include "utils/mmap.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

MappedFile mmap_file(const char* path) {
    MappedFile result = { .data = NULL, .size = 0 };
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) return result;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return result;
    }

    result.size = st.st_size;
    result.data = mmap(NULL, result.size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (result.data == MAP_FAILED) {
        result.data = NULL;
        result.size = 0;
    }
    
    return result;
}

void munmap_file(MappedFile mf) {
    if (mf.data) munmap((void *)mf.data, mf.size);
}