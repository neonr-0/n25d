/*
// 2019 November 2
//
// The MIT License
//
// Copyright (c) 2019 NEON Software Solutions. http://neonsoftsol.com
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
*/
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "n25d.h"

//Macros
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)			(((a) > (b)) ? (a) : (b))
#endif

//Definitions
n25dModel* n25dModelInit(n25dModel* p_n25dmodel, float width, float height);
void n25dMeshNew(n25dPart* p_n25dPart, n25dMesh* p_n25Mesh, float x, float y, float width, float height);
void n25dPartParamTransitionTransform(n25dPart* pPart, int32_t ParamID, float f_param);
void n25dPartParamRotationTransform(n25dPart* pPart, n25dAffine* mRotation, float f_angle);

//Internal 2D affine helpers (file-local).
//Convention: n25dAffineMul(out, a, b) computes out = a * b, meaning b is applied
//first, then a, so (a*b)*p == a*(b*p).
static void n25dAffineSetIdentity(n25dAffine* a)
{
	a->m00 = 1.0f; a->m01 = 0.0f; a->m02 = 0.0f;
	a->m10 = 0.0f; a->m11 = 1.0f; a->m12 = 0.0f;
}
static void n25dAffineSetPivotRotate(n25dAffine* a, float cx, float cy, float angle)
{
	//Rotation about pivot (cx,cy) by angle (radians), rotation about the -Z axis:
	//a relative vector (dx,dy) maps to (dx*cos+dy*sin, -dx*sin+dy*cos), so +90 deg
	//maps (dx,dy) -> (dy,-dx). A point at the pivot maps to itself.
	float s = sinf(angle);
	float c = cosf(angle);
	a->m00 = c;   a->m01 = s;  a->m02 = cx*(1.0f - c) - cy*s;
	a->m10 = -s;  a->m11 = c;  a->m12 = cx*s + cy*(1.0f - c);
}
static void n25dAffineMul(n25dAffine* out, const n25dAffine* a, const n25dAffine* b)
{
	n25dAffine r;
	r.m00 = a->m00*b->m00 + a->m01*b->m10;
	r.m01 = a->m00*b->m01 + a->m01*b->m11;
	r.m02 = a->m00*b->m02 + a->m01*b->m12 + a->m02;
	r.m10 = a->m10*b->m00 + a->m11*b->m10;
	r.m11 = a->m10*b->m01 + a->m11*b->m11;
	r.m12 = a->m10*b->m02 + a->m11*b->m12 + a->m12;
	*out = r;
}
static void n25dAffineTransformPoint(const n25dAffine* a, float x, float y, float* out_x, float* out_y)
{
	*out_x = a->m00*x + a->m01*y + a->m02;
	*out_y = a->m10*x + a->m11*y + a->m12;
}

