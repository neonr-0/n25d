// Rotation-tree composition ORDER: a child's pivot rotation is PREPENDED to the
// parent's accumulated matrix, i.e. M_child = P_child * M_parent (P on the left).
// The pivots below are chosen so the two orders do NOT commute, which lets these
// checks catch a reversed (M_parent * P_child) composition. test_rotation_tree.c
// only uses an angle-0 child, where both orders agree, so this file pins the seam.
// Also covers depth 2 (grandchild) and a negative child angle.
#include <stdio.h>
#include <math.h>
#include "n25d.h"

static int check(const char* tag, n25dMesh* t, int v, float ex, float ey)
{
	int ok = fabsf(t->x[v] - ex) < 0.01f && fabsf(t->y[v] - ey) < 0.01f;
	printf("%s v%d=(%8.2f,%8.2f) want (%8.2f,%8.2f) %s\n", tag, v, t->x[v], t->y[v], ex, ey, ok ? "OK" : "MISMATCH");
	return ok;
}

//Rewrite all 6 origin vertices of a part to one point
static void setpts(n25dModel* m, int32_t part, float px, float py)
{
	n25dMesh* o = &m->nPart[part].n25dMeshOrigin;
	for (int i = 0; i < 6; i++)
	{
		o->x[i] = px;
		o->y[i] = py;
	}
}

int main(void)
{
	int ok = 1;

	//--- Scenario 1: depth 2, both +90 deg, DIFFERENT pivots (non-commuting) ---
	// part0 root       pivot (0,0)  +90 -> P0 = [ 0  1  0; -1  0  0]
	// part1 child      pivot (10,0) +90 -> P1 = [ 0  1 10; -1  0 10]
	//   M1 = P1*P0 = [-1  0 10; 0 -1 10]  i.e. (x,y) -> (10-x, 10-y)
	//   (reversed order P0*P1 would give (x,y) -> (10-x, -10-y))
	// part2 grandchild angle 0 -> M2 = M1
	{
		n25dModel* m = n25dModelNew(100, 100);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 1, 1);                   // part0
		int32_t c1 = n25dPartNew(m, n25dTexture_id(2), 0, 0, 1, 1);      // part1
		int32_t c2 = n25dPartNew(m, n25dTexture_id(3), 0, 0, 1, 1);      // part2
		n25dPartAddRotaryParent(m, c1, 0);   // part1 <- part0
		n25dPartAddRotaryParent(m, c2, c1);  // part2 <- part1
		n25dPartSetAnchor(m, 0, 0, 0);       // part0 pivot (0,0)
		n25dPartSetAnchor(m, c1, -10, 0);    // part1 pivot (10,0)
		n25dPartSetAnchor(m, c2, 0, 0);      // part2 angle 0 (pivot irrelevant)
		setpts(m, 0, 1, 0);    // part0 origin (1,0)
		setpts(m, c1, 0, 0);   // part1 origin (0,0)
		//part2: three distinct points to fully pin M1
		n25dMesh* o2 = &m->nPart[c2].n25dMeshOrigin;
		o2->x[0] = 1;  o2->y[0] = 2;    // -> (9, 8)
		o2->x[1] = 0;  o2->y[1] = 0;    // -> (10, 10)
		o2->x[2] = 10; o2->y[2] = 10;   // -> (0, 0)
		o2->x[3] = 1;  o2->y[3] = 2;
		o2->x[4] = 0;  o2->y[4] = 0;
		o2->x[5] = 10; o2->y[5] = 10;
		int32_t p = n25dModelParamNew(m, "NPARAM_ANGLE_X", 14, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamRotate(m, 0,  p, 1, (float)(M_PI / 2));   // part0 +90
		n25dPartSetParamRotate(m, c1, p, 1, (float)(M_PI / 2));   // part1 +90
		n25dModelTransformationBegin(m);
		n25dModelTransformation(m, p, 1.0f);
		n25dModelTransformationEnd(m);
		ok &= check("S1-part0", &m->nPart[0].n25dMeshTransform, 0, 0, -1);
		ok &= check("S1-part1", &m->nPart[c1].n25dMeshTransform, 0, 10, 10);
		ok &= check("S1-part2", &m->nPart[c2].n25dMeshTransform, 0, 9, 8);
		ok &= check("S1-part2", &m->nPart[c2].n25dMeshTransform, 1, 10, 10);
		ok &= check("S1-part2", &m->nPart[c2].n25dMeshTransform, 2, 0, 0);
		n25dModelFree(m);
	}

	//--- Scenario 2: NEGATIVE child angle, different pivot ---
	// part0 root  pivot (0,0)  +90 -> P0 = [ 0  1 0; -1  0 0]
	// part1 child pivot (0,10) -90 -> P1 = [ 0 -1 10; 1 0 10]
	//   M1 = P1*P0 = [1 0 10; 0 1 10]  i.e. (x,y) -> (x+10, y+10)
	//   (reversed order P0*P1 would give (x,y) -> (x+10, y-10))
	{
		n25dModel* m = n25dModelNew(100, 100);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 1, 1);                   // part0
		int32_t c1 = n25dPartNew(m, n25dTexture_id(2), 0, 0, 1, 1);      // part1
		n25dPartAddRotaryParent(m, c1, 0);
		n25dPartSetAnchor(m, 0, 0, 0);      // part0 pivot (0,0)
		n25dPartSetAnchor(m, c1, 0, 10);    // part1 pivot (0,10)
		setpts(m, 0, 1, 0);    // part0 origin (1,0)
		setpts(m, c1, 1, 2);   // part1 origin (1,2)
		int32_t p = n25dModelParamNew(m, "NPARAM_ANGLE_X", 14, 0.0f, -1.0f, 1.0f);
		n25dPartSetParamRotate(m, 0,  p, 1, (float)(M_PI / 2));   // part0 +90
		n25dPartSetParamRotate(m, c1, p, 1, (float)(-M_PI / 2));  // part1 -90
		n25dModelTransformationBegin(m);
		n25dModelTransformation(m, p, 1.0f);
		n25dModelTransformationEnd(m);
		ok &= check("S2-part0", &m->nPart[0].n25dMeshTransform, 0, 0, -1);
		ok &= check("S2-part1", &m->nPart[c1].n25dMeshTransform, 0, 11, 12);
		n25dModelFree(m);
	}

	printf("RESULT: %s\n", ok ? "ALL PASS" : "FAIL");
	return ok ? 0 : 1;
}
