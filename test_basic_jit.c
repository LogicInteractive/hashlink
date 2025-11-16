#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

int main() {
    uint32_t *code = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    
    code[0] = 0xd2800a80;  // MOVZ X0, #84
    code[1] = 0xd65f03c0;  // RET
    
    __builtin___clear_cache((char*)code, (char*)(code+2));
    
    uint64_t (*func)(void) = (void*)code;
    uint64_t result = func();
    
    printf("Result: %lu (expected: 84)\n", result);
    munmap(code, 4096);
    return (result == 84) ? 0 : 1;
}