n25dModel* n25dModelInit(n25dModel* p_n25dmodel, float width, float height)
{
	p_n25dmodel->nPartCount_max = (int32_t)0;
	p_n25dmodel->nPartCount = (int32_t)0;
	p_n25dmodel->nPart = (void*)0;
	p_n25dmodel->x = (float)0.0f;
	p_n25dmodel->y = (float)0.0f;
	p_n25dmodel->z = (float)0.0f;
	p_n25dmodel->width = width;
	p_n25dmodel->height = height;
	//params new
	p_n25dmodel->nParam = (void*)0;
	p_n25dmodel->nParamC = (int32_t)0;
	p_n25dmodel->nParamCMAX = (int32_t)0;
	p_n25dmodel->nModelName = (void*)0;
	p_n25dmodel->nModelNameLen = (int32_t)0;
	//poses
	p_n25dmodel->nPose = (void*)0;
	p_n25dmodel->nPoseC = (int32_t)0;
	p_n25dmodel->nPoseCMAX = (int32_t)0;
	//motions
	p_n25dmodel->nMotion = (void*)0;
	p_n25dmodel->nMotionC = (int32_t)0;
	p_n25dmodel->nMotionCMAX = (int32_t)0;
	return p_n25dmodel;
}
n25dModel* n25dModelNew(float width, float height)
{
	n25dModel* p_n25dmodel = (n25dModel*)malloc(sizeof(n25dModel));
	if (p_n25dmodel == NULL)
		return NULL;
	return n25dModelInit(p_n25dmodel, width, height);
}
void n25dModelSetName(n25dModel* p_n25dmodel, char* charA, int32_t charLen)
{
	if (p_n25dmodel == NULL || charLen < 0 || (charLen > 0 && charA == NULL))
		return;
	free(p_n25dmodel->nModelName);
	if (charLen == 0)
	{
		p_n25dmodel->nModelName = NULL;
		p_n25dmodel->nModelNameLen = (int32_t)0;
		return;
	}
	//+1 for the null terminator
	p_n25dmodel->nModelName = (char*)malloc((size_t)charLen + 1);
	if (p_n25dmodel->nModelName == NULL)
	{
		p_n25dmodel->nModelNameLen = (int32_t)0;
		return;
	}
	memcpy(p_n25dmodel->nModelName, charA, (size_t)charLen);
	p_n25dmodel->nModelName[charLen] = '\0';
	p_n25dmodel->nModelNameLen = charLen;
}
//Parts section
int32_t n25dPartNew(n25dModel* p_n25dModel, n25dTexture in_texture, float x, float y, float width, float height)
{
	if (p_n25dModel->nPart == NULL)//allocate memory
		p_n25dModel->nPart = n25dMemoryDynHelper(p_n25dModel->nPart, sizeof(n25dPart),
			&p_n25dModel->nPartCount_max, MAX_ALLOC_N25D_PARTS);
	if (p_n25dModel->nPartCount == p_n25dModel->nPartCount_max)//Allocate more memory if need
		p_n25dModel->nPart = n25dMemoryDynHelper(p_n25dModel->nPart, sizeof(n25dPart),
			&p_n25dModel->nPartCount_max, MAX_ALLOC_N25D_PARTS);

	assert(p_n25dModel->nPart != NULL);//Part is not initialized or out of memory
	if (p_n25dModel->nPart == NULL) return -1;//release check: negative return means out of memory
	assert(p_n25dModel->nPartCount < p_n25dModel->nPartCount_max); //Part counter overflow

	p_n25dModel->nPart[p_n25dModel->nPartCount].DrawOrder = 0; // default
	p_n25dModel->nPart[p_n25dModel->nPartCount].TextureID = in_texture; // not assigned
	p_n25dModel->nPart[p_n25dModel->nPartCount].pointcount = 6; //default 2 triangle sprite
	p_n25dModel->nPart[p_n25dModel->nPartCount].x = p_n25dModel->x + x; //relative
	p_n25dModel->nPart[p_n25dModel->nPartCount].y = p_n25dModel->y + y; //relative
	p_n25dModel->nPart[p_n25dModel->nPartCount].z = 0.0f; // for 2d always zero (don't need use any zbuffer?)
	p_n25dModel->nPart[p_n25dModel->nPartCount].width = width;
	p_n25dModel->nPart[p_n25dModel->nPartCount].height = height;
	p_n25dModel->nPart[p_n25dModel->nPartCount].anchor_x = 0.0f; // anchor point for rotation
	p_n25dModel->nPart[p_n25dModel->nPartCount].anchor_y = 0.0f;
	p_n25dModel->nPart[p_n25dModel->nPartCount].angle = 0.0f;
	p_n25dModel->nPart[p_n25dModel->nPartCount].ParentPartID = 0; //also used for rotation tree
	p_n25dModel->nPart[p_n25dModel->nPartCount].ParentPartIDC = 0;
	n25dAffineSetIdentity(&p_n25dModel->nPart[p_n25dModel->nPartCount].RotationMatrix);//identity matrix for rotation tree
	//Params
	p_n25dModel->nPart[p_n25dModel->nPartCount].n25dMeshParam = NULL;
	p_n25dModel->nPart[p_n25dModel->nPartCount].n25dMeshParamC = 0;
	p_n25dModel->nPart[p_n25dModel->nPartCount].n25dMeshParamCMAX = 0;
	//Init Origin mesh
	n25dMeshNew(&p_n25dModel->nPart[p_n25dModel->nPartCount], &p_n25dModel->nPart[p_n25dModel->nPartCount].n25dMeshOrigin,
		p_n25dModel->nPart[p_n25dModel->nPartCount].x, p_n25dModel->nPart[p_n25dModel->nPartCount].y,
		p_n25dModel->nPart[p_n25dModel->nPartCount].width, p_n25dModel->nPart[p_n25dModel->nPartCount].height);
	//Init Transformation mesh
	n25dMeshNew(&p_n25dModel->nPart[p_n25dModel->nPartCount], &p_n25dModel->nPart[p_n25dModel->nPartCount].n25dMeshTransform,
		p_n25dModel->nPart[p_n25dModel->nPartCount].x, p_n25dModel->nPart[p_n25dModel->nPartCount].y,
		p_n25dModel->nPart[p_n25dModel->nPartCount].width, p_n25dModel->nPart[p_n25dModel->nPartCount].height);

	p_n25dModel->nPartCount++;
	return p_n25dModel->nPartCount - 1;
}
void n25dPartSetUVTexture(n25dModel* p_n25dModel, int32_t n25dPartID, float u, float v, float u_end, float v_end)
{
	//
	//u = (x - a) / w and v = (z - b) / h 
	//maybe?

	assert(p_n25dModel->nPartCount > n25dPartID); //Heap corrupted or wrong ID
	//setup mesh on default points (sprite -> 2 triangle)
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.u = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.v = v;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.u_end = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.v_end = v_end;
	//1 1 triangle
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[0] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[0] = v;
	//2
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[1] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[1] = v;
	//3
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[2] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[2] = v_end;
	//4 2 triangle
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[3] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[3] = v_end;
	//5 
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[4] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[4] = v;
	//6
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tu[5] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshTransform.tv[5] = v_end;
	//Set also origin mesh
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.u = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.v = v;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.u_end = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.v_end = v_end;
	//1 1 triangle
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[0] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[0] = v;
	//2
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[1] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[1] = v;
	//3
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[2] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[2] = v_end;
	//4 2 triangle
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[3] = u;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[3] = v_end;
	//5 
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[4] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[4] = v;
	//6
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tu[5] = u_end;
	p_n25dModel->nPart[n25dPartID].n25dMeshOrigin.tv[5] = v_end;
}
//Mesh section
void n25dMeshNew(n25dPart* p_n25dPart, n25dMesh* p_n25Mesh, float x, float y, float width, float height)
{
	//two triangle
	p_n25Mesh->pointcount = p_n25dPart->pointcount;
	p_n25Mesh->TextureID = p_n25dPart->TextureID;
	p_n25Mesh->DrawOrder = p_n25dPart->DrawOrder;
	p_n25Mesh->u = 0.0f;
	p_n25Mesh->v = 0.0f;
	p_n25Mesh->u_end = 1.0f;
	p_n25Mesh->v_end = 1.0f;
	p_n25Mesh->texture_param[0] = 0.0f;
	p_n25Mesh->texture_param[1] = 0.0f;
	p_n25Mesh->texture_param[2] = 0.0f;
	p_n25Mesh->texture_param[3] = 0.0f;
	//allocate two triangle
	p_n25Mesh->x = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->y = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->z = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->r = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->g = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->b = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->a = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->tu = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	p_n25Mesh->tv = (float*)malloc(p_n25Mesh->pointcount * sizeof(float));
	//out of memory check (the default box writes below assume 6 allocated vertices)
	if (p_n25Mesh->x == NULL || p_n25Mesh->y == NULL || p_n25Mesh->z == NULL ||
		p_n25Mesh->r == NULL || p_n25Mesh->g == NULL || p_n25Mesh->b == NULL ||
		p_n25Mesh->a == NULL || p_n25Mesh->tu == NULL || p_n25Mesh->tv == NULL ||
		p_n25Mesh->pointcount < 6)
		return;
	//default box sprite
	for (int i = 0; i < p_n25Mesh->pointcount; i++)
	{
		p_n25Mesh->r[i] = 1.0f;
		p_n25Mesh->g[i] = 1.0f;
		p_n25Mesh->b[i] = 1.0f;
		p_n25Mesh->a[i] = 1.0f;
	}
	//1 1 triangle
	p_n25Mesh->x[0] = x;
	p_n25Mesh->y[0] = y + height;
	p_n25Mesh->z[0] = 0.0f;
	p_n25Mesh->tu[0] = 0.0f;
	p_n25Mesh->tv[0] = 0.0f;
	//2
	p_n25Mesh->x[1] = x + width;
	p_n25Mesh->y[1] = y + height;
	p_n25Mesh->z[1] = 0.0f;
	p_n25Mesh->tu[1] = 1.0f;
	p_n25Mesh->tv[1] = 0.0f;
	//3
	p_n25Mesh->x[2] = x;
	p_n25Mesh->y[2] = y;
	p_n25Mesh->z[2] = 0.0f;
	p_n25Mesh->tu[2] = 0.0f;
	p_n25Mesh->tv[2] = 1.0f;
	//4 2 triangle
	p_n25Mesh->x[3] = x;
	p_n25Mesh->y[3] = y;
	p_n25Mesh->z[3] = 0.0f;
	p_n25Mesh->tu[3] = 0.0f;
	p_n25Mesh->tv[3] = 1.0f;
	//5 
	p_n25Mesh->x[4] = x + width;
	p_n25Mesh->y[4] = y + height;
	p_n25Mesh->z[4] = 0.0f;
	p_n25Mesh->tu[4] = 1.0f;
	p_n25Mesh->tv[4] = 0.0f;
	//6
	p_n25Mesh->x[5] = x + width;
	p_n25Mesh->y[5] = y;
	p_n25Mesh->z[5] = 0.0f;
	p_n25Mesh->tu[5] = 1.0f;
	p_n25Mesh->tv[5] = 1.0f;
}
void n25dMeshCopy(n25dMesh* p_n25dMeshIn, n25dMesh* p_n25dMeshOut)
{	
	assert(p_n25dMeshIn->pointcount == p_n25dMeshOut->pointcount);//It's two identical meshes
	p_n25dMeshOut->TextureID = p_n25dMeshIn->TextureID;
	p_n25dMeshOut->DrawOrder = p_n25dMeshIn->DrawOrder;
	p_n25dMeshOut->u = p_n25dMeshIn->u;
	p_n25dMeshOut->v = p_n25dMeshIn->v;
	p_n25dMeshOut->u_end = p_n25dMeshIn->u_end;
	p_n25dMeshOut->v_end = p_n25dMeshIn->v_end;
	memcpy(p_n25dMeshOut->texture_param, p_n25dMeshIn->texture_param,sizeof(p_n25dMeshOut->texture_param));
	for (int i = 0; i < p_n25dMeshIn->pointcount; i++)
	{
		p_n25dMeshOut->x[i] = p_n25dMeshIn->x[i];
		p_n25dMeshOut->y[i] = p_n25dMeshIn->y[i];
		p_n25dMeshOut->z[i] = p_n25dMeshIn->z[i];
		p_n25dMeshOut->tu[i] = p_n25dMeshIn->tu[i];
		p_n25dMeshOut->tv[i] = p_n25dMeshIn->tv[i];
		p_n25dMeshOut->r[i] = p_n25dMeshIn->r[i];
		p_n25dMeshOut->g[i] = p_n25dMeshIn->g[i];
		p_n25dMeshOut->b[i] = p_n25dMeshIn->b[i];
		p_n25dMeshOut->a[i] = p_n25dMeshIn->a[i];
	}
}
//Clean all vertex (if changed vertex count)
void n25dMeshReallocateMemory(n25dMesh* p_n25Mesh, int32_t point_count)
{
	if (p_n25Mesh == NULL || point_count <= 0)
		return;
	p_n25Mesh->pointcount = point_count;
	if (p_n25Mesh->x != NULL)
	{
		free(p_n25Mesh->x);
		p_n25Mesh->x = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->y != NULL)
	{
		free(p_n25Mesh->y);
		p_n25Mesh->y = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->z != NULL)
	{
		free(p_n25Mesh->z);
		p_n25Mesh->z = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->r != NULL)
	{
		free(p_n25Mesh->r);
		p_n25Mesh->r = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->g != NULL)
	{
		free(p_n25Mesh->g);
		p_n25Mesh->g = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->b != NULL)
	{
		free(p_n25Mesh->b);
		p_n25Mesh->b = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->a != NULL)
	{
		free(p_n25Mesh->a);
		p_n25Mesh->a = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->tu != NULL)
	{
		free(p_n25Mesh->tu);
		p_n25Mesh->tu = (float*)malloc(point_count * sizeof(float));
	}
	if (p_n25Mesh->tv != NULL)
	{
		free(p_n25Mesh->tv);
		p_n25Mesh->tv = (float*)malloc(point_count * sizeof(float));
	}

	//out of memory check
	if (p_n25Mesh->x == NULL || p_n25Mesh->y == NULL || p_n25Mesh->z == NULL ||
		p_n25Mesh->r == NULL || p_n25Mesh->g == NULL || p_n25Mesh->b == NULL ||
		p_n25Mesh->a == NULL || p_n25Mesh->tu == NULL || p_n25Mesh->tv == NULL)
		return;

	//Default values
	for (int i = 0; i < p_n25Mesh->pointcount; i++)
	{
		p_n25Mesh->x[i] = 0.0f;
		p_n25Mesh->y[i] = 0.0f;
		p_n25Mesh->z[i] = 0.0f;
		p_n25Mesh->r[i] = 1.0f;
		p_n25Mesh->g[i] = 1.0f;
		p_n25Mesh->b[i] = 1.0f;
		p_n25Mesh->a[i] = 1.0f;
		p_n25Mesh->tu[i] = 0.0f;
		p_n25Mesh->tv[i] = 0.0f;
	}
}
//Free mesh vertex arrays
void n25dMeshFree(n25dMesh* p_n25Mesh)
{
	if (p_n25Mesh == NULL)
		return;
	free(p_n25Mesh->x); p_n25Mesh->x = NULL;
	free(p_n25Mesh->y); p_n25Mesh->y = NULL;
	free(p_n25Mesh->z); p_n25Mesh->z = NULL;
	free(p_n25Mesh->r); p_n25Mesh->r = NULL;
	free(p_n25Mesh->g); p_n25Mesh->g = NULL;
	free(p_n25Mesh->b); p_n25Mesh->b = NULL;
	free(p_n25Mesh->a); p_n25Mesh->a = NULL;
	free(p_n25Mesh->tu); p_n25Mesh->tu = NULL;
	free(p_n25Mesh->tv); p_n25Mesh->tv = NULL;
}
//Free entire model (parts, meshes, params, name)
void n25dModelFree(n25dModel* p_n25dModel)
{
	if (p_n25dModel == NULL)
		return;
	for (int32_t i = 0; i < p_n25dModel->nPartCount; i++)
	{
		n25dPart* pPart = &p_n25dModel->nPart[i];
		if (pPart->n25dMeshParam != NULL)
		{
			for (int32_t j = 0; j < pPart->n25dMeshParamC; j++)
			{
				free(pPart->n25dMeshParam[j].x_hi);
				free(pPart->n25dMeshParam[j].y_hi);
				free(pPart->n25dMeshParam[j].x_low);
				free(pPart->n25dMeshParam[j].y_low);
			}
			free(pPart->n25dMeshParam);
			pPart->n25dMeshParam = NULL;
		}
		n25dMeshFree(&pPart->n25dMeshOrigin);
		n25dMeshFree(&pPart->n25dMeshTransform);
	}
	//nParam may be NULL with nParamC > 0 after a grow OOM; guard the deref
	if (p_n25dModel->nParam != NULL)
	{
		for (int32_t i = 0; i < p_n25dModel->nParamC; i++)
			free(p_n25dModel->nParam[i].name);
	}
	//nPose may be NULL with nPoseC > 0 after a grow OOM; guard the deref
	if (p_n25dModel->nPose != NULL)
	{
		for (int32_t i = 0; i < p_n25dModel->nPoseC; i++)
		{
			free(p_n25dModel->nPose[i].name);
			free(p_n25dModel->nPose[i].entries);
		}
	}
	//nMotion may be NULL with nMotionC > 0 after a grow OOM; guard the deref
	if (p_n25dModel->nMotion != NULL)
	{
		for (int32_t i = 0; i < p_n25dModel->nMotionC; i++)
		{
			if (p_n25dModel->nMotion[i].curves != NULL)
			{
				for (int32_t j = 0; j < p_n25dModel->nMotion[i].curveC; j++)
					free(p_n25dModel->nMotion[i].curves[j].keys);
				free(p_n25dModel->nMotion[i].curves);
			}
			free(p_n25dModel->nMotion[i].name);
		}
	}
	free(p_n25dModel->nPart);
	free(p_n25dModel->nParam);
	free(p_n25dModel->nPose);
	free(p_n25dModel->nMotion);
	free(p_n25dModel->nModelName);
	free(p_n25dModel);
}
//Get transformed mesh vertex data as a flat array (x,y,z,r,g,b,a,tu,tv per vertex).
//out_data == NULL: returns the required size in floats. Returns -1 on error.
int32_t n25dGetTransformedVertexData(n25dModel* p_n25dModel, int32_t partID, float* out_data, int32_t max_size)
{
	if (p_n25dModel == NULL || partID < 0 || partID >= p_n25dModel->nPartCount)
		return -1;
	n25dMesh* pMesh = &p_n25dModel->nPart[partID].n25dMeshTransform;
	int32_t needed = pMesh->pointcount * 9;
	if (out_data == NULL)
		return needed;
	if (max_size < needed)
		return -1;
	for (int32_t i = 0; i < pMesh->pointcount; i++)
	{
		out_data[i * 9 + 0] = pMesh->x[i];
		out_data[i * 9 + 1] = pMesh->y[i];
		out_data[i * 9 + 2] = pMesh->z[i];
		out_data[i * 9 + 3] = pMesh->r[i];
		out_data[i * 9 + 4] = pMesh->g[i];
		out_data[i * 9 + 5] = pMesh->b[i];
		out_data[i * 9 + 6] = pMesh->a[i];
		out_data[i * 9 + 7] = pMesh->tu[i];
		out_data[i * 9 + 8] = pMesh->tv[i];
	}
	return needed;
}
//Helpers
void* n25dMemoryDynHelper(void* MemPointer, size_t MemorySize, int32_t* MaximumMemory, int32_t MemoryStep)
{
	if (MemPointer == NULL)
	{
		*MaximumMemory = MemoryStep;
		return malloc((*MaximumMemory) * MemorySize);
	}
	else
	{
		*MaximumMemory += MemoryStep;
		void* tmp = realloc(MemPointer, (*MaximumMemory) * MemorySize);
		if (tmp == NULL)
		{
			*MaximumMemory -= MemoryStep; //Roll back, realloc failed
			return NULL;// failed
		}
		else
			return tmp;
	}
}
n25dTexture n25dTexture_ptr(void *ptr)
{
	n25dTexture tex;
	memset(&tex, 0, sizeof(n25dTexture));
	tex.ptr = ptr;
	return tex;
}
n25dTexture n25dTexture_id(uint32_t id)
{
	n25dTexture tex;
	memset(&tex, 0, sizeof(n25dTexture));
	tex.id = id;
	return tex;
}

