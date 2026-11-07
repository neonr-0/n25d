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
#include <stdint.h>
#include <stdlib.h>

//Internal headers
#include "n25dtypes.h"

//Definitions
#define N25D_VERSION_MAJOR 0
#define N25D_VERSION_MINOR 4
//Dynamic memory allocation steps values
#define MAX_ALLOC_N25D_OBJ 10
#define MAX_ALLOC_N25D_PARTS 16
#define MAX_ALLOC_N25D_PARAMS 40
#define MAX_PARAM_NAME 128 //max parameter name length in bytes (enforced by n25dModelParamNew + loader)
#define MAX_POSE_NAME 128 //max pose name length in bytes (enforced by n25dModelPoseNew + loader)
#define MAX_ALLOC_N25D_POSES 8
#define MAX_ALLOC_N25D_POSE_ENTRIES 8
#define MAX_MOTION_NAME 128 //max motion name length in bytes (enforced by n25dModelMotionNew + loader)
#define MAX_ALLOC_N25D_MOTIONS 8
#define MAX_ALLOC_N25D_MOTION_CURVES 8
#define MAX_ALLOC_N25D_MOTION_KEYS 16

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
int32_t n25dPartParamNew(n25dPart* pPart);
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
// n25dMeshCopy(n25dMesh* p_n25dMeshIn, n25dMesh* p_n25dMeshOut);
// no return data
//	parameters:
//				p_n25dMeshIn - source mesh
//				p_n25dMeshOut - destination mesh (must be allocated with the same point count)
//	note:
//	Copies all vertex data and texture coordinates
//////////////////////////////////////////////////////////////////////////
void n25dMeshCopy(n25dMesh* p_n25dMeshIn, n25dMesh* p_n25dMeshOut);

//////////////////////////////////////////////////////////////////////////
// n25dMeshFree(n25dMesh* p_n25Mesh);
// no return data
//	parameters:
//				p_n25Mesh - pointer to mesh to free (vertex arrays only, mesh struct stays)
//////////////////////////////////////////////////////////////////////////
void n25dMeshFree(n25dMesh* p_n25Mesh);

//////////////////////////////////////////////////////////////////////////
// n25dModelFree(n25dModel* p_n25dModel);
// no return data
//	parameters:
//				p_n25dModel - pointer to model to free (all parts, meshes, params, name
//				and the model struct itself; do NOT free it again afterwards)
//////////////////////////////////////////////////////////////////////////
void n25dModelFree(n25dModel* p_n25dModel);

//////////////////////////////////////////////////////////////////////////
// n25dGetTransformedVertexData(n25dModel* p_n25dModel, int32_t partID, float* out_data, int32_t max_size);
// returns number of floats written, required size in floats if out_data is NULL, -1 on error
//	parameters:
//				p_n25dModel - pointer to allocated model
//				partID - part ID
//				out_data - output buffer (flat: x,y,z,r,g,b,a,tu,tv per vertex) or NULL to query size
//				max_size - output buffer size in floats
//////////////////////////////////////////////////////////////////////////
int32_t n25dGetTransformedVertexData(n25dModel* p_n25dModel, int32_t partID, float* out_data, int32_t max_size);

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
// n25dModelParamNew(n25dModel* p_n25dModel, char* name, int32_t name_len, float default_param, float min_param, float max_param);
// return parameter ID
//	parameters:
//				p_n25dModel - pointer to allocated model (to allocate new model use function n25dModelNew())
//				name - parameter name (stable identity); may be NULL when name_len == 0
//				name_len - name length in bytes (authoritative, no NUL required)
//				default_param - default parameter
//				min_param - minimum parameter
//				max_param - maximum parameter
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelParamNew(n25dModel* p_n25dModel, char* name, int32_t name_len, float default_param, float min_param, float max_param);

