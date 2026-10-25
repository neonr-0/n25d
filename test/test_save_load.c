#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

int main(void)
{
    n25dModel *m = n25dModelNew(100, 100);
    n25dModelSetName(m, "HELLO", 5);
    int32_t p0 = n25dPartNew(m, n25dTexture_id(7), 10, 20, 50, 50);
    int32_t param = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
    (void)param;
    printf("created: part=%d param=%d nameLen=%d name='%.*s'\n",
           p0, param, m->nModelNameLen, m->nModelNameLen, m->nModelName);

    int32_t len = 0;
    int8_t *buf = n25dSaveToMemory(m, &len);
    printf("saved %d bytes\n", len);
    if (!buf) return 1;

    n25dModel *l = n25dLoadFromMemory(buf, len);
    if (!l) { printf("FAIL: load returned NULL\n"); return 1; }
    printf("loaded: parts=%d params=%d nameLen=%d\n",
           l->nPartCount, l->nParamC, l->nModelNameLen);
    if (l->nPartCount != 1 || l->nParamC != 1 || l->nModelNameLen != 5 ||
        l->nPart[0].TextureID.id != 7)
    { printf("FAIL: wrong model after load\n"); return 1; }
    if (l->nModelName) {
        for (int i = 0; i < l->nModelNameLen && i < 32; i++)
            printf("[%02x]", (unsigned char)l->nModelName[i]);
        printf("\n");
    }
    if (l->nPartCount > 0) {
        n25dPart *pt = &l->nPart[0];
        printf("loaded part0: x=%.1f y=%.1f w=%.1f h=%.1f pts=%d tex=%u\n",
               pt->x, pt->y, pt->width, pt->height, pt->pointcount, pt->TextureID.id);
    }

    n25dModel *t = n25dLoadFromMemory(buf, len - 7);
    printf("truncated load: %s\n", t ? "LEAKED A MODEL (BAD)" : "NULL (good)");
    int8_t tiny[4] = {0x25, 0x25, 0x0D, 0xFE};
    n25dModel *t2 = n25dLoadFromMemory(tiny, 4);
    printf("tiny load: %s\n", t2 ? "LEAKED A MODEL (BAD)" : "NULL (good)");
    return (t == NULL && t2 == NULL) ? 0 : 1;
}
