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
#pragma once
#ifndef N25D_MAIN_H
#define N25D_MAIN_H

//Dependencies
#include <cglm/cglm.h>

//Internal headers
#include "n25dtypes.h"

//Definitions
#define N25D_VERSION_MAJOR 0
#define N25D_VERSION_MINOR 4
//Dynamic memory allocation steps values
#define MAX_ALLOC_N25D_OBJ 10
#define MAX_ALLOC_N25D_PARTS 16
#define MAX_ALLOC_N25D_PARAMS 40

//////////////////////////////////////////////////////////////////////////
// n25dModelNew(float width, float height);
// return pointer of allocated data n25dModel
//	parameters:
//				width - RESERVED (may be NULL, may be not needed at all)
//				height - RESERVED (may be NULL, may be not needed at all)
//////////////////////////////////////////////////////////////////////////
n25dModel* n25dModelNew(float width, float height);
//////////////////////////////////////////////////////////////////////////
// n25dModelSetName(n25dModel* p_n25dmodel, char* charA, int32_t charLen);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model (to allocate new model use function n25dModelNew())
//				charA - UTF8 string
//				charLen - Length of string
//////////////////////////////////////////////////////////////////////////
void n25dModelSetName(n25dModel* p_n25dmodel, char* charA, int32_t charLen);
#pragma region n25dParts
//////////////////////////////////////////////////////////////////////////
// Parts section
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// n25dPartNew(n25dModel* p_n25dModel, n25dTexture in_texture, float x, float y, float width, float height);
// return part ID
//	parameters:
//				p_n25dModel - pointer to allocated model (to allocate new model use function n25dModelNew())
//				n25dTexture - your texture, use n25dTexture_ptr(pointer) for DirectX or n25dTexture_id() for OpenGL
//				x - relative coordinate from x of Model
//				y - same as x but y
//				width - part width
//				height - part height
//////////////////////////////////////////////////////////////////////////
int32_t n25dPartNew(n25dModel* p_n25dModel, n25dTexture in_texture, float x, float y, float width, float height);
//////////////////////////////////////////////////////////////////////////
// n25dPartParamNew(n25dPart* pPart);
// no return data
//	parameters:
//				pPart - pointer to Part where will be created new parameter
//////////////////////////////////////////////////////////////////////////
void n25dPartParamNew(n25dPart* pPart);
//////////////////////////////////////////////////////////////////////////
// n25dPartSetUVTexture(n25dModel* p_n25dModel, int32_t n25dPartID, float u, float v, float u_end, float v_end);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model (to allocate new model use function n25dModelNew())
//				n25dPartID - part ID
//				u - beginning u texture coordinate (if you use atlas set your texture coordinate, if not use 0.0f)
//				v - beginning v texture coordinate (if you use atlas set your texture coordinate, if not use 0.0f)
//				u_end - end u texture coordinate (if you use atlas set your texture coordinate, if not use atlas set 1.0f)
//				v_end - end v texture coordinate (if you use atlas set your texture coordinate, if not use atlas set 1.0f)
//////////////////////////////////////////////////////////////////////////
void n25dPartSetUVTexture(n25dModel* p_n25dModel, int32_t n25dPartID, float u, float v, float u_end, float v_end);
//////////////////////////////////////////////////////////////////////////
// n25dMeshReallocateMemory(n25dMesh* p_n25Mesh, int32_t point_count);
// no return data
//	parameters:
//				p_n25Mesh - pointer to allocated model (to allocate new model use function n25dModelNew())
//				point_count - part ID
//	note:
//	Cleaning all current vertex information and allocating memory
//////////////////////////////////////////////////////////////////////////
void n25dMeshReallocateMemory(n25dMesh* p_n25Mesh, int32_t point_count);