//////////////////////////////////////////////////////////////////////////
// n25dModelGetParamID(n25dModel* p_n25dModel, char* name, int32_t name_len);
// return parameter ID, or -1 if not found
//	parameters:
//				p_n25dModel - pointer to allocated model
//				name - parameter name to look up
//				name_len - name length in bytes
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelGetParamID(n25dModel* p_n25dModel, char* name, int32_t name_len);

//////////////////////////////////////////////////////////////////////////
// n25dModelSetParam(n25dModel* p_n25dModel, int32_t ParamID, float value);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				ParamID - parameter ID
//				value - new value (clamped to [min,max]); does not recompute the mesh
//////////////////////////////////////////////////////////////////////////
void n25dModelSetParam(n25dModel* p_n25dModel, int32_t ParamID, float value);

//////////////////////////////////////////////////////////////////////////
// n25dModelApplyParams(n25dModel* p_n25dModel);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//	recomputes all transformed meshes from current param values
//////////////////////////////////////////////////////////////////////////
void n25dModelApplyParams(n25dModel* p_n25dModel);

//////////////////////////////////////////////////////////////////////////
// n25dModelPoseNew(n25dModel* p_n25dModel, char* name, int32_t name_len);
// return pose ID, or -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				name - pose name (stable identity); may be NULL when name_len == 0
//				name_len - name length in bytes (authoritative, no NUL required)
//	Creates an empty pose (no param entries yet); add them with n25dModelPoseAddParam.
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelPoseNew(n25dModel* p_n25dModel, char* name, int32_t name_len);

//////////////////////////////////////////////////////////////////////////
// n25dModelPoseAddParam(n25dModel* p_n25dModel, int32_t poseID, int32_t paramID, float value);
// return 0 on success, -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				poseID - pose ID (from n25dModelPoseNew)
//				paramID - parameter ID (from n25dModelGetParamID)
//				value - value to set when the pose is applied
//	Appends one (param -> value) entry. Duplicates are allowed; last wins on apply.
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelPoseAddParam(n25dModel* p_n25dModel, int32_t poseID, int32_t paramID, float value);

//////////////////////////////////////////////////////////////////////////
// n25dModelGetPoseID(n25dModel* p_n25dModel, char* name, int32_t name_len);
// return pose ID, or -1 if not found
//	parameters:
//				p_n25dModel - pointer to allocated model
//				name - pose name to look up
//				name_len - name length in bytes
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelGetPoseID(n25dModel* p_n25dModel, char* name, int32_t name_len);

//////////////////////////////////////////////////////////////////////////
// n25dModelApplyPose(n25dModel* p_n25dModel, int32_t poseID);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				poseID - pose ID
//	Sets each of the pose's params (clamped to [min,max]) and recomputes all
//	meshes. Params not in the pose are left unchanged (partial override).
//////////////////////////////////////////////////////////////////////////
void n25dModelApplyPose(n25dModel* p_n25dModel, int32_t poseID);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionNew(n25dModel* p_n25dModel, char* name, int32_t name_len);
// return motion ID, or -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				name - motion name (stable identity); may be NULL when name_len == 0
//				name_len - name length in bytes (authoritative, no NUL required)
//	Creates an empty motion (no curves yet); add them with n25dModelMotionAddCurve.
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionNew(n25dModel* p_n25dModel, char* name, int32_t name_len);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionAddCurve(n25dModel* p_n25dModel, int32_t motionID, int32_t paramID);
// return curve index, or -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID (from n25dModelMotionNew)
//				paramID - parameter ID (from n25dModelGetParamID); must be valid
//	Attaches a curve for paramID to the motion; add keys with n25dModelMotionCurveAddKey.
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionAddCurve(n25dModel* p_n25dModel, int32_t motionID, int32_t paramID);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionCurveAddKey(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex, float time, float value);
// return 0 on success, -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//				curveIndex - curve index (from n25dModelMotionAddCurve)
//				time - key time in seconds
//				value - parameter value at the key time
//	Appends a key (defaults to N25EASE_LINEAR, bezier control points 0).
//	Fails if time < the curve's current last key time (keys stay sorted).
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionCurveAddKey(n25dModel* p_n25dModel, int32_t motionID,
									   int32_t curveIndex, float time, float value);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionCurveKeySetEase(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex, int32_t keyIndex, int32_t ease, float bz0, float bz1, float bz2, float bz3);