#pragma region RotationControl
void n25dPartSetAnchor(n25dModel* p_n25dModel, int32_t n25dPartID, float x, float y)
{
	assert(p_n25dModel->nPartCount > n25dPartID); //Heap corrupted or wrong ID
	p_n25dModel->nPart[n25dPartID].anchor_x = x;
	p_n25dModel->nPart[n25dPartID].anchor_y = y;
}
void n25dPartAddRotaryParent(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t n25dPartParentID)
{
	assert(p_n25dModel->nPartCount > n25dPartID); //Heap corrupted or wrong ID
	p_n25dModel->nPart[n25dPartID].ParentPartID = n25dPartParentID;
	p_n25dModel->nPart[n25dPartID].ParentPartIDC = p_n25dModel->nPart[n25dPartParentID].ParentPartIDC + 1;
}
#pragma endregion RotationControl
#pragma region ParamsModifier
int32_t n25dModelParamNew(n25dModel* p_n25dModel, char* name, int32_t name_len, float default_param, float min_param, float max_param)
{
	if (p_n25dModel == NULL)
		return -1;
	if (name_len < 0 || name_len > (int32_t)MAX_PARAM_NAME || (name_len > 0 && name == NULL))
		return -1;
	if (p_n25dModel->nParam == NULL)//allocate memory
		p_n25dModel->nParam = n25dMemoryDynHelper(p_n25dModel->nParam, sizeof(n25dParam),
			&p_n25dModel->nParamCMAX, MAX_ALLOC_N25D_PARAMS);
	if (p_n25dModel->nParamC == p_n25dModel->nParamCMAX)//Allocate more memory if need
		p_n25dModel->nParam = n25dMemoryDynHelper(p_n25dModel->nParam, sizeof(n25dParam),
			&p_n25dModel->nParamCMAX, MAX_ALLOC_N25D_PARAMS);

	assert(p_n25dModel->nParam != NULL);//Param is not initialized or out of memory

	if (p_n25dModel->nParam == NULL) return -1;//this section need error handling

	assert(p_n25dModel->nParamC < p_n25dModel->nParamCMAX); //Param counter overflow

	//Initialize the new param slot (append-only: index nParamC is never reused)
	n25dParam* p = &p_n25dModel->nParam[p_n25dModel->nParamC];
	p->param_min = min_param;
	p->param_max = max_param;
	//Clamp the default into range so param_cur == param_def at creation
	if (default_param < min_param) default_param = min_param;
	if (default_param > max_param) default_param = max_param;
	p->param_cur = default_param;
	p->param_def = default_param;
	//Name: length is authoritative, NUL terminator is for debug convenience
	p->name = NULL;
	p->name_len = 0;
	if (name_len > 0)
	{
		p->name = (char*)malloc((size_t)name_len + 1);
		if (p->name == NULL)
			return -1;//slot not counted, no part slots added yet
		memcpy(p->name, name, (size_t)name_len);
		p->name[name_len] = '\0';
		p->name_len = name_len;
	}
	//Add params to all parts
	for (int i = 0; i < p_n25dModel->nPartCount; i++)
		n25dPartParamNew(&p_n25dModel->nPart[i]);
	p_n25dModel->nParamC++;
	return p_n25dModel->nParamC - 1;
}

