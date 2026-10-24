// H-tier feature tests: texture v2, v1 compat, free API, flat getter
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"
static int fails = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS %s\n", msg); else { printf("FAIL %s\n", msg); fails++; } } while(0)

int main(void)
{
    // 1. Texture id round-trip (v2)
    {
        n25dModel* m = n25dModelNew(10,10);
        n25dPartNew(m, n25dTexture_id(7), 1,2,3,4);
        n25dPartNew(m, n25dTexture_id(99), 5,6,7,8);
        int32_t len = 0;
        int8_t* buf = n25dSaveToMemory(m, &len);
        n25dModel* l = n25dLoadFromMemory(buf, len);
        CHECK(l && l->nPartCount == 2, "tex-v2: loaded");
        CHECK(l && l->nPart[0].TextureID.id == 7, "tex-v2: part0 tex==7");
        CHECK(l && l->nPart[1].TextureID.id == 99, "tex-v2: part1 tex==99");
        // 2. v1 compat: patch version byte to 1, strip 4 tex bytes per part (before EOF)
        // layout ends: ...partC(4) tex0(4) tex1(4) EOF(4) -> remove both tex ints
        int8_t v1[4096];
        memcpy(v1, buf, len);
        v1[4] = 1; // version
        memmove(v1+len-8, v1+len-4, 4); // keep EOF, drop 2 tex ints
        n25dModel* v1m = n25dLoadFromMemory(v1, len-8);
        CHECK(v1m && v1m->nPartCount == 2, "tex-v1: old file loads");
        CHECK(v1m && v1m->nPart[0].TextureID.id == 0, "tex-v1: tex defaults to 0");
        if (v1m) n25dModelFree(v1m);
        if (l) n25dModelFree(l);
    }
    // 3. Free API
    {
        n25dModel* m = n25dModelNew(10,10);
        n25dModelSetName(m, "abc", 3);
        n25dPartNew(m, n25dTexture_id(1), 0,0,10,10);
        n25dModelParamNew(m, 0,0,1);
        int32_t len = 0;
        int8_t* buf = n25dSaveToMemory(m, &len);
        n25dModel* l = n25dLoadFromMemory(buf, len);
        CHECK(l != NULL, "free: loaded model");
        if (l) n25dModelFree(l);       // must not crash
        n25dModelFree(NULL);            // NULL safe
        n25dMeshFree(NULL);             // NULL safe
        n25dModelFree(m);               // free original too
        CHECK(1, "free: no crash");
    }
    // 4. Flat getter
    {
        n25dModel* m = n25dModelNew(10,10);
        n25dPartNew(m, n25dTexture_id(1), 0,0,10,10);
        int32_t need = n25dGetTransformedVertexData(m, 0, NULL, 0);
        CHECK(need == 54, "getter: query size == 54");
        float data[54];
        int32_t n = n25dGetTransformedVertexData(m, 0, data, 54);
        CHECK(n == 54, "getter: wrote 54 floats");
        CHECK(data[0] == m->nPart[0].n25dMeshTransform.x[0] &&
              data[1] == m->nPart[0].n25dMeshTransform.y[0] &&
              data[8] == m->nPart[0].n25dMeshTransform.tv[0], "getter: values match mesh");
        CHECK(n25dGetTransformedVertexData(m, 5, data, 54) == -1, "getter: bad partID -> -1");
        CHECK(n25dGetTransformedVertexData(m, 0, data, 10) == -1, "getter: small buffer -> -1");
        CHECK(n25dGetTransformedVertexData(NULL, 0, data, 54) == -1, "getter: NULL model -> -1");
        n25dModelFree(m);
    }
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
    return fails ? 1 : 0;
}
