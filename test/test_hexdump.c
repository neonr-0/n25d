#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

int main(void)
{
    n25dModel *m = n25dModelNew(100, 100);
    n25dModelSetName(m, "HELLO", 5);
    n25dPartNew(m, n25dTexture_id(1), 10, 20, 50, 50);
    n25dModelParamNew(m, 0.5f, -1.0f, 1.0f);
    int32_t len = 0;
    int8_t *buf = n25dSaveToMemory(m, &len);
    printf("saved %d bytes\n", len);
    for (int i = 0; i < 64 && i < len; i += 16) {
        printf("%04x: ", i);
        for (int j = 0; j < 16 && i+j < len; j++) printf("%02x ", (unsigned char)buf[i+j]);
        printf("\n");
    }
    printf("loading ===\n");
    n25dModel *l = n25dLoadFromMemory(buf, len);
    printf("loaded: parts=%d params=%d nameLen=%d\n", l->nPartCount, l->nParamC, l->nModelNameLen);
    if (!l || l->nPartCount != 1 || l->nParamC != 1 || l->nModelNameLen != 5)
    { printf("FAIL: wrong model after load\n"); return 1; }
    return 0;
}
