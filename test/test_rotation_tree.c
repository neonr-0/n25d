// Parent/child rotation tree
#include <stdio.h>
#include <math.h>
#include "n25d.h"
#include "n25dfile.h"

static int check(const char* tag, n25dMesh* t, int v, float ex, float ey)
{
    int ok = fabsf(t->x[v]-ex) < 0.01f && fabsf(t->y[v]-ey) < 0.01f;
    printf("%s part v%d=(%8.2f,%8.2f) %s\n", tag, v, t->x[v], t->y[v], ok?"OK":"MISMATCH");
    return ok;
}
int main(void)
{
    n25dModel* m = n25dModelNew(100, 100);
    n25dPartNew(m, n25dTexture_id(1), 0, 0, 100, 100);  // part0
    int32_t c = n25dPartNew(m, n25dTexture_id(2), 100, 0, 100, 50); // part1 child
    n25dPartAddRotaryParent(m, c, 0);
    int32_t p = n25dModelParamNew(m, "NPARAM_ANGLE_X", 14, 0.0f, -1.0f, 1.0f);
    n25dPartSetParamRotate(m, 0, p, 0, 0.0f);
    n25dPartSetParamRotate(m, 0, p, 1, (float)(M_PI/2));
    n25dModelTransformationBegin(m);
    n25dModelTransformation(m, p, 1.0f);
    n25dModelTransformationEnd(m);
    // part0 anchor(0,0): center (0,0); (0,100)->(100,0)
    // part1 inherits: (100,50)->(50,-100)
    int ok = check("ROT90", &m->nPart[0].n25dMeshTransform, 0, 100, 0) &
             check("ROT90", &m->nPart[1].n25dMeshTransform, 0, 50, -100);
    return ok ? 0 : 1;
}
