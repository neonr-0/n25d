// Rotation convention lock: effective center = (-anchor_x, +anchor_y),
// +90 deg maps (dx,dy) -> (dy,-dx) relative to center.
// Quad layout: v0=TL v1=TR v2=BL v3=BL v4=TR v5=BR
#include <stdio.h>
#include <math.h>
#include "n25d.h"
#include "n25dfile.h"

int main(void)
{
    n25dModel* m = n25dModelNew(100, 100);
    n25dPartNew(m, n25dTexture_id(1), 0, 0, 20, 20);
    n25dPartSetAnchor(m, 0, 10.0f, 10.0f); // effective center (-10, 10)
    int32_t p = n25dModelParamNew(m, 0.0f, -1.0f, 1.0f);
    n25dPartSetParamRotate(m, 0, p, 0, 0.0f);
    n25dPartSetParamRotate(m, 0, p, 1, (float)(M_PI/2));
    n25dModelTransformationBegin(m);
    n25dModelTransformation(m, p, 1.0f);
    n25dModelTransformationEnd(m);
    struct { float x, y; } expv[6] = {
        {  0,   0 }, {  0, -20 }, { -20,   0 },
        { -20,   0 }, {  0, -20 }, { -20, -20 }
    };
    n25dMesh* t = &m->nPart[0].n25dMeshTransform;
    int ok = 1;
    for (int i = 0; i < 6; i++) {
        int good = fabsf(t->x[i] - expv[i].x) < 0.01f && fabsf(t->y[i] - expv[i].y) < 0.01f;
        printf("v%d -> (%8.3f, %8.3f) %s\n", i, t->x[i], t->y[i], good ? "OK" : "MISMATCH");
        if (!good) ok = 0;
    }
    n25dModelFree(m);
    return ok ? 0 : 1;
}
