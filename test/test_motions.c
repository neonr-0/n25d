// Motion tests: authoring/lookup/query, interpolation (linear + cubic bezier
// easing, hold at ends), playback (partial override + clamp + vertex drive),
// save/load round-trip, and corrupt/edge load rejection.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

// Write: header + Begin + ModelName(len0) + ModelParams + nparams param blocks
// (NPARAM_0..NPARAM_n-1) + ModelParts(0) + ModelPoses(0) + ModelMotions marker.
// The header claims 0 poses (none present in these hand-crafted streams).
// Returns the offset immediately after the ModelMotions marker.
static int32_t write_motion_prefix(int8_t* buf, int nparams)
{
	int32_t o = 0;
	o = put32(buf, o, 0xFE0D2525); // signature
	o = put32(buf, o, 2);          // version 2
	o = put32(buf, o, 0);          // part count 0
	o = put32(buf, o, nparams);    // param count
	o = put32(buf, o, 0);          // pose count 0
	o = put32(buf, o, 0);          // n25FileT_Begin
	o = put32(buf, o, 1);          // n25FileT_ModelName
	o = put32(buf, o, 0);          // name length 0
	o = put32(buf, o, 4);          // n25FileT_ModelParams
	for (int i = 0; i < nparams; i++)
	{
		char nm[16]; int nl = snprintf(nm, sizeof nm, "NPARAM_%d", i);
		o = put32(buf, o, 5);      // n25FileT_ModelParam_next
		o = put32(buf, o, nl);     // name_len
		memcpy(buf + o, nm, (size_t)nl); o += nl;
		o = putf32(buf, o, 0.0f);  // def
		o = putf32(buf, o, -1.0f); // min
		o = putf32(buf, o, 1.0f);  // max
	}
	o = put32(buf, o, 2);          // n25FileT_ModelParts (0 parts)
	o = put32(buf, o, 6);          // n25FileT_ModelPoses (0 poses)
	o = put32(buf, o, 10);         // n25FileT_ModelMotions
	return o;
}

