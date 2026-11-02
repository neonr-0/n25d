// Affine pivot-rotate unit test (public API):
//  - a point at the effective center (-anchor_x, +anchor_y) maps to itself
//  - +90 deg maps relative (dx,dy) -> (dy,-dx); -90 deg maps (dx,dy) -> (-dy,dx)
// Covers both the hi (positive param) and low (negative param) rotation paths.
#include <stdio.h>
#include <math.h>
#include "n25d.h"

//Effective center for this test
#define CX 3.0f
#define CY (-2.0f)

static int check(const char* tag, float x, float y, float ex, float ey)
{
	int ok = fabsf(x - ex) < 0.01f && fabsf(y - ey) < 0.01f;
	printf("%s got (%8.3f, %8.3f) want (%8.3f, %8.3f) %s\n", tag, x, y, ex, ey, ok ? "OK" : "MISMATCH");
	return ok;
}

int main(void)
{
	//Three test points, one of them exactly at the pivot:
	//  (3,-2)  at center, relative (0,0)
	//  (4,0)   relative (1,2)
	//  (-1,5)  relative (-4,7)
	const float pts[3][2] = { { CX, CY }, { 4.0f, 0.0f }, { -1.0f, 5.0f } };
	int ok = 1;

	for (int pass = 0; pass < 2; pass++)
	{
		const char* tag = (pass == 0) ? "ROT+90" : "ROT-90";
		n25dModel* m = n25dModelNew(100, 100);
		n25dPartNew(m, n25dTexture_id(1), 0, 0, 1, 1);
		//anchor = (-cx, cy) so effective center = (-anchor_x, +anchor_y) = (CX, CY)
		n25dPartSetAnchor(m, 0, -CX, CY);
		//Rewrite origin vertices to the test points (only rotation is applied)
		n25dMesh* o = &m->nPart[0].n25dMeshOrigin;
		for (int i = 0; i < 6; i++)
		{
			o->x[i] = pts[i % 3][0];
			o->y[i] = pts[i % 3][1];
		}
		int32_t p = n25dModelParamNew(m, "NPARAM_ANGLE_X", 14, 0.0f, -1.0f, 1.0f);
		if (pass == 0)
		{
			//hi path: f_param = +1, tangle = angle_hi * +1
			n25dPartSetParamRotate(m, 0, p, 1, (float)(M_PI / 2));
			n25dModelTransformationBegin(m);
			n25dModelTransformation(m, p, 1.0f);
		}
		else
		{
			//low path: f_param = -1, tangle = angle_low * -1 = -90 deg
			n25dPartSetParamRotate(m, 0, p, -1, (float)(M_PI / 2));
			n25dModelTransformationBegin(m);
			n25dModelTransformation(m, p, -1.0f);
		}
		n25dModelTransformationEnd(m);
		n25dMesh* t = &m->nPart[0].n25dMeshTransform;
		for (int i = 0; i < 6; i++)
		{
			float dx = pts[i % 3][0] - CX;
			float dy = pts[i % 3][1] - CY;
			float ex, ey;
			if (pass == 0) { ex = CX + dy; ey = CY - dx; }      // +90: (dx,dy)->(dy,-dx)
			else           { ex = CX - dy; ey = CY + dx; }      // -90: (dx,dy)->(-dy,dx)
			ok &= check(tag, t->x[i], t->y[i], ex, ey);
		}
		n25dModelFree(m);
	}
	printf("RESULT: %s\n", ok ? "ALL PASS" : "FAIL");
	return ok ? 0 : 1;
}
