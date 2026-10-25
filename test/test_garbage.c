// Garbage-input hardening tests
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

static int fails = 0;
static void expect_null(const char* what, n25dModel* m)
{
    if (m) { printf("FAIL %s: got model (parts=%d)\n", what, m->nPartCount); fails++; }
    else printf("PASS %s: NULL\n", what);
}
static void expect_ok(const char* what, n25dModel* m, int want_parts)
{
    if (!m) { printf("FAIL %s: NULL\n", what); fails++; }
    else if (m->nPartCount != want_parts) { printf("FAIL %s: parts=%d want %d\n", what, m->nPartCount, want_parts); fails++; }
    else printf("PASS %s: parts=%d\n", what, m->nPartCount);
}

int main(void)
{
    {
        int8_t g[300];
        for (int i = 0; i < 300; i++) g[i] = (int8_t)(i * 37 + 11);
        expect_null("garbage-300", n25dLoadFromMemory(g, 300));
    }
    {
        int8_t g[64];
        memset(g, 0xAB, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 0x77; g[5]=0; g[6]=0; g[7]=0;
        expect_null("badsig-version", n25dLoadFromMemory(g, 64));
    }
    {
        int8_t g[64];
        memset(g, 0x5A, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4]=1; g[8]=5;
        expect_null("header-claims-5-garbage", n25dLoadFromMemory(g, 64));
    }
    {
        int8_t g[24];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4]=1;
        g[20]=5;
        expect_ok("empty-valid", n25dLoadFromMemory(g, 24), 0);
    }
    {
        int8_t g[20];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4]=1;
        expect_ok("header-only", n25dLoadFromMemory(g, 20), 0);
    }
    {
        n25dModel* m = n25dModelNew(10,10);
        n25dModelSetName(m, "round", 5);
        n25dPartNew(m, n25dTexture_id(1), 0,0,10,10);
        n25dPartNew(m, n25dTexture_id(2), 5,5,10,10);
        n25dPartNew(m, n25dTexture_id(3), 9,9,10,10);
        n25dModelParamNew(m, "NPARAM_ANGLE_Y", 14, 0.5f, 0.0f, 1.0f);
        int32_t len = 0;
        int8_t* buf = n25dSaveToMemory(m, &len);
        n25dModel* l = n25dLoadFromMemory(buf, len);
        if (l && l->nPartCount == 3 && l->nParamC == 1 && l->nModelNameLen == 5 &&
            l->nModelName[0]=='r' && l->nModelName[4]=='d' &&
            l->nPart[0].TextureID.id==1 && l->nPart[1].TextureID.id==2 && l->nPart[2].TextureID.id==3)
            printf("PASS roundtrip-3parts: len=%d tex ok\n", len);
        else { printf("FAIL roundtrip-3parts: %p parts=%d\n", (void*)l, l?l->nPartCount:-1); fails++; }
    }
    {
        n25dModel* m = NULL;
        int32_t rc = n25dLoadFromFileA(&m, "/tmp/n25d_definitely_not_here_xyz.bin");
        if (rc == -1 && m == NULL) printf("PASS file-missing: rc=-1 m=NULL\n");
        else { printf("FAIL file-missing: rc=%d m=%p\n", rc, (void*)m); fails++; }
        FILE* f = fopen("/tmp/n25d_empty.bin", "wb"); fclose(f);
        m = NULL;
        rc = n25dLoadFromFileA(&m, "/tmp/n25d_empty.bin");
        if (rc == -1 && m == NULL) printf("PASS file-empty: rc=-1 m=NULL\n");
        else { printf("FAIL file-empty: rc=%d m=%p\n", rc, (void*)m); fails++; }
        n25dModel* s = n25dModelNew(1,1);
        n25dPartNew(s, n25dTexture_id(9), 1,2,3,4);
        rc = n25dSaveToFileA(s, "/tmp/n25d_rt.bin");
        m = NULL;
        int32_t rc2 = n25dLoadFromFileA(&m, "/tmp/n25d_rt.bin");
        if (rc == 0 && rc2 == 0 && m && m->nPartCount == 1 && m->nPart[0].TextureID.id == 9) printf("PASS file-roundtrip\n");
        else { printf("FAIL file-roundtrip: rc=%d rc2=%d m=%p\n", rc, rc2, (void*)m); fails++; }
    }
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
    return fails ? 1 : 0;
}