int main(void)
{
	// 1. Authoring: MotionNew / AddCurve / AddKey success + failure
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		int32_t p1 = n25dModelParamNew(m, "NPARAM_EYE_L", 12, 1.0f, 0.0f, 1.0f);
		CHECK(n25dModelMotionNew(NULL, "NMOTION_A", 11) == -1, "authoring: NULL model == -1");
		int32_t mo = n25dModelMotionNew(m, "NMOTION_TALK", 12);
		CHECK(mo == 0, "authoring: motion id == 0");
		int32_t c0 = n25dModelMotionAddCurve(m, mo, p0);
		int32_t c1 = n25dModelMotionAddCurve(m, mo, p1);
		CHECK(c0 == 0 && c1 == 1, "authoring: curve indices 0,1");
		CHECK(n25dModelMotionAddCurve(m, 99, p0) == -1, "authoring: bad motion id == -1");
		CHECK(n25dModelMotionAddCurve(m, mo, 99) == -1, "authoring: bad param id == -1");
		CHECK(n25dModelMotionCurveAddKey(m, mo, c0, 0.0f, 0.0f) == 0, "authoring: key ok");
		CHECK(n25dModelMotionCurveAddKey(m, mo, c0, 1.0f, 1.0f) == 0, "authoring: key ok");
		CHECK(n25dModelMotionCurveAddKey(m, mo, 99, 0.0f, 0.0f) == -1, "authoring: bad curve index == -1");
		CHECK(n25dModelMotionCurveAddKey(NULL, mo, c0, 0.5f, 0.5f) == -1, "authoring: NULL model == -1");
		//Out-of-order key (earlier than the last) is rejected; equal time allowed
		CHECK(n25dModelMotionCurveAddKey(m, mo, c0, 0.5f, 0.5f) == -1, "authoring: out-of-order key rejected");
		CHECK(n25dModelMotionCurveAddKey(m, mo, c1, 1.0f, 0.5f) == 0, "authoring: first key ok");
		CHECK(n25dModelMotionCurveAddKey(m, mo, c1, 1.0f, 0.6f) == 0, "authoring: equal-time key ok");
		CHECK(n25dModelMotionCurveAddKey(m, mo, c1, 0.9f, 0.7f) == -1, "authoring: out-of-order key rejected 2");
		CHECK(m->nMotion[0].curves[0].keyC == 2, "authoring: failed adds not stored");
		CHECK(m->nMotion[0].curves[1].keyC == 2, "authoring: equal-time keys stored");
		//SetEase validation
		CHECK(n25dModelMotionCurveKeySetEase(m, mo, c0, 0, 2, 0, 0, 0, 0) == -1, "ease: unknown easing tag == -1");
		CHECK(n25dModelMotionCurveKeySetEase(m, mo, c0, 9, (int32_t)N25EASE_BEZIER, 0, 0, 0, 0) == -1, "ease: bad key index == -1");
		CHECK(n25dModelMotionCurveKeySetEase(m, mo, c0, 0, (int32_t)N25EASE_BEZIER, 0.42f, 0.0f, 0.58f, 1.0f) == 0, "ease: set bezier ok");
		CHECK(m->nMotion[0].curves[0].keys[0].ease == (int32_t)N25EASE_BEZIER &&
			m->nMotion[0].curves[0].keys[0].bz[0] == 0.42f &&
			m->nMotion[0].curves[0].keys[0].bz[3] == 1.0f, "ease: bezier stored on key");
		n25dModelFree(m);
	}
	// 2. GetMotionID: found / unknown / len0 / NULL
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t mo = n25dModelMotionNew(m, "NMOTION_TALK", 12);
		CHECK(mo == 0, "id: created");
		CHECK(n25dModelGetMotionID(m, "NMOTION_TALK", 12) == 0, "id: found == 0");
		CHECK(n25dModelGetMotionID(m, "NMOTION_NOPE", 12) == -1, "id: unknown == -1");
		CHECK(n25dModelGetMotionID(m, "NMOTION_TALK", 0) == -1, "id: name_len 0 == -1");
		CHECK(n25dModelGetMotionID(m, NULL, 12) == -1, "id: NULL name == -1");
		CHECK(n25dModelGetMotionID(NULL, "NMOTION_TALK", 12) == -1, "id: NULL model == -1");
		n25dModelFree(m);
	}
	// 3. Query getters: curve count, curve param id, key count, duration
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f);
		int32_t p1 = n25dModelParamNew(m, "NPARAM_B", 8, 0.0f, -1.0f, 1.0f);
		int32_t mo = n25dModelMotionNew(m, "NMOTION_Q", 9);
		int32_t c0 = n25dModelMotionAddCurve(m, mo, p0);
		n25dModelMotionAddCurve(m, mo, p1);
		n25dModelMotionCurveAddKey(m, mo, c0, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c0, 2.0f, 1.0f);
		n25dModelMotionCurveAddKey(m, mo, 1, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, 1, 3.5f, 1.0f);
		CHECK(n25dModelMotionGetCurveCount(m, mo) == 2, "query: curve count == 2");
		CHECK(n25dModelMotionGetCurveCount(m, 99) == -1, "query: bad motion id == -1");
		CHECK(n25dModelMotionCurveGetParamID(m, mo, 0) == p0, "query: curve0 param id");
		CHECK(n25dModelMotionCurveGetParamID(m, mo, 9) == -1, "query: bad curve index == -1");
		CHECK(n25dModelMotionCurveGetKeyCount(m, mo, 0) == 2, "query: key count == 2");
		CHECK(n25dModelMotionCurveGetKeyCount(m, mo, 1) == 2, "query: key count curve1 == 2");
		CHECK(n25dMotionGetDuration(m, mo) == 3.5f, "query: duration == max key time (3.5)");
		CHECK(n25dMotionGetDuration(m, 99) == 0.0f, "query: bad motion id duration == 0");
		n25dModelFree(m);
	}
	// 4. Name cap: 129-byte motion name rejected at creation; 128 accepted
	{
		n25dModel* m = n25dModelNew(10, 10);
		char big[129]; memset(big, 'A', 128); big[128] = 0;
		CHECK(n25dModelMotionNew(m, big, 129) == -1, "cap: 129-byte name rejected");
		char ok[129]; memset(ok, 'B', 128); ok[128] = 0;
		CHECK(n25dModelMotionNew(m, ok, 128) == 0, "cap: 128-byte name accepted");
		n25dModelFree(m);
	}
	// 5. Interpolation: exact key, linear midpoint, hold at ends, single key
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		int32_t mo = n25dModelMotionNew(m, "NMOTION_LIN", 11);
		int32_t c = n25dModelMotionAddCurve(m, mo, p);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 2.0f);
		CHECK(n25dMotionCurveValueAt(m, mo, c, 0.0f) == 0.0f, "interp: exact first key value");
		CHECK(n25dMotionCurveValueAt(m, mo, c, 1.0f) == 2.0f, "interp: exact last key value");
		CHECK(n25dMotionCurveValueAt(m, mo, c, 0.5f) == 1.0f, "interp: linear midpoint");
		CHECK(n25dMotionCurveValueAt(m, mo, c, -0.5f) == 0.0f, "interp: hold below first key");
		CHECK(n25dMotionCurveValueAt(m, mo, c, 2.5f) == 2.0f, "interp: hold above last key");
		CHECK(n25dMotionCurveValueAt(m, 99, 0, 0.5f) == 0.0f, "interp: bad motion id == 0.0f");
		CHECK(n25dMotionCurveValueAt(m, mo, 99, 0.5f) == 0.0f, "interp: bad curve index == 0.0f");
		//Single-key curve returns that value for all t
		int32_t mo2 = n25dModelMotionNew(m, "NMOTION_ONE", 11);
		int32_t c2 = n25dModelMotionAddCurve(m, mo2, p);
		n25dModelMotionCurveAddKey(m, mo2, c2, 5.0f, 3.0f);
		CHECK(n25dMotionCurveValueAt(m, mo2, c2, 0.0f) == 3.0f, "interp: single key holds early");
		CHECK(n25dMotionCurveValueAt(m, mo2, c2, 5.0f) == 3.0f, "interp: single key exact");
		CHECK(n25dMotionCurveValueAt(m, mo2, c2, 9.0f) == 3.0f, "interp: single key holds late");
		n25dModelFree(m);
	}
	// 6. Bezier easing: identity == linear, endpoints, symmetric in-out,
	//    asymmetric in (clearly below linear), final-segment hold
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -10.0f, 10.0f);
		//Identity bezier (0,0)/(1,1) equals linear: 0 -> 10, midpoint must be 5
		int32_t mo = n25dModelMotionNew(m, "NMOTION_ID", 10);
		int32_t c = n25dModelMotionAddCurve(m, mo, p);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 10.0f);
		n25dModelMotionCurveKeySetEase(m, mo, c, 0, (int32_t)N25EASE_BEZIER, 0.0f, 0.0f, 1.0f, 1.0f);
		CHECK(n25dMotionCurveValueAt(m, mo, c, 0.5f) == 5.0f, "bezier: identity control points == linear midpoint");
		//Endpoints hold: t=0 -> start value, t=1 -> end value
		CHECK(n25dMotionCurveValueAt(m, mo, c, 0.0f) == 0.0f, "bezier: t=0 -> start value");
		CHECK(n25dMotionCurveValueAt(m, mo, c, 1.0f) == 10.0f, "bezier: t=1 -> end value");
		//Symmetric ease-in-out cubic-bezier(0.42,0,0.58,1): by symmetry the
		//midpoint maps to exactly 0.5 (computed independently, not via the solver)
		int32_t mo2 = n25dModelMotionNew(m, "NMOTION_INOUT", 13);
		int32_t c2 = n25dModelMotionAddCurve(m, mo2, p);
		n25dModelMotionCurveAddKey(m, mo2, c2, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo2, c2, 1.0f, 10.0f);
		n25dModelMotionCurveKeySetEase(m, mo2, c2, 0, (int32_t)N25EASE_BEZIER, 0.42f, 0.0f, 0.58f, 1.0f);
		float v_inout = n25dMotionCurveValueAt(m, mo2, c2, 0.5f);
		CHECK(fabsf(v_inout - 5.0f) < 1e-4f, "bezier: symmetric in-out midpoint == linear midpoint");
		//Asymmetric ease-in cubic-bezier(0.42,0,1,1): at t=0.5 the eased fraction
		//is ~0.32, so the value must be clearly below the linear midpoint 5 and
		//above 1 (proves the curve is applied, not ignored)
		int32_t mo3 = n25dModelMotionNew(m, "NMOTION_IN", 10);
		int32_t c3 = n25dModelMotionAddCurve(m, mo3, p);
		n25dModelMotionCurveAddKey(m, mo3, c3, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo3, c3, 1.0f, 10.0f);
		n25dModelMotionCurveKeySetEase(m, mo3, c3, 0, (int32_t)N25EASE_BEZIER, 0.42f, 0.0f, 1.0f, 1.0f);
		float v_in = n25dMotionCurveValueAt(m, mo3, c3, 0.5f);
		CHECK(v_in < 4.5f && v_in > 1.0f, "bezier: ease-in midpoint clearly below linear");
		//Bezier on the final segment still holds the end value at/after the last key
		int32_t mo4 = n25dModelMotionNew(m, "NMOTION_HOLD", 12);
		int32_t c4 = n25dModelMotionAddCurve(m, mo4, p);
		n25dModelMotionCurveAddKey(m, mo4, c4, 0.0f, 0.0f);
		n25dModelMotionCurveKeySetEase(m, mo4, c4, 0, (int32_t)N25EASE_BEZIER, 0.42f, 0.0f, 1.0f, 1.0f);
		n25dModelMotionCurveAddKey(m, mo4, c4, 1.0f, 10.0f);
		CHECK(n25dMotionCurveValueAt(m, mo4, c4, 1.0f) == 10.0f, "bezier: final segment holds end at last key");
		CHECK(n25dMotionCurveValueAt(m, mo4, c4, 2.0f) == 10.0f, "bezier: final segment holds end after last key");
		n25dModelFree(m);
	}
	// 7. Playback: SampleMotion sets target params (clamped) and recomputes
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, 0.0f, 1.0f);
		int32_t mo = n25dModelMotionNew(m, "NMOTION_TALK", 12);
		int32_t c = n25dModelMotionAddCurve(m, mo, p);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 2.0f); //above max
		n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 3.0f); //above max
		n25dModelSampleMotion(m, mo, 0.5f);
		CHECK(m->nParam[p].param_cur == 1.0f, "playback: sampled value clamped to max");
		n25dModelSampleMotion(m, 99, 0.5f); //bad motion id: no-op, no crash
		CHECK(m->nParam[p].param_cur == 1.0f, "playback: bad motion id is a no-op");
		n25dModelFree(m);
	}
	// 8. Playback: partial override (param without a curve untouched)
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f);
		int32_t p1 = n25dModelParamNew(m, "NPARAM_B", 8, 0.0f, -1.0f, 1.0f);
		n25dModelSetParam(m, p1, 0.42f); //pre-set, not in the motion
		int32_t mo = n25dModelMotionNew(m, "NMOTION_P0", 10);
		int32_t c = n25dModelMotionAddCurve(m, mo, p0);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 1.0f);
		n25dModelSampleMotion(m, mo, 0.75f);
		CHECK(m->nParam[p0].param_cur == 0.75f, "playback: p0 set by motion");
		CHECK(m->nParam[p1].param_cur == 0.42f, "playback: p1 untouched (partial override)");
		n25dModelFree(m);
	}
	// 9. Playback: recomputes transformed vertices (bound vertex moves)
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamVertexPos(m, 0, p, 1, 0, 10.0f, 0.0f); //x_hi[0] = 10
		int32_t mo = n25dModelMotionNew(m, "NMOTION_TALK", 12);
		int32_t c = n25dModelMotionAddCurve(m, mo, p);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, 2.0f, 1.0f);
		float ox0 = m->nPart[0].n25dMeshOrigin.x[0];
		n25dModelSampleMotion(m, mo, 1.0f); //value 0.5
		CHECK(m->nParam[p].param_cur == 0.5f, "playback: param at t=1 == 0.5");
		CHECK(m->nPart[0].n25dMeshTransform.x[0] == ox0 + 5.0f, "playback: vertex0 x += 5");
		n25dModelFree(m);
	}
	// 10. Save/load round-trip: names, curve counts, param ids, key times/values, easing
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f); //id 0
		int32_t p1 = n25dModelParamNew(m, "NPARAM_EYE_L", 12, 1.0f, 0.0f, 1.0f); //id 1
		int32_t ma = n25dModelMotionNew(m, "NMOTION_TALK", 12);
		int32_t ca0 = n25dModelMotionAddCurve(m, ma, p0);
		n25dModelMotionCurveAddKey(m, ma, ca0, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, ma, ca0, 1.0f, 0.7f);
		n25dModelMotionAddCurve(m, ma, p1);
		n25dModelMotionCurveAddKey(m, ma, 1, 0.5f, 1.0f);
		int32_t mb = n25dModelMotionNew(m, "NMOTION_BLINK", 13);
		int32_t cb = n25dModelMotionAddCurve(m, mb, p1);
		n25dModelMotionCurveAddKey(m, mb, cb, 0.0f, 0.0f);
		n25dModelMotionCurveKeySetEase(m, mb, cb, 0, (int32_t)N25EASE_BEZIER, 0.42f, 0.0f, 1.0f, 1.0f);
		n25dModelMotionCurveAddKey(m, mb, cb, 0.2f, 1.0f);
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		CHECK(buf != NULL, "rt: save ok");
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nMotionC == 2, "rt: 2 motions loaded");
		CHECK(l && n25dModelGetMotionID(l, "NMOTION_TALK", 12) == 0, "rt: talk id == 0");
		CHECK(l && n25dModelGetMotionID(l, "NMOTION_BLINK", 13) == 1, "rt: blink id == 1");
		CHECK(l && l->nMotion[0].curveC == 2, "rt: talk has 2 curves");
		CHECK(l && l->nMotion[0].curves[0].param_id == 0 && l->nMotion[0].curves[0].keyC == 2, "rt: talk c0 param+keys");
		CHECK(l && l->nMotion[0].curves[0].keys[1].time == 1.0f && l->nMotion[0].curves[0].keys[1].value == 0.7f, "rt: talk c0 key1 preserved");
		CHECK(l && l->nMotion[0].curves[0].keys[0].ease == (int32_t)N25EASE_LINEAR, "rt: linear easing preserved");
		CHECK(l && l->nMotion[0].curves[1].param_id == 1 && l->nMotion[0].curves[1].keyC == 1, "rt: talk c1 param+key");
		CHECK(l && l->nMotion[1].curveC == 1 && l->nMotion[1].curves[0].keyC == 2, "rt: blink curve+keys");
		CHECK(l && l->nMotion[1].curves[0].keys[0].ease == (int32_t)N25EASE_BEZIER &&
			l->nMotion[1].curves[0].keys[0].bz[0] == 0.42f && l->nMotion[1].curves[0].keys[0].bz[3] == 1.0f, "rt: bezier preserved");
		CHECK(l && l->nMotion[1].curves[0].keys[1].value == 1.0f, "rt: blink key1 value preserved");
		if (l) n25dModelFree(l);
		free(buf);
		n25dModelFree(m);
	}
	// 11. v2 file with 0 motions (just the ModelMotions marker) loads
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1); //1 param, 0 motions
		o = put32(buf, o, 12);         // n25FileT_EOF (no motion blocks)
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nMotionC == 0, "zeromotion: 0-motion file loads");
		CHECK(l && l->nParamC == 1, "zeromotion: param still loads");
		if (l) n25dModelFree(l);
	}
	// 12. Hand-crafted exact motion file loads with correct data
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1); //1 param
		o = put32(buf, o, 11);          // n25FileT_ModelMotion_next
		o = put32(buf, o, 12);          // name_len 12
		memcpy(buf + o, "NMOTION_TALK", 12); o += 12;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, 2);           // key_count 2
		o = putf32(buf, o, 0.0f);       // key0 time
		o = putf32(buf, o, 0.0f);       // key0 value
		o = put32(buf, o, 1);           // key0 ease = bezier
		o = putf32(buf, o, 0.42f);      // bz[0]
		o = putf32(buf, o, 0.0f);       // bz[1]
		o = putf32(buf, o, 0.58f);      // bz[2]
		o = putf32(buf, o, 1.0f);       // bz[3]
		o = putf32(buf, o, 1.0f);       // key1 time
		o = putf32(buf, o, 1.0f);       // key1 value
		o = put32(buf, o, 0);           // key1 ease = linear
		o = putf32(buf, o, 0.0f);       // bz[0]
		o = putf32(buf, o, 0.0f);       // bz[1]
		o = putf32(buf, o, 0.0f);       // bz[2]
		o = putf32(buf, o, 0.0f);       // bz[3]
		o = put32(buf, o, 12);          // n25FileT_EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nMotionC == 1, "handcraft: motion loaded");
		CHECK(l && l->nMotion[0].name_len == 12 && memcmp(l->nMotion[0].name, "NMOTION_TALK", 12) == 0, "handcraft: name preserved");
		CHECK(l && l->nMotion[0].curveC == 1 && l->nMotion[0].curves[0].param_id == 0 && l->nMotion[0].curves[0].keyC == 2, "handcraft: curve+keys preserved");
		CHECK(l && l->nMotion[0].curves[0].keys[0].ease == (int32_t)N25EASE_BEZIER &&
			l->nMotion[0].curves[0].keys[0].bz[0] == 0.42f && l->nMotion[0].curves[0].keys[0].bz[2] == 0.58f, "handcraft: bezier preserved");
		CHECK(l && n25dMotionGetDuration(l, 0) == 1.0f, "handcraft: duration == 1");
		//Sampling the loaded ease-in-out curve at the midpoint (symmetric -> 0.5)
		CHECK(l && fabsf(n25dMotionCurveValueAt(l, 0, 0, 0.5f) - 0.5f) < 1e-4f, "handcraft: sampled midpoint == 0.5");
		if (l) n25dModelFree(l);
	}
	// 13. curve_count too large (declared 4097, no data) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, (int32_t)N25D_FILE_MAX_MOTION_CURVES + 1); // curve_count 4097
		o = put32(buf, o, 12);          // EOF (no curve data)
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: curve_count too large -> NULL");
		if (l) n25dModelFree(l);
	}
	// 14. curve param_id out of range (1 param, curve references id 5) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 5);           // curve param_id 5 (only param 0 exists)
		o = put32(buf, o, 0);           // key_count 0
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: curve param_id out of range -> NULL");
		if (l) n25dModelFree(l);
	}
	// 15. Truncated keys (declares 2 keys, only 1 present) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, 2);           // key_count 2
		o = putf32(buf, o, 0.0f);       // key0 time
		o = putf32(buf, o, 0.0f);       // key0 value
		o = put32(buf, o, 0);           // key0 ease
		o = putf32(buf, o, 0.0f);       // bz[0]
		o = putf32(buf, o, 0.0f);       // bz[1]
		o = putf32(buf, o, 0.0f);       // bz[2]
		o = putf32(buf, o, 0.0f);       // bz[3]
		//key1 missing; buffer ends here
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: truncated keys -> NULL");
		if (l) n25dModelFree(l);
	}

	// 16. Key payload 4 bytes short PER KEY (the off-by-4-per-key gap).
	// A key is 28 bytes (time f32, value f32, ease i32, 4 bz f32). The loader's
	// upfront size guard must account for all 28; a buffer that only has 24/key
	// passes a 24/key guard but the key loop reads 28/key -> out of bounds.
	// key_count=2 with exactly 2*24=48 bytes of key data, then a valid EOF.
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, 2);           // key_count 2
		//Two keys but only 24 bytes each (drop one bz float each) = 48 bytes total
		for (int k = 0; k < 2; k++)
		{
			o = putf32(buf, o, (float)k); // time
			o = putf32(buf, o, 0.0f);     // value
			o = put32(buf, o, 0);         // ease
			o = putf32(buf, o, 0.0f);     // bz[0]
			o = putf32(buf, o, 0.0f);     // bz[1]
			o = putf32(buf, o, 0.0f);     // bz[2]  (bz[3] intentionally omitted)
		}
		o = put32(buf, o, 12);          // n25FileT_EOF
		n25dModel* l2 = n25dLoadFromMemory(buf, o);
		CHECK(l2 == NULL, "corrupt: key payload 4 bytes short per key -> NULL");
		if (l2) n25dModelFree(l2);
	}
	// 17. Motion name_len > MAX_MOTION_NAME (129) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 129);         // name_len 129 (> 128)
		memset(buf + o, 'X', 129); o += 129;
		o = put32(buf, o, 0);           // curve_count 0
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: motion name_len 129 -> NULL");
		if (l) n25dModelFree(l);
	}
	// 18. Non-decreasing-violating keys (time 1.0 then 0.5) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, 2);           // key_count 2
		o = putf32(buf, o, 1.0f);       // key0 time
		o = putf32(buf, o, 0.0f);       // key0 value
		o = put32(buf, o, 0);           // key0 ease
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = putf32(buf, o, 0.5f);       // key1 time (earlier than key0)
		o = putf32(buf, o, 1.0f);       // key1 value
		o = put32(buf, o, 0);           // key1 ease
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: non-decreasing-violating keys -> NULL");
		if (l) n25dModelFree(l);
	}
	// 19. Unknown ease tag (2) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, 1);           // key_count 1
		o = putf32(buf, o, 0.0f);       // key0 time
		o = putf32(buf, o, 0.0f);       // key0 value
		o = put32(buf, o, 2);           // key0 ease = unknown tag
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: unknown ease tag -> NULL");
		if (l) n25dModelFree(l);
	}
	// 20. Streaming bound: motion marker count over the file cap -> NULL
	{
		int8_t* buf = (int8_t*)malloc(65536);
		memset(buf, 0, 65536);
		int32_t o = write_motion_prefix(buf, 1);
		for (int i = 0; i <= (int)N25D_FILE_MAX_MOTIONS; i++) //4097 motions > cap 4096
		{
			o = put32(buf, o, 11);      // ModelMotion_next
			o = put32(buf, o, 0);       // name_len 0
			o = put32(buf, o, 0);       // curve_count 0
		}
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "streambound: 4097 motion markers -> NULL");
		if (l) n25dModelFree(l);
		free(buf);
	}
	// 21. Accept boundary: key_count == N25D_FILE_MAX_MOTION_KEYS (65536) loads
	{
		int8_t* buf = (int8_t*)malloc(2000000);
		memset(buf, 0, 2000000);
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 8);           // name_len 8
		memcpy(buf + o, "NPMOTION", 8); o += 8;
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // curve param_id 0
		o = put32(buf, o, (int32_t)N25D_FILE_MAX_MOTION_KEYS); // key_count 65536
		for (int i = 0; i < (int)N25D_FILE_MAX_MOTION_KEYS; i++)
		{
			o = putf32(buf, o, (float)i * 0.001f); // time (non-decreasing)
			o = putf32(buf, o, 0.5f);              // value
			o = put32(buf, o, 0);                  // ease linear
			o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
			o = putf32(buf, o, 0.0f); o = putf32(buf, o, 0.0f);
		}
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nMotionC == 1 && l->nMotion[0].curves[0].keyC == (int32_t)N25D_FILE_MAX_MOTION_KEYS,
			"boundary: 65536 keys loads");
		if (l) n25dModelFree(l);
		free(buf);
	}
	// 22. Empty-name motion (name_len==0) round-trips; name NULL; keys kept
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		int32_t mo = n25dModelMotionNew(m, NULL, 0);
		CHECK(mo == 0, "emptyname: created");
		int32_t c = n25dModelMotionAddCurve(m, mo, p);
		n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 0.6f);
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nMotionC == 1, "emptyname: loaded");
		CHECK(l && l->nMotion[0].name_len == 0 && l->nMotion[0].name == NULL, "emptyname: name NULL");
		CHECK(l && l->nMotion[0].curveC == 1 && l->nMotion[0].curves[0].keyC == 2 &&
			l->nMotion[0].curves[0].keys[1].value == 0.6f, "emptyname: keys kept");
		if (l) n25dModelFree(l);
		free(buf);
		n25dModelFree(m);
	}
	// 23. Grow path: >8 motions, a motion with >8 curves, a curve with >16 keys
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f); //0
		n25dModelParamNew(m, "NPARAM_B", 8, 0.0f, -1.0f, 1.0f); //1
		for (int i = 0; i < 10; i++) //10 motions > initial step 8
		{
			char nm[16]; int nl = snprintf(nm, sizeof nm, "NMOTION_M%d", i);
			int32_t mo = n25dModelMotionNew(m, nm, nl);
			if (i == 0)
			{
				for (int j = 0; j < 12; j++) //12 curves > initial step 8
				{
					int32_t c = n25dModelMotionAddCurve(m, mo, j % 2);
					if (j == 0)
						for (int k = 0; k < 20; k++) //20 keys > initial step 16
							n25dModelMotionCurveAddKey(m, mo, c, (float)k * 0.1f, 0.0f);
				}
			}
			else
			{
				int32_t c = n25dModelMotionAddCurve(m, mo, 0);
				n25dModelMotionCurveAddKey(m, mo, c, 0.0f, 0.0f);
				n25dModelMotionCurveAddKey(m, mo, c, 1.0f, 0.5f);
			}
		}
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nMotionC == 10, "grow: 10 motions loaded");
		CHECK(l && l->nMotion[0].curveC == 12, "grow: motion0 has 12 curves");
		CHECK(l && l->nMotion[0].curves[0].keyC == 20, "grow: curve0 has 20 keys");
		CHECK(l && l->nMotion[5].curveC == 1 && l->nMotion[5].curves[0].keys[1].value == 0.5f, "grow: motion5 preserved");
		if (l) n25dModelFree(l);
		free(buf);
		n25dModelFree(m);
	}

	// 24. Robustness: all-negative key times (duration = max key time) and NaN
	//     key-time rejection (authoring + load)
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f);
		//Every key time negative -> duration is the max key time (negative), not 0.0
		int32_t mo = n25dModelMotionNew(m, "NMOTION_NEG", 11);
		int32_t c = n25dModelMotionAddCurve(m, mo, p0);
		n25dModelMotionCurveAddKey(m, mo, c, -2.0f, 0.0f);
		n25dModelMotionCurveAddKey(m, mo, c, -0.5f, 1.0f);
		CHECK(n25dMotionGetDuration(m, mo) == -0.5f, "robust: all-negative key times -> duration = max key time (-0.5)");
		//NaN key time rejected at authoring
		int32_t mo2 = n25dModelMotionNew(m, "NMOTION_NAN", 11);
		int32_t c2 = n25dModelMotionAddCurve(m, mo2, p0);
		CHECK(n25dModelMotionCurveAddKey(m, mo2, c2, nanf(""), 0.5f) == -1, "robust: NaN key time rejected at authoring");
		//NaN key time rejected at load
		int8_t buf[256];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_motion_prefix(buf, 1);
		o = put32(buf, o, 11);          // ModelMotion_next
		o = put32(buf, o, 0);           // name_len 0
		o = put32(buf, o, 1);           // curve_count 1
		o = put32(buf, o, 0);           // param_id 0
		o = put32(buf, o, 1);           // key_count 1
		o = putf32(buf, o, nanf(""));   // key time = NaN
		o = putf32(buf, o, 0.5f);       // value
		o = put32(buf, o, 0);           // ease
		o = putf32(buf, o, 0.0f);       // bz[0]
		o = putf32(buf, o, 0.0f);       // bz[1]
		o = putf32(buf, o, 0.0f);       // bz[2]
		o = putf32(buf, o, 0.0f);       // bz[3]
		o = put32(buf, o, 12);          // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "robust: NaN key time rejected at load");
		if (l) n25dModelFree(l);
		n25dModelFree(m);
	}

	if (fails == 0)
	{
		printf("RESULT: ALL PASS\n");
		return 0;
	}
	printf("RESULT: %d FAILED\n", fails);
	return 1;
}
