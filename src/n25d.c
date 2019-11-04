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
void n25dPartParamRotationTransform(n25dPart* pPart, mat4* mRotation, float f_angle);

n25dModel* n25dModelInit(n25dModel* p_n25dmodel, float width, float height)
{
	p_n25dmodel->nPartCount_max = 0;
	p_n25dmodel->nPartCount = 0;
	p_n25dmodel->nPart = 0;
	p_n25dmodel->x = 0.0f;
	p_n25dmodel->y = 0.0f;
	p_n25dmodel->z = 0.0f;
	p_n25dmodel->width = width;
	p_n25dmodel->height = height;
	//params new
	p_n25dmodel->nParam = NULL;
	p_n25dmodel->nParamC = 0;
	p_n25dmodel->nParamCMAX = 0;
	p_n25dmodel->nModelName = NULL;
	p_n25dmodel->nModelNameLen = 0;
	return p_n25dmodel;
}
n25dModel* n25dModelNew(float width, float height)
{
	return n25dModelInit(malloc(sizeof(n25dModel)), width, height);
}
void n25dModelSetName(n25dModel* p_n25dmodel, char* charA, int32_t charLen)
{
	if (p_n25dmodel->nModelName != NULL)
	{
		free(p_n25dmodel->nModelName);
		p_n25dmodel->nModelName = (char*)malloc(charLen * sizeof(char));
		memcpy(p_n25dmodel->nModelName, charA, charLen);
		p_n25dmodel->nModelNameLen = charLen;
	}
	else
	{
		p_n25dmodel->nModelName = (char*)malloc(charLen * sizeof(char));
		memcpy(p_n25dmodel->nModelName, charA, charLen);
		p_n25dmodel->nModelNameLen = charLen;
	}
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
	if (p_n25dModel->nPart == NULL) return 0;//release check return zero if out of memory
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
	glm_mat4_identity(p_n25dModel->nPart[p_n25dModel->nPartCount].RotationMatrix);//identify matrix for rotation tree
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
//Clean all vertex (if changed vertex count)
void n25dMeshReallocateMemory(n25dMesh* p_n25Mesh, int32_t point_count)
{
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
			return NULL;// failed
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
int32_t n25dModelParamNew(n25dModel* p_n25dModel, float default_param, float min_param, float max_param)
{
	if (p_n25dModel->nParam == NULL)//allocate memory
		p_n25dModel->nParam = n25dMemoryDynHelper(p_n25dModel->nParam, sizeof(n25dParam),
			&p_n25dModel->nParamCMAX, MAX_ALLOC_N25D_PARAMS);
	if (p_n25dModel->nParamC == p_n25dModel->nParamCMAX)//Allocate more memory if need
		p_n25dModel->nParam = n25dMemoryDynHelper(p_n25dModel->nParam, sizeof(n25dParam),
			&p_n25dModel->nParamCMAX, MAX_ALLOC_N25D_PARAMS);

	assert(p_n25dModel->nParam != NULL);//Part is not initialized or out of memory

	if (p_n25dModel->nParam == NULL) return -1;//this section need error handling

	assert(p_n25dModel->nParamC < p_n25dModel->nParamCMAX); //Param counter overflow

	p_n25dModel->nParam[p_n25dModel->nParamC].param_cur = default_param;
	p_n25dModel->nParam[p_n25dModel->nParamC].param_min = min_param;
	p_n25dModel->nParam[p_n25dModel->nParamC].param_max = max_param;
	//Add params to all parts
	for (int i = 0; i < p_n25dModel->nPartCount; i++)
		n25dPartParamNew(&p_n25dModel->nPart[i]);
	p_n25dModel->nParamC++;
	return p_n25dModel->nParamC - 1;
}
void n25dPartParamNew(n25dPart* pPart)
{
	if (pPart->n25dMeshParam == NULL)//allocate memory
		pPart->n25dMeshParam = n25dMemoryDynHelper(pPart->n25dMeshParam, sizeof(n25dMatrixTransform),
			&pPart->n25dMeshParamCMAX, MAX_ALLOC_N25D_PARAMS);
	if (pPart->n25dMeshParamC == pPart->n25dMeshParamCMAX)//Allocate more memory if need
		pPart->n25dMeshParam = n25dMemoryDynHelper(pPart->n25dMeshParam, sizeof(n25dMatrixTransform),
			&pPart->n25dMeshParamCMAX, MAX_ALLOC_N25D_PARAMS);

	assert(pPart->n25dMeshParam != NULL);//Part is not initialized or out of memory

	assert(pPart->n25dMeshParamC < pPart->n25dMeshParamCMAX); //Param counter overflow

															  //rotation
	pPart->n25dMeshParam[pPart->n25dMeshParamC].angle_hi = 0.0f;
	pPart->n25dMeshParam[pPart->n25dMeshParamC].angle_low = 0.0f;
	pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low = (float*)malloc(pPart->pointcount * sizeof(float));
	pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low = (float*)malloc(pPart->pointcount * sizeof(float));
	for (int i = 0; i < pPart->pointcount; i++)
	{
		pPart->n25dMeshParam[pPart->n25dMeshParamC].x_hi[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_hi[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].x_low[i] = 0.0f;
		pPart->n25dMeshParam[pPart->n25dMeshParamC].y_low[i] = 0.0f;
	}

	pPart->n25dMeshParamC++;
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
		glm_mat4_identity(p_n25dModel->nPart[i].RotationMatrix);
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
				mat4 model;
				glm_mat4_identity(model);
				n25dPartParamRotationTransform(&p_n25dModel->nPart[i], &model, p_n25dModel->nPart[i].angle);
				//Store rotation matrix
				glm_mat4_copy(model, p_n25dModel->nPart[i].RotationMatrix);
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
				mat4 model;
				//Get rotation matrix from parent
				glm_mat4_copy(p_n25dModel->nPart[p_n25dModel->nPart[i].ParentPartID].RotationMatrix, model);
				n25dPartParamRotationTransform(&p_n25dModel->nPart[i], &model, p_n25dModel->nPart[i].angle);
				//Store rotation matrix
				glm_mat4_copy(model, p_n25dModel->nPart[i].RotationMatrix);
			}
		}
}
//Mesh transformation
void n25dPartParamTransitionTransform(n25dPart* pPart, int32_t ParamID, float f_param)
{
	if (f_param != 0.0f)
	{
		//Vertex transforming (Translation)
		for (int i = 0; i < pPart->pointcount; i++)
		{
			float tx = pPart->n25dMeshTransform.x[i];
			float ty = pPart->n25dMeshTransform.y[i];
			//Set translation matrix for this vertex
			mat4 m;
			glm_mat4_identity(m);
			vec4 mvec = { pPart->n25dMeshTransform.x[i], pPart->n25dMeshTransform.y[i] , 0.0f ,1.0f };
			//Translate coordinates by params
			if (f_param > 0.0f)
			{
				tx = pPart->n25dMeshParam[ParamID].x_hi[i];
				ty = pPart->n25dMeshParam[ParamID].y_hi[i];
			}
			else if (f_param < 0.0f)
			{
				tx = pPart->n25dMeshParam[ParamID].x_low[i];
				ty = pPart->n25dMeshParam[ParamID].y_low[i];
			}
			tx = tx * f_param;
			ty = ty * f_param;
			vec3 tvec = { tx, ty, 0.0f };
			glm_translate(m, tvec);
			vec4 destvec = { 0.0f,0.0f,0.0f,0.0f };
			glm_mat4_mulv(m, mvec, destvec);
			//Update vertex transition
			pPart->n25dMeshTransform.x[i] = destvec[0];
			pPart->n25dMeshTransform.y[i] = destvec[1];
		}
	}
}
void n25dPartParamRotationTransform(n25dPart* pPart, mat4* mRotation, float f_angle)
{
	mat4 m;
	//Vertex transforming
	for (int i = 0; i < pPart->pointcount; i++)
	{
		float tx = pPart->n25dMeshTransform.x[i];
		float ty = pPart->n25dMeshTransform.y[i];
		//Get rotation matrix
		glm_mat4_copy(*mRotation, m);
		vec4 mvec = { pPart->n25dMeshTransform.x[i], pPart->n25dMeshTransform.y[i] , 0.0f , 1.0f };
		//Rotate at point
		glm_rotate_at(m, (vec3) { -pPart->anchor_x, pPart->anchor_y }, f_angle, (vec3) { 0.0f, 0.0f, -1.0f });
		vec4 destvec = { 0.0f,0.0f,0.0f,0.0f };
		glm_mat4_mulv(m, mvec, destvec);
		//Send to transform
		pPart->n25dMeshTransform.x[i] = destvec[0];
		pPart->n25dMeshTransform.y[i] = destvec[1];
	}
	//Save rotation matrix
	glm_mat4_copy(m, *mRotation);
}
#pragma endregion ParamsModifier