int32_t n25dModelGetParamID(n25dModel* p_n25dModel, char* name, int32_t name_len)
{
	if (p_n25dModel == NULL || name_len <= 0 || name == NULL)
		return -1;
	for (int32_t i = 0; i < p_n25dModel->nParamC; i++)
	{
		if (p_n25dModel->nParam[i].name_len == name_len &&
			memcmp(p_n25dModel->nParam[i].name, name, (size_t)name_len) == 0)
			return i;
	}
	return -1;
}

void n25dModelSetParam(n25dModel* p_n25dModel, int32_t ParamID, float value)
{
	if (p_n25dModel == NULL || ParamID < 0 || ParamID >= p_n25dModel->nParamC)
		return;
	n25dParam* p = &p_n25dModel->nParam[ParamID];
	if (value >= p->param_max)
		value = p->param_max;
	if (value <= p->param_min)
		value = p->param_min;
	p->param_cur = value;
}

void n25dModelApplyParams(n25dModel* p_n25dModel)
{
	if (p_n25dModel == NULL)
		return;
	n25dModelTransformationBegin(p_n25dModel);
	for (int32_t i = 0; i < p_n25dModel->nParamC; i++)
		n25dModelTransformation(p_n25dModel, i, p_n25dModel->nParam[i].param_cur);
	n25dModelTransformationEnd(p_n25dModel);
}

