#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

int main(void)
{
    n25dModel *m = n25dModelNew(10, 10);
    n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
    n25dMeshReallocateMemory(&m->nPart[0].n25dMeshOrigin, 2000);
    n25dMeshReallocateMemory(&m->nPart[0].n25dMeshTransform, 2000);
    m->nPart[0].pointcount = 2000;
    int32_t len = 0;
    int8_t *buf = n25dSaveToMemory(m, &len);
    /* v2: 20 hdr + 8 name + 4 partstype + [4 parttype + 44 + 24 + 72000 + 4 paramC + 4 tex] + 4 EOF = 72120 */
    printf("BIGMESH saved len=%d expected 72120\n", len);
    return len == 72120 ? 0 : 1;
}
