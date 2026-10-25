// Named param core tests: round-trip, lookup, set/apply, v1 compat, bounds fix, name cap
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n25d.h"
#include "n25dfile.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS %s\n", msg); else { printf("FAIL %s\n", msg); fails++; } } while(0)

// Write a little-endian int32 at offset, return new offset
static int32_t put32(int8_t* p, int32_t off, int32_t val)
{
	p[off]   = (int8_t)(val & 0xFF);
	p[off+1] = (int8_t)((val >> 8) & 0xFF);
	p[off+2] = (int8_t)((val >> 16) & 0xFF);
	p[off+3] = (int8_t)((val >> 24) & 0xFF);
	return off + 4;
}

// Write a little-endian float at offset, return new offset
static int32_t putf32(int8_t* p, int32_t off, float val)
{
	union { float f; unsigned int u; } v; v.f = val;
	p[off]   = (int8_t)(v.u & 0xFF);
	p[off+1] = (int8_t)((v.u >> 8) & 0xFF);
	p[off+2] = (int8_t)((v.u >> 16) & 0xFF);
	p[off+3] = (int8_t)((v.u >> 24) & 0xFF);
	return off + 4;
}

int main(void)
{
	// 1. Round-trip named params (save -> load preserves name, id order, def/min/max)
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		n25dModelParamNew(m, "NPARAM_ANGLE_X",    14, 0.0f, -1.0f, 1.0f);
		n25dModelParamNew(m, "NPARAM_EYE_L", 12, 1.0f,  0.0f, 1.0f);
		n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nParamC == 3, "rt: 3 params loaded");
		CHECK(l && n25dModelGetParamID(l, "NPARAM_ANGLE_X",    14) == 0, "rt: id angle_x == 0");
		CHECK(l && n25dModelGetParamID(l, "NPARAM_EYE_L", 12) == 1, "rt: id eye_l_open == 1");
		CHECK(l && n25dModelGetParamID(l, "NPARAM_MOUTH", 12) == 2, "rt: id mouth_open_y == 2");
		CHECK(l && l->nParam[1].param_def == 1.0f && l->nParam[1].param_min == 0.0f && l->nParam[1].param_max == 1.0f, "rt: eye def/min/max preserved");
		CHECK(l && l->nParam[0].param_def == 0.0f && l->nParam[0].param_min == -1.0f && l->nParam[0].param_max == 1.0f, "rt: angle def/min/max preserved");
		if (l) n25dModelFree(l);
		n25dModelFree(m);
	}
	// 2. GetParamID: found, unknown, name_len 0, NULL
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dModelParamNew(m, "NPARAM_BREATH", 13, 0.5f, -1.0f, 1.0f);
		CHECK(n25dModelGetParamID(m, "NPARAM_BREATH", 13) == 0, "id: found == 0");
		CHECK(n25dModelGetParamID(m, "NPARAM_NOPE", 11) == -1, "id: unknown == -1");
		CHECK(n25dModelGetParamID(m, "NPARAM_BREATH", 0) == -1, "id: name_len 0 == -1");
		CHECK(n25dModelGetParamID(m, NULL, 0) == -1, "id: NULL name == -1");
		n25dModelFree(m);
	}
	// 3. SetParam clamp: max, min, in-range, bad id
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		n25dModelSetParam(m, p, 5.0f);
		CHECK(m->nParam[p].param_cur == 1.0f, "set: clamp to max");
		n25dModelSetParam(m, p, -5.0f);
		CHECK(m->nParam[p].param_cur == -1.0f, "set: clamp to min");
		n25dModelSetParam(m, p, 0.3f);
		CHECK(m->nParam[p].param_cur == 0.3f, "set: in-range exact");
		n25dModelSetParam(m, 99, 0.5f); // bad id, no crash
		CHECK(1, "set: bad id no crash");
		n25dModelFree(m);
	}
	// 4. ApplyParams moves a bound vertex by x_hi * param
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamVertexPos(m, 0, p, 1, 0, 10.0f, 0.0f); // x_hi[0] = 10
		float ox0 = m->nPart[0].n25dMeshOrigin.x[0];
		n25dModelSetParam(m, p, 1.0f);
		n25dModelApplyParams(m);
		CHECK(m->nPart[0].n25dMeshTransform.x[0] == ox0 + 10.0f, "apply: vertex0 x += 10");
		n25dModelFree(m);
	}
	// 5. v1 compat: genuine v1 param block (3 floats, no name) loads as unnamed, def == saved cur
	{
		int8_t v1[512];
		memset(v1, 0, sizeof(v1));
		int32_t o = 0;
		o = put32(v1, o, 0xFE0D2525); // signature
		o = put32(v1, o, 1);          // version 1
		o = put32(v1, o, 1);          // part count 1
		o = put32(v1, o, 1);          // param count 1
		o += 4;                       // reserved
		o = put32(v1, o, 0);          // n25FileT_ModelName
		o = put32(v1, o, 0);          // name length 0
		o = put32(v1, o, 3);          // n25FileT_ModelParams
		o = put32(v1, o, 4);          // n25FileT_ModelParam_next
		o = putf32(v1, o, 0.5f);      // cur
		o = putf32(v1, o, -1.0f);     // min
		o = putf32(v1, o, 1.0f);      // max
		o = put32(v1, o, 1);          // n25FileT_ModelParts
		o = put32(v1, o, 2);          // n25FileT_ModelPart_next
		o = put32(v1, o, 6);          // part pointcount 6
		o += 40;                      // 10 zero part fields
		o = put32(v1, o, 6);          // mesh pointcount 6
		o += 20;                      // 5 zero mesh fields
		o += 6 * 36;                  // 6 verts x 9 floats (zero)
		o = put32(v1, o, 0);          // mesh param count 0
		o = put32(v1, o, 5);          // n25FileT_EOF
		n25dModel* v1m = n25dLoadFromMemory(v1, o);
		CHECK(v1m && v1m->nParamC == 1, "v1: param loads");
		CHECK(v1m && v1m->nParam[0].name_len == 0, "v1: param unnamed");
		CHECK(v1m && v1m->nParam[0].param_def == 0.5f, "v1: def == saved cur");
		CHECK(v1m && v1m->nParam[0].param_min == -1.0f && v1m->nParam[0].param_max == 1.0f, "v1: min/max preserved");
		if (v1m) n25dModelFree(v1m);
	}
	// 6. Bounds fix: part created AFTER a param has 0 mesh params -> ApplyParams must not crash
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_X", 8, 0.0f, -1.0f, 1.0f);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10); // created after the param
		CHECK(m->nPart[0].n25dMeshParamC == 0, "bounds: part has 0 mesh params");
		n25dModelSetParam(m, p, 0.5f);
		n25dModelApplyParams(m); // pre-fix: OOB / NULL-deref on n25dMeshParam[ParamID]
		CHECK(m->nPart[0].n25dMeshTransform.x[0] == m->nPart[0].n25dMeshOrigin.x[0], "bounds: unbound part vertex unchanged");
		CHECK(1, "bounds: apply no crash");
		n25dModelFree(m);
	}
	// 7. Name cap: v2 param with name_len > MAX_PARAM_NAME -> load returns NULL
	{
		int8_t v2[512];
		memset(v2, 0, sizeof(v2));
		int32_t o = 0;
		o = put32(v2, o, 0xFE0D2525); // signature
		o = put32(v2, o, 2);          // version 2
		o = put32(v2, o, 0);          // part count 0
		o = put32(v2, o, 1);          // param count 1
		o += 4;                       // reserved
		o = put32(v2, o, 0);          // n25FileT_ModelName
		o = put32(v2, o, 0);          // name length 0
		o = put32(v2, o, 3);          // n25FileT_ModelParams
		o = put32(v2, o, 4);          // n25FileT_ModelParam_next
		o = put32(v2, o, (int32_t)MAX_PARAM_NAME + 1); // name_len too big
		o += MAX_PARAM_NAME + 1;      // name bytes (padding)
		o = putf32(v2, o, 0.0f);      // def
		o = putf32(v2, o, -1.0f);     // min
		o = putf32(v2, o, 1.0f);      // max
		o = put32(v2, o, 5);          // n25FileT_EOF
		n25dModel* cap = n25dLoadFromMemory(v2, o);
		CHECK(cap == NULL, "cap: name_len > MAX -> NULL");
		if (cap) n25dModelFree(cap);
	}
	// 8. Boundary + edge cases (review follow-ups)
	{
		// 8a. name_len == MAX_PARAM_NAME (128) round-trips (the passing boundary)
		char longname[MAX_PARAM_NAME + 1];
		for (int i = 0; i < MAX_PARAM_NAME; i++) longname[i] = (char)('A' + (i % 26));
		longname[MAX_PARAM_NAME] = '\0';
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p = n25dModelParamNew(m, longname, MAX_PARAM_NAME, 0.0f, -1.0f, 1.0f);
		CHECK(p == 0, "edge: 128-char name accepted");
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nParamC == 1 && l->nParam[0].name_len == MAX_PARAM_NAME, "edge: 128-char name round-trips");
		CHECK(l && n25dModelGetParamID(l, longname, MAX_PARAM_NAME) == 0, "edge: 128-char name resolves");
		CHECK(n25dModelParamNew(m, longname, MAX_PARAM_NAME + 1, 0.0f, -1.0f, 1.0f) == -1, "edge: 129-char name rejected at creation");
		if (l) n25dModelFree(l);
		n25dModelFree(m);

		// 8b. v2 unnamed param (name_len 0) round-trips via API (blob-skip path)
		n25dModel* m2 = n25dModelNew(10, 10);
		n25dPartNew(m2, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p2 = n25dModelParamNew(m2, NULL, 0, 0.25f, -1.0f, 1.0f);
		CHECK(p2 == 0 && m2->nParam[0].name_len == 0, "edge: unnamed param created");
		int32_t len2 = 0;
		int8_t* buf2 = n25dSaveToMemory(m2, &len2);
		n25dModel* l2 = n25dLoadFromMemory(buf2, len2);
		CHECK(l2 && l2->nParamC == 1 && l2->nParam[0].name_len == 0, "edge: unnamed v2 param round-trips");
		CHECK(l2 && l2->nParam[0].param_def == 0.25f, "edge: unnamed v2 def preserved");
		if (l2) n25dModelFree(l2);
		n25dModelFree(m2);

		// 8c. v2 negative name_len -> load NULL
		int8_t v2neg[256];
		memset(v2neg, 0, sizeof(v2neg));
		int32_t o = 0;
		o = put32(v2neg, o, 0xFE0D2525); // signature
		o = put32(v2neg, o, 2);          // version 2
		o = put32(v2neg, o, 0);          // part count 0
		o = put32(v2neg, o, 1);          // param count 1
		o += 4;                          // reserved
		o = put32(v2neg, o, 0);          // n25FileT_ModelName
		o = put32(v2neg, o, 0);          // name length 0
		o = put32(v2neg, o, 3);          // n25FileT_ModelParams
		o = put32(v2neg, o, 4);          // n25FileT_ModelParam_next
		o = put32(v2neg, o, -1);         // name_len = -1
		o = putf32(v2neg, o, 0.0f);      // def
		o = putf32(v2neg, o, -1.0f);     // min
		o = putf32(v2neg, o, 1.0f);      // max
		o = put32(v2neg, o, 5);          // n25FileT_EOF
		n25dModel* neg = n25dLoadFromMemory(v2neg, o);
		CHECK(neg == NULL, "edge: negative name_len -> NULL");
		if (neg) n25dModelFree(neg);
	}
	printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
	return fails ? 1 : 0;
}