int32_t n25dModelPoseNew(n25dModel* p_n25dModel, char* name, int32_t name_len)
{
	if (p_n25dModel == NULL)
		return -1;
	if (name_len < 0 || name_len > (int32_t)MAX_POSE_NAME || (name_len > 0 && name == NULL))
		return -1;
	if (p_n25dModel->nPose == NULL)//allocate memory
		p_n25dModel->nPose = n25dMemoryDynHelper(p_n25dModel->nPose, sizeof(n25dPose),
			&p_n25dModel->nPoseCMAX, MAX_ALLOC_N25D_POSES);
	if (p_n25dModel->nPoseC == p_n25dModel->nPoseCMAX)//Allocate more memory if need
		p_n25dModel->nPose = n25dMemoryDynHelper(p_n25dModel->nPose, sizeof(n25dPose),
			&p_n25dModel->nPoseCMAX, MAX_ALLOC_N25D_POSES);
	if (p_n25dModel->nPose == NULL) return -1;

	//Initialize the new pose slot (append-only: index nPoseC is never reused)
	n25dPose* p = &p_n25dModel->nPose[p_n25dModel->nPoseC];
	p->name = NULL;
	p->name_len = 0;
	p->entries = NULL;
	p->entryC = 0;
	p->entryCMAX = 0;
	if (name_len > 0)
	{
		//Name: length is authoritative, NUL terminator is for debug convenience
		p->name = (char*)malloc((size_t)name_len + 1);
		if (p->name == NULL)
			return -1;
		memcpy(p->name, name, (size_t)name_len);
		p->name[name_len] = '\0';
		p->name_len = name_len;
	}
	p_n25dModel->nPoseC++;
	return p_n25dModel->nPoseC - 1;
}

int32_t n25dModelPoseAddParam(n25dModel* p_n25dModel, int32_t poseID, int32_t paramID, float value)
{
	if (p_n25dModel == NULL || p_n25dModel->nPose == NULL)
		return -1;
	if (poseID < 0 || poseID >= p_n25dModel->nPoseC)
		return -1;
	if (paramID < 0 || paramID >= p_n25dModel->nParamC)
		return -1;
	n25dPose* p = &p_n25dModel->nPose[poseID];
	if (p->entryC == p->entryCMAX)//Allocate more memory if need
		p->entries = n25dMemoryDynHelper(p->entries, sizeof(n25dPoseParam),
			&p->entryCMAX, MAX_ALLOC_N25D_POSE_ENTRIES);
	if (p->entries == NULL)
		return -1;
	p->entries[p->entryC].param_id = paramID;
	p->entries[p->entryC].value = value;
	p->entryC++;
	return 0;
}

int32_t n25dModelGetPoseID(n25dModel* p_n25dModel, char* name, int32_t name_len)
{
	if (p_n25dModel == NULL || name_len <= 0 || name == NULL)
		return -1;
	for (int32_t i = 0; i < p_n25dModel->nPoseC; i++)
	{
		if (p_n25dModel->nPose[i].name_len == name_len &&
			memcmp(p_n25dModel->nPose[i].name, name, (size_t)name_len) == 0)
			return i;
	}
	return -1;
}

void n25dModelApplyPose(n25dModel* p_n25dModel, int32_t poseID)
{
	if (p_n25dModel == NULL || p_n25dModel->nPose == NULL)
		return;
	if (poseID < 0 || poseID >= p_n25dModel->nPoseC)
		return;
	n25dPose* p = &p_n25dModel->nPose[poseID];
	for (int32_t i = 0; i < p->entryC; i++)
		n25dModelSetParam(p_n25dModel, p->entries[i].param_id, p->entries[i].value);
	n25dModelApplyParams(p_n25dModel);
}
//////////////////////////////////////////////////////////////////////////
// Motion (keyframed param curves) section
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionNew(n25dModel* p_n25dModel, char* name, int32_t name_len)
{
	if (p_n25dModel == NULL)
		return -1;
	if (name_len < 0 || name_len > (int32_t)MAX_MOTION_NAME || (name_len > 0 && name == NULL))
		return -1;
	if (p_n25dModel->nMotion == NULL)//allocate memory
		p_n25dModel->nMotion = n25dMemoryDynHelper(p_n25dModel->nMotion, sizeof(n25dMotion),
			&p_n25dModel->nMotionCMAX, MAX_ALLOC_N25D_MOTIONS);
	if (p_n25dModel->nMotionC == p_n25dModel->nMotionCMAX)//Allocate more memory if need
		p_n25dModel->nMotion = n25dMemoryDynHelper(p_n25dModel->nMotion, sizeof(n25dMotion),
			&p_n25dModel->nMotionCMAX, MAX_ALLOC_N25D_MOTIONS);
	if (p_n25dModel->nMotion == NULL) return -1;

	//Initialize the new motion slot (append-only: index nMotionC is never reused)
	n25dMotion* mo = &p_n25dModel->nMotion[p_n25dModel->nMotionC];
	mo->name = NULL;
	mo->name_len = 0;
	mo->curves = NULL;
	mo->curveC = 0;
	mo->curveCMAX = 0;
	if (name_len > 0)
	{
		//Name: length is authoritative, NUL terminator is for debug convenience
		mo->name = (char*)malloc((size_t)name_len + 1);
		if (mo->name == NULL)
			return -1;
		memcpy(mo->name, name, (size_t)name_len);
		mo->name[name_len] = '\0';
		mo->name_len = name_len;
	}
	p_n25dModel->nMotionC++;
	return p_n25dModel->nMotionC - 1;
}

