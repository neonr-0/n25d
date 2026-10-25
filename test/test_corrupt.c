#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

int main(void)
{
    n25dModel *m = n25dModelNew(100, 100);
    n25dPartNew(m, n25dTexture_id(7), 10, 20, 50, 50);
    n25dModelParamNew(m, "NPARAM_ANGLE_X", 14, 0.0f, -1.0f, 1.0f);
    int32_t len = 0;
    int8_t *buf = n25dSaveToMemory(m, &len);
    n25dModel *l = n25dLoadFromMemory(buf, len);
    if (!l) { printf("FAIL: no-name load NULL\n"); return 1; }
    printf("NO-NAME: parts=%d (want 1) params=%d (want 1)\n", l->nPartCount, l->nParamC);
    if (l->nPartCount != 1 || l->nParamC != 1) { printf("FAIL: no-name counts\n"); return 1; }
    if (l->nPartCount == 1) {
        n25dPart *p = &l->nPart[0];
        printf("NO-NAME part0: x=%.1f y=%.1f w=%.1f h=%.1f tex=%u (want 10,20,50,50,7)\n",
               p->x, p->y, p->width, p->height, p->TextureID.id);
        printf("NO-NAME part0 mesh v0=(%.1f,%.1f) (want 10,70)\n",
               p->n25dMeshOrigin.x[0], p->n25dMeshOrigin.y[0]);
        if (p->TextureID.id != 7) { printf("FAIL: tex\n"); return 1; }
    }
    n25dModel *t = n25dLoadFromMemory(buf, len - 7);
    printf("TRUNCATED: %s\n", t ? "LEAKED A MODEL (BAD)" : "NULL (good)");
    int8_t tiny[4] = {0x25, 0x25, 0x0d, 0xfe};
    n25dModel *t2 = n25dLoadFromMemory(tiny, 4);
    printf("TINY4: %s\n", t2 ? "LEAKED A MODEL (BAD)" : "NULL (good)");
    n25dModel *t3 = n25dLoadFromMemory(NULL, 100);
    printf("NULLBUF: %s\n", t3 ? "LEAKED A MODEL (BAD)" : "NULL (good)");
    int32_t flen = 0;
    int8_t *fd = n25dFileRead(NULL, &flen);
    printf("NULLFILE: data=%p len=%d\n", (void*)fd, flen);
    return (t == NULL && t2 == NULL && t3 == NULL && fd == NULL) ? 0 : 1;
}