// return 0 on success, -1 on failure
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//				curveIndex - curve index
//				keyIndex - key index within the curve
//				ease - N25EASE_LINEAR or N25EASE_BEZIER (any other value fails);
//					applies to the segment from this key to the next
//				bz0..bz3 - bezier control points x1,y1,x2,y2 (meaningful for N25EASE_BEZIER only)
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionCurveKeySetEase(n25dModel* p_n25dModel, int32_t motionID,
										   int32_t curveIndex, int32_t keyIndex,
										   int32_t ease,
										   float bz0, float bz1, float bz2, float bz3);

//////////////////////////////////////////////////////////////////////////
// n25dModelGetMotionID(n25dModel* p_n25dModel, char* name, int32_t name_len);
// return motion ID, or -1 if not found
//	parameters:
//				p_n25dModel - pointer to allocated model
//				name - motion name to look up
//				name_len - name length in bytes
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelGetMotionID(n25dModel* p_n25dModel, char* name, int32_t name_len);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionGetCurveCount(n25dModel* p_n25dModel, int32_t motionID);
// return curve count, or -1 on a bad motion id
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionGetCurveCount(n25dModel* p_n25dModel, int32_t motionID);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionCurveGetParamID(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex);
// return the curve's parameter ID, or -1 on a bad motion/curve id
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//				curveIndex - curve index
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionCurveGetParamID(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex);

//////////////////////////////////////////////////////////////////////////
// n25dModelMotionCurveGetKeyCount(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex);
// return key count, or -1 on a bad motion/curve id
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//				curveIndex - curve index
//////////////////////////////////////////////////////////////////////////
int32_t n25dModelMotionCurveGetKeyCount(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex);

//////////////////////////////////////////////////////////////////////////
// n25dMotionGetDuration(n25dModel* p_n25dModel, int32_t motionID);
// return motion duration in seconds (max key time across all curves; 0 if no keys
// or a bad motion id), derived on demand, never stored
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//////////////////////////////////////////////////////////////////////////
float   n25dMotionGetDuration(n25dModel* p_n25dModel, int32_t motionID);

//////////////////////////////////////////////////////////////////////////
// n25dMotionCurveValueAt(n25dModel* p_n25dModel, int32_t motionID, int32_t curveIndex, float time);
// return the curve's interpolated value at `time` (pure read: no bus write, no
// recompute; 0.0f on a bad motion/curve id). Holds the first/last key value
// outside the key time range. A curve with 0 keys always yields 0.0f.
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID
//				curveIndex - curve index
//				time - time in seconds
//////////////////////////////////////////////////////////////////////////
float   n25dMotionCurveValueAt(n25dModel* p_n25dModel, int32_t motionID,
								   int32_t curveIndex, float time);

//////////////////////////////////////////////////////////////////////////
// n25dModelSampleMotion(n25dModel* p_n25dModel, int32_t motionID, float time);
// no return data
//	parameters:
//				p_n25dModel - pointer to allocated model
//				motionID - motion ID (bad id is a no-op)
//				time - time in seconds
//	Writes every curve's value at `time` to the bus (clamped to [min,max]) and
//	recomputes all meshes. Params without a curve are left unchanged (partial
//	override). A curve with 0 keys writes 0.0f to its param. The caller owns the
//	clock (looping/fading wrap `time` before calling).
//////////////////////////////////////////////////////////////////////////
void    n25dModelSampleMotion(n25dModel* p_n25dModel, int32_t motionID, float time);

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