int32_t n25dModelMotionAddCurve(n25dModel* p_n25dModel, int32_t motionID, int32_t paramID)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	if (paramID < 0 || paramID >= p_n25dModel->nParamC)
		return -1;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (mo->curveC == mo->curveCMAX)//Allocate more memory if need
		mo->curves = n25dMemoryDynHelper(mo->curves, sizeof(n25dMotionCurve),
			&mo->curveCMAX, MAX_ALLOC_N25D_MOTION_CURVES);
	if (mo->curves == NULL)
		return -1;
	n25dMotionCurve* c = &mo->curves[mo->curveC];
	c->param_id = paramID;
	c->keys = NULL;
	c->keyC = 0;
	c->keyCMAX = 0;
	mo->curveC++;
	return mo->curveC - 1;
}

int32_t n25dModelMotionCurveAddKey(n25dModel* p_n25dModel, int32_t motionID,
									   int32_t curveIndex, float time, float value)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (curveIndex < 0 || curveIndex >= mo->curveC)
		return -1;
	n25dMotionCurve* c = &mo->curves[curveIndex];
	//Reject a non-finite key time: NaN slips past the sorted check below (NaN
	//compares false both ways) and would break the sampler's binary search.
	if (time != time)
		return -1;
	//Keys stay sorted: reject a key earlier than the current last key
	if (c->keyC > 0 && time < c->keys[c->keyC - 1].time)
		return -1;
	if (c->keyC == c->keyCMAX)//Allocate more memory if need
		c->keys = n25dMemoryDynHelper(c->keys, sizeof(n25dMotionKey),
			&c->keyCMAX, MAX_ALLOC_N25D_MOTION_KEYS);
	if (c->keys == NULL)
		return -1;
	c->keys[c->keyC].time = time;
	c->keys[c->keyC].value = value;
	c->keys[c->keyC].ease = (int32_t)N25EASE_LINEAR;
	c->keys[c->keyC].bz[0] = 0.0f;
	c->keys[c->keyC].bz[1] = 0.0f;
	c->keys[c->keyC].bz[2] = 0.0f;
	c->keys[c->keyC].bz[3] = 0.0f;
	c->keyC++;
	return 0;
}

int32_t n25dModelMotionCurveKeySetEase(n25dModel* p_n25dModel, int32_t motionID,
										   int32_t curveIndex, int32_t keyIndex,
										   int32_t ease,
										   float bz0, float bz1, float bz2, float bz3)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (curveIndex < 0 || curveIndex >= mo->curveC)
		return -1;
	n25dMotionCurve* c = &mo->curves[curveIndex];
	if (keyIndex < 0 || keyIndex >= c->keyC)
		return -1;
	if (ease != (int32_t)N25EASE_LINEAR && ease != (int32_t)N25EASE_BEZIER)
		return -1;
	c->keys[keyIndex].ease = ease;
	c->keys[keyIndex].bz[0] = bz0;
	c->keys[keyIndex].bz[1] = bz1;
	c->keys[keyIndex].bz[2] = bz2;
	c->keys[keyIndex].bz[3] = bz3;
	return 0;
}

int32_t n25dModelGetMotionID(n25dModel* p_n25dModel, char* name, int32_t name_len)
{
	if (p_n25dModel == NULL || name_len <= 0 || name == NULL)
		return -1;
	for (int32_t i = 0; i < p_n25dModel->nMotionC; i++)
	{
		if (p_n25dModel->nMotion[i].name_len == name_len &&
			memcmp(p_n25dModel->nMotion[i].name, name, (size_t)name_len) == 0)
			return i;
	}
	return -1;
}

int32_t n25dModelMotionGetCurveCount(n25dModel* p_n25dModel, int32_t motionID)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	return p_n25dModel->nMotion[motionID].curveC;
}

int32_t n25dModelMotionCurveGetParamID(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (curveIndex < 0 || curveIndex >= mo->curveC)
		return -1;
	return mo->curves[curveIndex].param_id;
}

int32_t n25dModelMotionCurveGetKeyCount(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return -1;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return -1;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (curveIndex < 0 || curveIndex >= mo->curveC)
		return -1;
	return mo->curves[curveIndex].keyC;
}

float n25dMotionGetDuration(n25dModel* p_n25dModel, int32_t motionID)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return 0.0f;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return 0.0f;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	//Duration = max key time. Seed from the first key (not 0.0f) so it is
	//correct when every key time is negative; 0.0f when the motion has no keys.
	int have = 0;
	float duration = 0.0f;
	for (int32_t i = 0; i < mo->curveC; i++)
	{
		n25dMotionCurve* c = &mo->curves[i];
		for (int32_t j = 0; j < c->keyC; j++)
		{
			if (!have || c->keys[j].time > duration)
			{
				duration = c->keys[j].time;
				have = 1;
			}
		}
	}
	return duration;
}

