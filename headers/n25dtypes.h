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


//////////////////////////////////////////////////////////////////////////
// Universal structure to hold texture ID (OpenGL) or pointer (DirectX)
//////////////////////////////////////////////////////////////////////////
typedef union { void *ptr; uint32_t id; } n25dTexture;
//////////////////////////////////////////////////////////////////////////
// n25dParam structure hold parameters data range
//////////////////////////////////////////////////////////////////////////
struct n25dParam
{
	float param_cur;
	float param_min;
	float param_max;
};
typedef struct n25dParam n25dParam;
//////////////////////////////////////////////////////////////////////////
// Mesh structure use to copy data on render
// x,y,z - vertex position, maximum vertex count data holds on pointcount
//////////////////////////////////////////////////////////////////////////
struct n25dMesh
{
	int32_t pointcount; // Vertex count
	int32_t DrawOrder; // RESERVED
	n25dTexture TextureID; //Assigned texture
	float texture_param[4]; //RESERVED: Texture additional parameters
							//Texture uv map
	float u, v, u_end, v_end;
	//Vertex
	float* x;
	float* y;
	float* z; //always zero for 2d
			  //Colors
	float* r;
	float* g;
	float* b;
	float* a;
	//Texture uv coordinates
	float* tu;
	float* tv;
};
typedef struct n25dMesh n25dMesh;
//////////////////////////////////////////////////////////////////////////
// Mesh transformation
// Displaying mesh (vertex data)
// *_hi - for parameters 0.0f~1.0f
// *_low - for parameters -1.0f~-0.0f
//////////////////////////////////////////////////////////////////////////
struct n25dMatrixTransform
{
	float* x_hi;
	float* x_low;
	float* y_hi;
	float* y_low;
	float angle_hi, angle_low;
};
typedef struct n25dMatrixTransform n25dMatrixTransform;
//////////////////////////////////////////////////////////////////////////
// n25dPart
//////////////////////////////////////////////////////////////////////////
struct n25dPart
{
	int32_t pointcount; // Vertex count
	n25dTexture TextureID; //Assigned texture
	int32_t DrawOrder;
	//Part coordinates (relative of model coordinate x,y)
	float x;
	float y;
	float z; //always zero for 2d
	float width;
	float height;
	//Rotation control
	float anchor_x; //relative x coordinate to rotate at pivot point
	float anchor_y; //same as anchor_y but y
	float angle; //rotation angle in RADIANS, for child parts used sum all previous parents
	int32_t ParentPartID;
	int32_t ParentPartIDC; //internal counter Parent parts (rotation dependencies)
	mat4 RotationMatrix; // rotation state (child parts use this matrix to rotate at pivot point)
	//box sprite (on this moment)
	int32_t nMeshCount; //deprecated
	n25dMesh n25dMeshTransform; //Transformed mesh (to render data)
	//Original mesh
	n25dMesh n25dMeshOrigin;
	//Transforming parameters (use to change mesh state like rotate or transform vertex data)
	n25dMatrixTransform* n25dMeshParam;
	int32_t n25dMeshParamC; // Counter parameters
	int32_t n25dMeshParamCMAX; // Maximum parameters is used (need to dynamic allocate data)
};
typedef struct n25dPart n25dPart;
//////////////////////////////////////////////////////////////////////////
// n25dModel 
//////////////////////////////////////////////////////////////////////////
struct n25dModel
{
	//Position
	float x;
	float y;
	float z; //always zero for 2d
	//Size
	float width;
	float height;
	//Texture
	n25dTexture texture;
	//Parts
	int32_t nPartCount;
	int32_t nPartCount_max;
	//Global parameters (need for transforming meshes)
	int32_t nParamC;
	int32_t nParamCMAX;
	//Model name
	int32_t nModelNameLen;
	//Pointers
	n25dParam* nParam;
	n25dPart* nPart;
	char* nModelName;
};
typedef struct n25dModel n25dModel;