//////////////////////////////////////////////////////////////////////////
//Parts rotation settings
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// n25dPartSetAnchor(n25dModel* p_n25dModel, int32_t n25dPartID, float x, float y);
// no return data
//	parameters:
//				p_n25dModel - pointer to model
//				n25dPartID - part ID
//				x - anchor x coordinate
//				y - anchor y coordinate
//////////////////////////////////////////////////////////////////////////
void n25dPartSetAnchor(n25dModel* p_n25dModel, int32_t n25dPartID, float x, float y);
//////////////////////////////////////////////////////////////////////////
// n25dPartAddRotaryParent(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t n25dPartParentID);
// no return data
//	parameters:
//				p_n25dModel - pointer to model
//				n25dPartID - part ID
//				n25dPartParentID - another part ID to create rotation dependence
// Use this if part need rotation at another part (like a bone)
//////////////////////////////////////////////////////////////////////////
void n25dPartAddRotaryParent(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t n25dPartParentID);
#pragma endregion n25dParts
//////////////////////////////////////////////////////////////////////////
// Parameters (parts modifications)
// PLEASE Set parameters after create model and all parts!
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// n25dModelParamNew(n25dModel* p_n25dModel, float default_param, float min_param, float max_param);
// return parameter ID
//	parameters:
//				p_n25dModel - pointer to allocated model (to allocate new model use function n25dModelNew())
//				default_param - default parameter
//				min_param - minimum parameter
//				max_param - maximum parameter
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelParamNew(n25dModel* p_n25dModel, float default_param, float min_param, float max_param);

//////////////////////////////////////////////////////////////////////////
// n25dPartSetParamVertexPos(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, int32_t v, float x, float y);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				n25dPartID - part ID
//				ParamID - parameter ID
//				Key - parameter key (may be 1,0,-1)
//				v - vertex number
//				x - vertex x position
//				y - vertex y position
//////////////////////////////////////////////////////////////////////////
void n25dPartSetParamVertexPos(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, int32_t v, float x, float y);
//////////////////////////////////////////////////////////////////////////
// n25dPartSetParamRotate(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, float angle_rad);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				n25dPartID - part ID
//				ParamID - parameter ID
//				Key - parameter key (may be 1,0,-1)
//				angle_rad - rotation angle (in radians)
//////////////////////////////////////////////////////////////////////////
void n25dPartSetParamRotate(n25dModel* p_n25dModel, int32_t n25dPartID, int32_t ParamID, int32_t Key, float angle_rad);

//////////////////////////////////////////////////////////////////////////
// Transformations
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// n25dModelTransformationBegin(n25dModel* p_n25dModel)
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//////////////////////////////////////////////////////////////////////////
void n25dModelTransformationBegin(n25dModel* p_n25dModel);
//////////////////////////////////////////////////////////////////////////
// n25dModelTransformation(n25dModel* p_n25dModel, int32_t ParamID, float f_param)
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				ParamID - parameter ID
//				f_param - transformation parameter (linear from min to max, defaults -1.0f to 1.0f)
//////////////////////////////////////////////////////////////////////////
void n25dModelTransformation(n25dModel* p_n25dModel, int32_t ParamID, float f_param);
//////////////////////////////////////////////////////////////////////////
// n25dModelTransformationEnd(n25dModel* p_n25dModel)
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//////////////////////////////////////////////////////////////////////////
void n25dModelTransformationEnd(n25dModel* p_n25dModel);

//////////////////////////////////////////////////////////////////////////
// Helper functions 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// n25dTexture_ptr(void *ptr)
// return n25dTexture
//	parameters:
//				ptr - pointer to DirectX texture
//////////////////////////////////////////////////////////////////////////
n25dTexture n25dTexture_ptr(void *ptr);
//////////////////////////////////////////////////////////////////////////
// n25dTexture_ptr(uint32_t id)
// return n25dTexture
//	parameters:
//				id - OpenGL texture ID
//////////////////////////////////////////////////////////////////////////
n25dTexture n25dTexture_id(uint32_t id);
//////////////////////////////////////////////////////////////////////////
// n25dMemoryDynHelper(void* MemPointer, size_t MemorySize, int32_t* MaximumMemory, int32_t MemoryStep)
// return reallocated memory pointer or zero if failed
//	parameters:
//				MemPointer - Memory pointer to increase
//				MemorySize - size of element
//				MaximumMemory - return maximum allocated memory
//				MemoryStep - elements count
//////////////////////////////////////////////////////////////////////////
void* n25dMemoryDynHelper(void* MemPointer, size_t MemorySize, int32_t* MaximumMemory, int32_t MemoryStep);

#endif /* N25D_MAIN_H */