//Cubic bezier solve as in CSS/WebKit: P0=(0,0), P3=(1,1), control points
//(x1,y1),(x2,y2). Newton iteration on x(u) == t with a bisection fallback.
static float n25dBezierSolveU(float x1, float x2, float t)
{
	float Ax = 3.0f * x1;
	float Bx = 3.0f * (x2 - x1) - Ax;
	float Cx = 1.0f - Ax - Bx;
	float u = t;
	int converged = 0;
	for (int i = 0; i < 8; i++)
	{
		float bx = ((Cx * u + Bx) * u + Ax) * u - t;
		if (fabsf(bx) < 1e-6f) { converged = 1; break; } // converged
		float dx = (3.0f * Cx * u + 2.0f * Bx) * u + Ax;
		if (fabsf(dx) < 1e-6f) break; // flat slope -> bisection fallback
		u -= bx / dx;
		if (u < 0.0f) u = 0.0f;
		else if (u > 1.0f) u = 1.0f;
	}
	if (!converged) // Newton did not converge (slope break or 8 iters) -> bisect
	{
		float lo = 0.0f, hi = 1.0f;
		for (int i = 0; i < 32; i++)
		{
			u = (lo + hi) * 0.5f;
			float bx = ((Cx * u + Bx) * u + Ax) * u;
			if (fabsf(bx - t) < 1e-6f) break;
			if (bx < t) lo = u; else hi = u;
		}
	}
	return u;
}

static float n25dBezierEase(float x1, float y1, float x2, float y2, float t)
{
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	float u = n25dBezierSolveU(x1, x2, t);
	float Ay = 3.0f * y1;
	float By = 3.0f * (y2 - y1) - Ay;
	float Cy = 1.0f - Ay - By;
	return ((Cy * u + By) * u + Ay) * u;
}

float n25dMotionCurveValueAt(n25dModel* p_n25dModel, int32_t motionID,
							   int32_t curveIndex, float time)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return 0.0f;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return 0.0f;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	if (curveIndex < 0 || curveIndex >= mo->curveC)
		return 0.0f;
	n25dMotionCurve* c = &mo->curves[curveIndex];
	int32_t n = c->keyC;
	if (n == 0)
		return 0.0f;
	if (n == 1)
		return c->keys[0].value;
	//Hold at the ends of the key range
	if (time <= c->keys[0].time)
		return c->keys[0].value;
	if (time >= c->keys[n - 1].time)
		return c->keys[n - 1].value;
	//Binary search: largest lo with keys[lo].time <= time (ends in [0, n-2])
	int32_t lo = 0, hi = n - 2;
	while (lo < hi)
	{
		int32_t mid = (lo + hi + 1) / 2;
		if (c->keys[mid].time <= time)
			lo = mid;
		else
			hi = mid - 1;
	}
	float t0 = c->keys[lo].time;
	float v0 = c->keys[lo].value;
	float t1 = c->keys[lo + 1].time;
	float v1 = c->keys[lo + 1].value;
	float span = t1 - t0;
	float frac = (span <= 0.0f) ? 1.0f : (time - t0) / span;
	float eased = (c->keys[lo].ease == (int32_t)N25EASE_BEZIER)
		? n25dBezierEase(c->keys[lo].bz[0], c->keys[lo].bz[1], c->keys[lo].bz[2], c->keys[lo].bz[3], frac)
		: frac;
	return v0 + (v1 - v0) * eased;
}

