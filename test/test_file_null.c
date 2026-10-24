#include <stdio.h>
#include "n25d.h"
#include "n25dfile.h"
int main(void)
{
    n25dModel *m = (n25dModel*)0x1;
    int32_t rc = n25dLoadFromFileA(&m, "/nonexistent_xyz_path/n25d.bin");
    printf("rc=%d m=%p\n", rc, (void*)m);
    return rc == -1 && m == NULL ? 0 : 1;
}
