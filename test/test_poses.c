// Pose tests: create/lookup, apply (partial override + clamp + vertex drive),
// save/load round-trip, header pose count, and corrupt/edge load rejection.
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

// Write: header + ModelName(len0) + ModelParams + nparams param blocks
// (NPARAM_0..NPARAM_n-1) + ModelParts(0 parts) + ModelPoses marker.
// header_pose_count is what the header claims (for cross-check tests).
// Returns the offset immediately after the ModelPoses marker.
static int32_t write_pose_prefix(int8_t* buf, int nparams, int header_pose_count)
{
	int32_t o = 0;
	o = put32(buf, o, 0xFE0D2525); // signature
	o = put32(buf, o, 2);          // version 2
	o = put32(buf, o, 0);          // part count 0
	o = put32(buf, o, nparams);    // param count
	o = put32(buf, o, header_pose_count); // pose count
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
	o = put32(buf, o, 6);          // n25FileT_ModelPoses
	return o;
}

int main(void)
{
	// 1. Pose create + add + GetPoseID (found / unknown / len0 / NULL)
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		int32_t p1 = n25dModelParamNew(m, "NPARAM_EYE_L", 12, 1.0f, 0.0f, 1.0f);
		int32_t pose = n25dModelPoseNew(m, "NPOSE_HAPPY", 12);
		CHECK(pose == 0, "create: pose id == 0");
		CHECK(n25dModelPoseAddParam(m, pose, p0, 0.7f) == 0, "add: mouth entry ok");
		CHECK(n25dModelPoseAddParam(m, pose, p1, 0.3f) == 0, "add: eye entry ok");
		CHECK(m->nPoseC == 1 && m->nPose[0].entryC == 2, "create: 1 pose, 2 entries");
		CHECK(n25dModelGetPoseID(m, "NPOSE_HAPPY", 12) == 0, "id: found == 0");
		CHECK(n25dModelGetPoseID(m, "NPOSE_NOPE", 10) == -1, "id: unknown == -1");
		CHECK(n25dModelGetPoseID(m, "NPOSE_HAPPY", 0) == -1, "id: name_len 0 == -1");
		CHECK(n25dModelGetPoseID(m, NULL, 12) == -1, "id: NULL name == -1");
		n25dModelFree(m);
	}
	// 2. AddParam validation: bad pose id, bad param id, NULL model
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		int32_t pose = n25dModelPoseNew(m, "NPOSE_A", 8);
		CHECK(n25dModelPoseAddParam(m, 99, p0, 0.5f) == -1, "add: bad pose id == -1");
		CHECK(n25dModelPoseAddParam(m, pose, 99, 0.5f) == -1, "add: bad param id == -1");
		CHECK(n25dModelPoseAddParam(NULL, pose, p0, 0.5f) == -1, "add: NULL model == -1");
		CHECK(m->nPose[0].entryC == 0, "add: failed adds not stored");
		n25dModelFree(m);
	}
	// 3. ApplyPose sets the target params and recomputes (vertex moves)
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamVertexPos(m, 0, p, 1, 0, 10.0f, 0.0f); // x_hi[0] = 10
		int32_t pose = n25dModelPoseNew(m, "NPOSE_TALK", 10);
		n25dModelPoseAddParam(m, pose, p, 1.0f);
		float ox0 = m->nPart[0].n25dMeshOrigin.x[0];
		n25dModelApplyPose(m, pose);
		CHECK(m->nParam[p].param_cur == 1.0f, "apply: param set to pose value");
		CHECK(m->nPart[0].n25dMeshTransform.x[0] == ox0 + 10.0f, "apply: vertex0 x += 10");
		n25dModelFree(m);
	}
	// 4. Partial override: pose sets only p0; p1 keeps its prior value
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p0 = n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f);
		int32_t p1 = n25dModelParamNew(m, "NPARAM_B", 8, 0.0f, -1.0f, 1.0f);
		n25dModelSetParam(m, p1, 0.42f); // pre-set, not in pose
		int32_t pose = n25dModelPoseNew(m, "NPOSE_P0", 8);
		n25dModelPoseAddParam(m, pose, p0, 0.9f);
		n25dModelApplyPose(m, pose);
		CHECK(m->nParam[p0].param_cur == 0.9f, "partial: p0 set by pose");
		CHECK(m->nParam[p1].param_cur == 0.42f, "partial: p1 untouched");
		n25dModelFree(m);
	}
	// 5. ApplyPose clamps to [min,max]
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		int32_t pose = n25dModelPoseNew(m, "NPOSE_BIG", 9);
		n25dModelPoseAddParam(m, pose, p, 5.0f); // > max
		n25dModelApplyPose(m, pose);
		CHECK(m->nParam[p].param_cur == 1.0f, "clamp: pose value clamped to max");
		n25dModelFree(m);
	}
	// 6. ApplyPose: empty pose (0 entries) is a no-op on params, no crash
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		n25dModelSetParam(m, p, 0.5f);
		int32_t pose = n25dModelPoseNew(m, "NPOSE_EMPTY", 11);
		n25dModelApplyPose(m, pose);
		CHECK(m->nParam[p].param_cur == 0.5f, "empty pose: param untouched");
		n25dModelApplyPose(m, 99); // bad pose id, no crash
		CHECK(1, "empty pose: bad id no crash");
		n25dModelFree(m);
	}
	// 7. Name cap: 129-byte pose name rejected at creation; 128 accepted
	{
		n25dModel* m = n25dModelNew(10, 10);
		char big[129]; memset(big, 'A', 128); big[128] = 0;
		CHECK(n25dModelPoseNew(m, big, 129) == -1, "cap: 129-byte name rejected");
		char ok[129]; memset(ok, 'B', 128); ok[128] = 0;
		CHECK(n25dModelPoseNew(m, ok, 128) == 0, "cap: 128-byte name accepted");
		n25dModelFree(m);
	}
	// 8. Save/load round-trip: names, entry counts, param ids, values, header count
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f); // id 0
		n25dModelParamNew(m, "NPARAM_EYE_L", 12, 1.0f, 0.0f, 1.0f); // id 1
		int32_t pa = n25dModelPoseNew(m, "NPOSE_HAPPY", 12);
		n25dModelPoseAddParam(m, pa, 0, 0.7f);
		n25dModelPoseAddParam(m, pa, 1, 0.3f);
		int32_t pb = n25dModelPoseNew(m, "NPOSE_EMPTY", 11); // 0 entries
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		CHECK(buf != NULL, "rt: save ok");
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nPoseC == 2, "rt: 2 poses loaded");
		CHECK(l && l->nParamC == 2 && l->nParam[0].param_cur == 0.0f, "rt: params loaded at default");
		CHECK(l && n25dModelGetPoseID(l, "NPOSE_HAPPY", 12) == 0, "rt: happy id == 0");
		CHECK(l && n25dModelGetPoseID(l, "NPOSE_EMPTY", 11) == 1, "rt: empty id == 1");
		CHECK(l && l->nPose[0].entryC == 2, "rt: happy has 2 entries");
		CHECK(l && l->nPose[0].entries[0].param_id == 0 && l->nPose[0].entries[0].value == 0.7f, "rt: happy[0] mouth=0.7");
		CHECK(l && l->nPose[0].entries[1].param_id == 1 && l->nPose[0].entries[1].value == 0.3f, "rt: happy[1] eye=0.3");
		CHECK(l && l->nPose[1].entryC == 0, "rt: empty pose has 0 entries");
		if (l) n25dModelFree(l);
		free(buf);
		n25dModelFree(m);
	}
	// 9. ApplyPose on a loaded model drives a bound vertex
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_MOUTH", 12, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamVertexPos(m, 0, p, 1, 0, 10.0f, 0.0f); // x_hi[0] = 10
		int32_t pose = n25dModelPoseNew(m, "NPOSE_TALK", 10);
		n25dModelPoseAddParam(m, pose, p, 1.0f);
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l != NULL, "loaded: ok");
		if (l)
		{
			int32_t lp = n25dModelGetParamID(l, "NPARAM_MOUTH", 12);
			int32_t lpose = n25dModelGetPoseID(l, "NPOSE_TALK", 10);
			float ox0 = l->nPart[0].n25dMeshOrigin.x[0];
			n25dModelApplyPose(l, lpose);
			CHECK(l->nParam[lp].param_cur == 1.0f, "loaded: param set");
			CHECK(l->nPart[0].n25dMeshTransform.x[0] == ox0 + 10.0f, "loaded: vertex0 x += 10");
			n25dModelFree(l);
		}
		free(buf);
		n25dModelFree(m);
	}
	// 10. v2 file with 0 poses (just the ModelPoses marker) loads with nPoseC == 0
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 0); // 1 param, header claims 0 poses
		o = put32(buf, o, 12);         // n25FileT_EOF (no pose blocks)
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nPoseC == 0, "zeropose: 0-pose file loads");
		CHECK(l && l->nParamC == 1, "zeropose: param still loads");
		if (l) n25dModelFree(l);
	}
	// 11. Hand-crafted valid pose file (exact) loads with correct data
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 1); // 1 param, header claims 1 pose
		o = put32(buf, o, 7);          // n25FileT_ModelPose_next
		o = put32(buf, o, 8);          // name_len 8
		memcpy(buf + o, "NPOSE_OK", 8); o += 8;
		o = put32(buf, o, 1);          // entry_count 1
		o = put32(buf, o, 0);          // entry param_id 0
		o = putf32(buf, o, 0.25f);     // entry value 0.25
		o = put32(buf, o, 12);         // n25FileT_EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nPoseC == 1, "handcraft: pose loaded");
		CHECK(l && l->nPose[0].name_len == 8 && memcmp(l->nPose[0].name, "NPOSE_OK", 8) == 0, "handcraft: name preserved");
		CHECK(l && l->nPose[0].entryC == 1 && l->nPose[0].entries[0].param_id == 0 && l->nPose[0].entries[0].value == 0.25f, "handcraft: entry preserved");
		if (l) n25dModelFree(l);
	}
	// 12. entry_count too large (declared 4097, no data) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 1);
		o = put32(buf, o, 7);          // ModelPose_next
		o = put32(buf, o, 8);          // name_len 8
		memcpy(buf + o, "NPOSE_BIG", 7); o += 7;
		// pad name to 8 bytes
		o += 1;
		o = put32(buf, o, N25D_FILE_MAX_POSE_ENTRIES + 1); // entry_count 4097
		o = put32(buf, o, 12);         // EOF (no entry data)
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: entry_count too large -> NULL");
		if (l) n25dModelFree(l);
	}
	// 13. entry param_id out of range (1 param, pose references id 5) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 1);
		o = put32(buf, o, 7);          // ModelPose_next
		o = put32(buf, o, 8);          // name_len 8
		memcpy(buf + o, "NPOSE_BAD", 7); o += 7; o += 1;
		o = put32(buf, o, 1);          // entry_count 1
		o = put32(buf, o, 5);          // entry param_id 5 (only param 0 exists)
		o = putf32(buf, o, 0.5f);
		o = put32(buf, o, 12);         // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: pose param_id out of range -> NULL");
		if (l) n25dModelFree(l);
	}
	// 14. Truncated pose block (declares 2 entries, only 1 present) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 1);
		o = put32(buf, o, 7);          // ModelPose_next
		o = put32(buf, o, 8);          // name_len 8
		memcpy(buf + o, "NPOSE_TRUNC", 7); o += 7; o += 1;
		o = put32(buf, o, 2);          // entry_count 2
		o = put32(buf, o, 0);          // entry 0 param_id
		o = putf32(buf, o, 0.5f);      // entry 0 value
		// entry 1 missing; buffer ends here
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: truncated pose entries -> NULL");
		if (l) n25dModelFree(l);
	}
	// 15. Pose name_len > MAX_POSE_NAME (129) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 1);
		o = put32(buf, o, 7);          // ModelPose_next
		o = put32(buf, o, 129);        // name_len 129 (> 128)
		memset(buf + o, 'X', 129); o += 129;
		o = put32(buf, o, 0);          // entry_count 0
		o = put32(buf, o, 12);         // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: pose name_len 129 -> NULL");
		if (l) n25dModelFree(l);
	}
	// 16. Header pose count mismatch (header says 2, stream has 1) -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 2); // header claims 2 poses
		o = put32(buf, o, 7);          // ModelPose_next (only 1 pose)
		o = put32(buf, o, 8);
		memcpy(buf + o, "NPOSE_ONE", 7); o += 7; o += 1;
		o = put32(buf, o, 0);          // entry_count 0
		o = put32(buf, o, 12);         // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "corrupt: header pose count mismatch -> NULL");
		if (l) n25dModelFree(l);
	}
	// 17. Streaming bound: header pose count 0 but a pose marker present -> NULL
	{
		int8_t buf[512];
		memset(buf, 0, sizeof(buf));
		int32_t o = write_pose_prefix(buf, 1, 0); // header claims 0 poses
		o = put32(buf, o, 7);          // ModelPose_next (unexpected)
		o = put32(buf, o, 8);
		memcpy(buf + o, "NPOSE_XTR", 7); o += 7; o += 1;
		o = put32(buf, o, 0);          // entry_count 0
		o = put32(buf, o, 12);         // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l == NULL, "streambound: header 0 poses but pose marker -> NULL");
		if (l) n25dModelFree(l);
	}
	// 18. Accept boundary: entry_count == N25D_FILE_MAX_POSE_ENTRIES (4096) loads
	{
		int8_t* buf = (int8_t*)malloc(65536);
		memset(buf, 0, 65536);
		int32_t o = write_pose_prefix(buf, 1, 1); // 1 param, header claims 1 pose
		o = put32(buf, o, 7);          // ModelPose_next
		o = put32(buf, o, 8);          // name_len 8
		memcpy(buf + o, "NPOSEMAX", 8); o += 8;
		o = put32(buf, o, N25D_FILE_MAX_POSE_ENTRIES); // entry_count 4096
		for (int i = 0; i < N25D_FILE_MAX_POSE_ENTRIES; i++)
		{
			o = put32(buf, o, 0);     // param_id 0
			o = putf32(buf, o, 0.5f); // value
		}
		o = put32(buf, o, 12);         // EOF
		n25dModel* l = n25dLoadFromMemory(buf, o);
		CHECK(l && l->nPoseC == 1 && l->nPose[0].entryC == N25D_FILE_MAX_POSE_ENTRIES, "boundary: 4096 entries loads");
		if (l) n25dModelFree(l);
		free(buf);
	}
	// 19. Empty-name pose (name_len==0) round-trips; name NULL; entry preserved
	{
		n25dModel* m = n25dModelNew(10, 10);
		int32_t p = n25dModelParamNew(m, "NPARAM_T", 8, 0.0f, -1.0f, 1.0f);
		int32_t pose = n25dModelPoseNew(m, NULL, 0);
		CHECK(pose == 0, "emptyname: created");
		n25dModelPoseAddParam(m, pose, p, 0.6f);
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nPoseC == 1, "emptyname: loaded");
		CHECK(l && l->nPose[0].name_len == 0 && l->nPose[0].name == NULL, "emptyname: name NULL");
		CHECK(l && l->nPose[0].entryC == 1 && l->nPose[0].entries[0].param_id == p && l->nPose[0].entries[0].value == 0.6f, "emptyname: entry preserved");
		if (l) n25dModelFree(l);
		free(buf);
		n25dModelFree(m);
	}
	// 20. Grow path: >8 poses (nPose realloc) + a pose with >8 entries (entries realloc)
	{
		n25dModel* m = n25dModelNew(10, 10);
		n25dModelParamNew(m, "NPARAM_A", 8, 0.0f, -1.0f, 1.0f); // 0
		n25dModelParamNew(m, "NPARAM_B", 8, 0.0f, -1.0f, 1.0f); // 1
		n25dModelParamNew(m, "NPARAM_C", 8, 0.0f, -1.0f, 1.0f); // 2
		for (int i = 0; i < 10; i++) // 10 poses > initial step 8
		{
			char nm[16]; int nl = snprintf(nm, sizeof nm, "NPOSE_P%d", i);
			int32_t pose = n25dModelPoseNew(m, nm, nl);
			if (i == 0)
				for (int j = 0; j < 12; j++) // 12 entries > initial step 8
					n25dModelPoseAddParam(m, pose, j % 3, 0.1f * j);
			else
				n25dModelPoseAddParam(m, pose, 0, 0.5f);
		}
		int32_t len = 0;
		int8_t* buf = n25dSaveToMemory(m, &len);
		n25dModel* l = n25dLoadFromMemory(buf, len);
		CHECK(l && l->nPoseC == 10, "grow: 10 poses loaded");
		CHECK(l && l->nPose[0].entryC == 12, "grow: pose0 has 12 entries");
		CHECK(l && l->nPose[5].entryC == 1 && l->nPose[5].entries[0].value == 0.5f, "grow: pose5 has 1 entry");
		if (l) n25dModelFree(l);
		free(buf);
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