void n25dModelSampleMotion(n25dModel* p_n25dModel, int32_t motionID, float time)
{
	if (p_n25dModel == NULL || p_n25dModel->nMotion == NULL)
		return;
	if (motionID < 0 || motionID >= p_n25dModel->nMotionC)
		return;
	n25dMotion* mo = &p_n25dModel->nMotion[motionID];
	for (int32_t c = 0; c < mo->curveC; c++)
	{
		float v = n25dMotionCurveValueAt(p_n25dModel, motionID, c, time);
		n25dModelSetParam(p_n25dModel, mo->curves[c].param_id, v);
	}
	n25dModelApplyParams(p_n25dModel);
}
int32_t n25dPartParamNew(n25dPart* pPart)
{
	if (pPart->n25dMeshParam == NULL)//allocate memory
		pPart->n25dMeshParam = n25dMemoryDynHelper(pPart->n25dMeshParam, sizeof(n25dMatrixTransform),
			&pPart->n25dMeshParamCMAX, MAX_ALLOC_N25D_PARAMS);
	if (pPart->n25dMeshParamC == pPart->n25dMeshParamCMAX)//Allocate more memory if need
		pPart->n25dMeshParam = n25dMemoryDynHelper(pPart->n25dMeshParam, sizeof(n25dMatrixTransform),
			&pPart->n25dMeshParamCMAX, MAX_ALLOC_N25D_PARAMS);
	if (pPart->n25dMeshParam == NULL || pPart->n25dMeshParamC >= pPart->n25dMeshParamCMAX)
		return -1;//Part is not initialized or out of memory

															  //rotation
	pPart->n25dMeshParam[pPart->n25dMeshParamC].angle_hi = 0.0f;
	pPart->n25dMeshParam[pPart->n25dMeshParamC].angle_low = 0.0f;
	pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low = (float*)malloc(pPart->pointcount * sizeof(float));
	//out of memory check
	if (pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi == NULL ||
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi == NULL ||
		pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low == NULL ||
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low == NULL)
	{
		free(pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi);
		free(pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi);
		free(pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low);
		free(pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low);
		return -1;
	}
	for (int i = 0; i < pPart->pointcount; i++)
	{
		pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low[i] = 0.0f;
	}

	pPart->n25dMeshParamC++;
	return 0;
}
//Assign control
void n25dPartSetParamVertexPos(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, int32_t v, float x, float y)
{
	//Part is not initialized
	assert(p_n25dModel->nPart != NULL);

	//Part counter overflow
	assert(p_n25dModel->nPartCount > n25dPartID);

	//Mesh parameter is not initialized (do you use n25dModelParamNew() first?)
	assert(p_n25dModel->nPart[n25dPartID].n25dMeshParam != NULL);

	//Param counter overflow
	assert(p_n25dModel->nParamC > ParamID);
	if (Key >= 1)
	{
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].x_hi[v] = x;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].y_hi[v] = y;
	}
	else if (Key <= -1)
	{
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].x_low[v] = x;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].y_low[v] = y;
	}
	else //zero
	{
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].x_low[v] = 0.0f;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].y_low[v] = 0.0f;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].x_hi[v] = 0.0f;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].y_hi[v] = 0.0f;
	}
}
void n25dPartSetParamRotate(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, float angle_rad)
{
	//Part is not initialized
	assert(p_n25dModel->nPart != NULL);

	//Part counter overflow
	assert(p_n25dModel->nPartCount > n25dPartID);

	//Mesh parameter is not initialized (do you use n25dModelParamNew() first?)
	assert(p_n25dModel->nPart[n25dPartID].n25dMeshParam != NULL);

	//Param counter overflow
	assert(p_n25dModel->nParamC > ParamID);

	if (Key >= 1)
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].angle_hi = angle_rad;
	else if (Key <= -1)
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].angle_low = angle_rad;
	else //zero
	{
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].angle_hi = 0.0f;
		p_n25dModel->nPart[n25dPartID].n25dMeshParam[ParamID].angle_low = 0.0f;
	}
}
//Transformation by params
void n25dModelTransformationBegin(n25dModel* p_n25dModel)
{
	//Model is not initialized
	assert(p_n25dModel != NULL);

	for (int i = 0; i < p_n25dModel->nPartCount; i++)
	{
		//reset all matrix
		n25dAffineSetIdentity(&p_n25dModel->nPart[i].RotationMatrix);
		for (int i2 = 0; i2 < p_n25dModel->nPart[i].pointcount; i2++)
		{
			//Reset Transform array
			p_n25dModel->nPart[i].n25dMeshTransform.x[i2] = p_n25dModel->nPart[i].n25dMeshOrigin.x[i2];
			p_n25dModel->nPart[i].n25dMeshTransform.y[i2] = p_n25dModel->nPart[i].n25dMeshOrigin.y[i2];
		}
		//Reset angle
		p_n25dModel->nPart[i].angle = 0.0f;
	}
}
void n25dModelTransformation(n25dModel* p_n25dModel, int32_t ParamID, float f_param) //not completed
{
	//Model is not initialized
	assert(p_n25dModel != NULL);

	//Param counter overflow
	assert(p_n25dModel->nParamC > ParamID);

	if (f_param >= p_n25dModel->nParam[ParamID].param_max)
		f_param = p_n25dModel->nParam[ParamID].param_max;
	if (f_param <= p_n25dModel->nParam[ParamID].param_min)
		f_param = p_n25dModel->nParam[ParamID].param_min;

	p_n25dModel->nParam[ParamID].param_cur = f_param;
	//Calculate transformation for each part of this parameter
	for (int i = 0; i < p_n25dModel->nPartCount; i++)
	{
		//Part may not be bound to this param (fewer mesh params than the model)
		if (p_n25dModel->nPart[i].n25dMeshParam == NULL || p_n25dModel->nPart[i].n25dMeshParamC <= ParamID)
			continue;
		//Calculate angle for all parts in this parameter
		float tangle = 0.0f;
		if (f_param > 0.0f)
			tangle = p_n25dModel->nPart[i].n25dMeshParam[ParamID].angle_hi;
		else if (f_param < 0.0f)
			tangle = p_n25dModel->nPart[i].n25dMeshParam[ParamID].angle_low;

		tangle = tangle * f_param;
		p_n25dModel->nPart[i].angle += tangle; //Add angle

		n25dPartParamTransitionTransform(&p_n25dModel->nPart[i], ParamID, f_param);
	}
}
void n25dModelTransformationEnd(n25dModel* p_n25dModel)
{
	//Rotation control
	int32_t MaxRotateEntry = 0;
	for (int i = 0; i < p_n25dModel->nPartCount; i++)
	{
		if (p_n25dModel->nPart[i].ParentPartIDC == 0)
		{
			if (p_n25dModel->nPart[i].angle != 0.0f)
			{
				//Rotate only parents
				n25dAffine model;
				n25dAffineSetIdentity(&model);
				n25dPartParamRotationTransform(&p_n25dModel->nPart[i], &model, p_n25dModel->nPart[i].angle);
				//Store rotation matrix
				p_n25dModel->nPart[i].RotationMatrix = model;
			}
		}
		//Collect maximum child count
		MaxRotateEntry = max(MaxRotateEntry, p_n25dModel->nPart[i].ParentPartIDC);
	}

	for (int i2 = 1; MaxRotateEntry >= i2; i2++)
		for (int i = 0; i < p_n25dModel->nPartCount; i++)
		{
			if (p_n25dModel->nPart[i].ParentPartIDC == i2)
			{
				//Rotate to each part of the parent's pivot point
				n25dAffine model;
				//Get rotation matrix from parent
				model = p_n25dModel->nPart[p_n25dModel->nPart[i].ParentPartID].RotationMatrix;
				n25dPartParamRotationTransform(&p_n25dModel->nPart[i], &model, p_n25dModel->nPart[i].angle);
				//Store rotation matrix
				p_n25dModel->nPart[i].RotationMatrix = model;
			}
		}
}
//Mesh transformation
void n25dPartParamTransitionTransform(n25dPart* pPart, int32_t ParamID, float f_param)
{
	if (f_param != 0.0f)
	{
		//The hi/low choice depends only on f_param (the caller guarantees f_param != 0),
		//so pick the offset arrays once, outside the vertex loop.
		const float* ox;
		const float* oy;
		if (f_param > 0.0f)
		{
			//Translate coordinates by params (offset for this vertex, scaled by param)
			ox = pPart->n25dMeshParam[ParamID].x_hi;
			oy = pPart->n25dMeshParam[ParamID].y_hi;
		}
		else //f_param < 0.0f
		{
			ox = pPart->n25dMeshParam[ParamID].x_low;
			oy = pPart->n25dMeshParam[ParamID].y_low;
		}
		//Vertex transforming (Translation)
		for (int i = 0; i < pPart->pointcount; i++)
		{
			//Update vertex transition (identity+translate matrix applied to the vertex = plain add)
			pPart->n25dMeshTransform.x[i] += ox[i] * f_param;
			pPart->n25dMeshTransform.y[i] += oy[i] * f_param;
		}
	}
}
void n25dPartParamRotationTransform(n25dPart* pPart, n25dAffine* mRotation, float f_angle)
{
	//Effective rotation center is (-anchor_x, +anchor_y) (the x negation is deliberate).
	//Prepend the pivot rotation: mNew = P * (*mRotation), so the existing transform is
	//applied first, then the rotation about the pivot.
	n25dAffine mNew;
	n25dAffineSetPivotRotate(&mNew, -pPart->anchor_x, pPart->anchor_y, f_angle);
	n25dAffineMul(&mNew, &mNew, mRotation);
	//Vertex transforming
	for (int i = 0; i < pPart->pointcount; i++)
	{
		float x;
		float y;
		n25dAffineTransformPoint(&mNew, pPart->n25dMeshTransform.x[i], pPart->n25dMeshTransform.y[i], &x, &y);
		//Send to transform
		pPart->n25dMeshTransform.x[i] = x;
		pPart->n25dMeshTransform.y[i] = y;
	}
	//Save rotation matrix
	*mRotation = mNew;
}
#pragma endregion ParamsModifier


