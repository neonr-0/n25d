// H-tier feature tests: texture v2, v1 compat, free API, flat getter
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"
static int fails = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS %s\n", msg); else { printf("FAIL %s\n", msg); fails++; } } while(0)

//Write a little-endian int32 at offset, return new offset
static int32_t put32(int8_t* p, int32_t off, int32_t val)
{
	p[off] = (int8_t)(val & 0xFF);
	p[off+1] = (int8_t)((val >> 8) & 0xFF);
	p[off+2] = (int8_t)((val >> 16) & 0xFF);
	p[off+3] = (int8_t)((val >> 24) & 0xFF);
	return off + 4;
}

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
        // 2. v1 compat: genuine v1 file, hand-crafted (1 part, 6-vertex box, no texture id at all)
        int8_t v1[4096];
        memset(v1, 0, sizeof(v1));
        int32_t o = 0;
        o = put32(v1, o, 0xFE0D2525); //signature (LE bytes 25 25 0d fe)
        o = put32(v1, o, 1);          //file version 1
        o = put32(v1, o, 1);          //n25dModelPartCount 1
        o = put32(v1, o, 0);          //n25dModelParamCount 0
        o += 4;                       //reserved
        o = put32(v1, o, 0);          //n25FileT_ModelName
        o = put32(v1, o, 0);          //name length 0
        o = put32(v1, o, 3);          //n25FileT_ModelParams (no count: 0 params, next marker follows)
        o = put32(v1, o, 1);          //n25FileT_ModelParts
        o = put32(v1, o, 2);          //n25FileT_ModelPart_next
        o = put32(v1, o, 6);          //part pointcount 6
        o += 40;                      //10 zero fields: draworder, x,y,z,w,h,ax,ay, parent, parentC
        o = put32(v1, o, 6);          //mesh pointcount 6 (must match part)
        o += 20;                      //5 zero fields: mesh draworder + 4 uv floats
        o += 6 * 36;                  //6 vertices x 9 floats (zeros)
        o = put32(v1, o, 0);          //mesh param count 0
        o = put32(v1, o, 5);          //n25FileT_EOF
        n25dModel* v1m = n25dLoadFromMemory(v1, o);
        CHECK(v1m && v1m->nPartCount == 1, "tex-v1: genuine v1 file loads");
        CHECK(v1m && v1m->nPartCount == 1 && v1m->nPart[0].TextureID.id == 0, "tex-v1: tex defaults to 0");
        if (v1m) n25dModelFree(v1m);
        if (l) n25dModelFree(l);
    }
    // 3. Free API
    {
        n25dModel* m = n25dModelNew(10,10);
        n25dModelSetName(m, "abc", 3);
        n25dPartNew(m, n25dTexture_id(1), 0,0,10,10);
        n25dModelParamNew(m, "NPARAM_ANGLE_Z", 14, 0, 0, 1);
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
