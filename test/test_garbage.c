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
        // Header only, no marker stream and no EOF terminator -> truncated -> NULL
        int8_t g[20];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4]=1;
        expect_null("header-only", n25dLoadFromMemory(g, 20));
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
    {
        // v2 stream missing the Begin marker (first marker is ModelName) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 1;                   // first marker: ModelName (v2 value), no Begin
        g[24] = 0;                   // name length 0
        g[28] = 12;                  // n25FileT_EOF
        expect_null("v2-no-begin", n25dLoadFromMemory(g, 32));
    }
    {
        // v2 stream with reserved section-start markers (Groups, Motions) -> loads (no-op)
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 8;                   // n25FileT_ModelGroups (reserved, no payload)
        g[36] = 10;                  // n25FileT_ModelMotions (reserved, no payload)
        g[40] = 12;                  // n25FileT_EOF
        expect_ok("v2-reserved-sections", n25dLoadFromMemory(g, 44), 0);
    }
    {
        // v2 stream with a reserved _next marker (ModelGroup_next, payload unknown) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 9;                   // n25FileT_ModelGroup_next (reserved)
        expect_null("v2-reserved-next", n25dLoadFromMemory(g, 36));
    }
    {
        // v2 unknown marker value (13) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 13;                  // unknown marker
        g[36] = 12;                  // n25FileT_EOF
        expect_null("v2-unknown-marker", n25dLoadFromMemory(g, 40));
    }
    {
        // v2 negative marker value (-1) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 0xFF;                // marker -1 (unknown)
        expect_null("v2-negative-marker", n25dLoadFromMemory(g, 36));
    }
    {
        // v1 stream with a v2-only marker value (6 = v2 ModelPoses, unknown in v1) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 1;                    // version 1
        g[20] = 0;                   // v1 ModelName
        g[24] = 0;                   // name length 0
        g[28] = 6;                   // v2-only value: unknown in v1
        g[32] = 5;                   // v1 EOF
        expect_null("v1-unknown-marker", n25dLoadFromMemory(g, 36));
    }
    {
        // v2 double Begin -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 0;                   // n25FileT_Begin (second time)
        g[28] = 1;                   // n25FileT_ModelName
        g[32] = 0;                   // name length 0
        g[36] = 12;                  // n25FileT_EOF
        expect_null("v2-double-begin", n25dLoadFromMemory(g, 40));
    }
    {
        // v2 Begin after data (Name first) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 1;                   // n25FileT_ModelName (first marker)
        g[24] = 0;                   // name length 0
        g[28] = 0;                   // n25FileT_Begin (too late)
        g[32] = 12;                  // n25FileT_EOF
        expect_null("v2-begin-after-data", n25dLoadFromMemory(g, 36));
    }
    {
        // v2 reserved _next marker 11 (ModelMotion_next) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 11;                  // n25FileT_ModelMotion_next (reserved)
        expect_null("v2-reserved-motion-next", n25dLoadFromMemory(g, 36));
    }
    {
        // v2 data after EOF (double EOF) -> NULL
        int8_t g[64];
        memset(g, 0, sizeof(g));
        memcpy(g, "\x25\x25\x0d\xfe", 4);
        g[4] = 2;                    // version 2
        g[20] = 0;                   // n25FileT_Begin
        g[24] = 1;                   // n25FileT_ModelName
        g[28] = 0;                   // name length 0
        g[32] = 12;                  // n25FileT_EOF
        g[36] = 12;                  // n25FileT_EOF (trailing data)
        expect_null("v2-data-after-eof", n25dLoadFromMemory(g, 40));
    }
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
    return fails ? 1 : 0;
}
