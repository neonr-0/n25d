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
//Dependencies
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
//Internal headers
#include "byte_order.h"
#include "n25d.h"
#include "n25dfile.h"

//Definitions
int32_t n25dFileHelperAdd_int32(void* p_Data, int32_t in_long, int32_t dataLen);
int32_t n25dFileHelperAdd_f32(void* p_Data, float in_float, int32_t dataLen);
int32_t n25dFileHelperAdd_blob(void* p_Data, void* in_data, int32_t lenght, int32_t dataLen);
int32_t n25dFileHelperGet_blob(void* p_Data, void* out_data, int32_t lenght, int32_t dataLen);

//Internal: release model memory (used on load failure paths)
static void n25dMeshFreeInternal(n25dMesh* p_n25Mesh)
{
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
static void n25dModelFreeInternal(n25dModel* p_n25dModel)
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
		n25dMeshFreeInternal(&pPart->n25dMeshOrigin);
		n25dMeshFreeInternal(&pPart->n25dMeshTransform);
	}
	free(p_n25dModel->nPart);
	free(p_n25dModel->nParam);
	free(p_n25dModel->nModelName);
	free(p_n25dModel);
}
int32_t n25dFileHelperGet_int32(void* p_Data, int32_t* out_long, int32_t dataLen);
int32_t n25dFileHelperGet_f32(void* p_Data, float* out_float, int32_t dataLen);

//Helpers
int32_t n25dFileHelperAdd_int32(void* p_Data, int32_t in_long, int32_t dataLen)
{
	int8_t* p_tmp_byte = p_Data;
	int32_t* p_tmp = (int32_t*)&p_tmp_byte[dataLen];
	*p_tmp = htofl(in_long);
	return dataLen += sizeof(int32_t);
}
int32_t n25dFileHelperAdd_f32(void* p_Data, float in_float, int32_t dataLen)
{
	int8_t* p_tmp_byte = p_Data;
	float* p_tmp = (float*)&p_tmp_byte[dataLen];
	*p_tmp = htoff(in_float);
	return dataLen += sizeof(float);
}
int32_t n25dFileHelperAdd_blob(void* p_Data, void* in_data, int32_t lenght, int32_t dataLen)
{
	//Endian depends!
	int8_t* p_tmp = p_Data;
	memcpy(&p_tmp[dataLen], in_data, lenght);
	return dataLen += lenght;
}
int32_t n25dFileHelperGet_int32(void* p_Data, int32_t* out_long, int32_t dataLen)
{
	int8_t* p_tmp_byte = p_Data;
	int32_t* p_tmp = (int32_t*)&p_tmp_byte[dataLen];
	*out_long = htofl(*p_tmp);
	return dataLen += sizeof(int32_t);
}
int32_t n25dFileHelperGet_f32(void* p_Data, float* out_float, int32_t dataLen)
{
	int8_t* p_tmp_byte = p_Data;
	float* p_tmp = (float*)&p_tmp_byte[dataLen];
	*out_float = htoff(*p_tmp);
	return dataLen += sizeof(float);
}
int32_t n25dFileHelperGet_blob(void* p_Data, void* out_data, int32_t lenght, int32_t dataLen)
{
	//Endian depends!
	int8_t* p_tmp = p_Data;
	memcpy(out_data, &p_tmp[dataLen], lenght);
	return dataLen += lenght;
}

//Serialization buffer grow helper (check + realloc + OOM report)
static int8_t* n25dSaveBufferGrow(int8_t* dataC, int32_t dataLen, int32_t* dataLenMax)
{
	if (dataLen > *dataLenMax - (MAX_BUFFER / 2))//Allocate more memory if need
	{
		dataC = (int8_t*)n25dMemoryDynHelper(dataC, sizeof(int8_t), dataLenMax, MAX_BUFFER);
		if (dataC == NULL)
			*dataLenMax = 0;
	}
	return dataC;
}

//File operations
FILE* n25dFileOpenA(char* PathA, char const* modeA)
{
	FILE* f_id = NULL;
#if defined(_MSC_VER) && defined(UNICODE)
	wchar_t ModeW[32];
	wchar_t wFilename[2048];
	//65001 UTF8
	if (0 == MultiByteToWideChar(65001, 0, PathA, -1, wFilename, sizeof(wFilename)))
		return 0;
	if (0 == MultiByteToWideChar(65001, 0, modeA, -1, ModeW, sizeof(ModeW)))
		return 0;
#if _MSC_VER >= 1400
	if (0 != _wfopen_s(&f_id, wFilename, ModeW))
		f_id = 0;
#else
	f_id = _wfopen(wFilename, ModeW);
#endif
//C11 safe version
#elif (__STDC_VERSION__ >=201112L && __STDC_LIB_EXT1__ == 1) || _MSC_VER >= 1400
	if (0 != fopen_s(&f_id, PathA, modeA))
		f_id = NULL;
#else
	f_id = fopen(PathA, modeA);
#endif
	return f_id;
}
int32_t n25dFileWrite(FILE* in_file, int8_t* in_data, int32_t in_dataLen)
{
	if (fwrite(in_data, sizeof(int8_t), in_dataLen, in_file) != in_dataLen)	
		return -1;
	
	return 0;
}
int8_t* n25dFileRead(FILE* in_file, int32_t* out_dataLen)
{
	if (in_file == NULL)
	{
		*out_dataLen = 0;
		return NULL;
	}
	//get file size
	if (fseek(in_file, 0, SEEK_END) != 0)
	{
		*out_dataLen = 0;
		return NULL;
	}
	long fileSize = ftell(in_file);
	rewind(in_file);
	if (fileSize < 0 || fileSize > (long)INT32_MAX)
	{
		*out_dataLen = 0;
		return NULL;
	}
	if (fileSize == 0)
	{
		*out_dataLen = 0;
		return NULL;
	}
	//allocate memory based on file size
	int8_t* dataC = (int8_t*)malloc((size_t)fileSize * sizeof(int8_t));
	if (dataC == NULL)
	{
		*out_dataLen = 0;
		return NULL;
	}
	if (fread(dataC, sizeof(int8_t), (size_t)fileSize, in_file) != (size_t)fileSize)
	{
		free(dataC);
		*out_dataLen = 0;
		return NULL;
	}
	*out_dataLen = (int32_t)fileSize;
	return dataC;
}
int32_t n25dFileClose(FILE* in_file)
{
	return fclose(in_file);
}

//Saving
int32_t n25dSaveToFileA(n25dModel* p_n25dModel, char* PathA)
{
	int32_t dataLen = 0;
	int8_t* dataC = n25dSaveToMemory(p_n25dModel, &dataLen);
	if (dataC == NULL)
		return -1;

	FILE* f_id = n25dFileOpenA(PathA, "wb");
	if (f_id == NULL)
	{
		free(dataC);
		return -1;
	}

	if (n25dFileWrite(f_id, dataC, dataLen) != 0)
	{
		free(dataC);
		n25dFileClose(f_id);
		return -1;
	}

	int32_t rc = n25dFileClose(f_id);
	free(dataC);
	return rc;
}
int8_t* n25dSaveToMemory(n25dModel* p_n25dModel, int32_t* out_dataLen)
{
	//Serialize data
	int8_t* dataC = NULL;
	int32_t dataLen = 0;
	int32_t dataLenMax = 0;
	if (dataC == NULL)//allocate memory
		dataC = n25dMemoryDynHelper(dataC, sizeof(int8_t), &dataLenMax, MAX_BUFFER);
	if (dataC == NULL)
		return dataC;

	//Serialize header information
	n25dFileHeader Header; 
	Header.n25dIdentify = htofl(n25dSignature);
	Header.n25dFileVersion = htofl(n25dVersionFile);
	Header.n25dModelPartCount = htofl(p_n25dModel->nPartCount);
	Header.n25dModelParamCount = htofl(p_n25dModel->nParamC);
	Header.Reserved = (int32_t)0;
	int32_t headersize = sizeof(Header);
	dataLen = n25dFileHelperAdd_blob(dataC, &Header, headersize, dataLen);

	//Next block (ModelName)
	dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_ModelName, dataLen);
	char* ModelName = p_n25dModel->nModelName;
	uint32_t ModelNameLen = p_n25dModel->nModelNameLen;
	assert(ModelNameLen < (uint32_t)MAX_MODEL_NAME); //Name it's too long
	//ModelNameLen - may be NULL if ModelName not presented
	dataLen = n25dFileHelperAdd_int32(dataC, ModelNameLen, dataLen);
	if(ModelNameLen != (uint32_t)0)
		dataLen = n25dFileHelperAdd_blob(dataC, ModelName, (int32_t)ModelNameLen, dataLen);
	//Next block (Params)
	dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_ModelParams, dataLen);
	for (int i = 0; i < p_n25dModel->nParamC; i++)
	{
		dataC = n25dSaveBufferGrow(dataC, dataLen, &dataLenMax);
		if (dataC == NULL) goto savefail;
		dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_ModelParam_next, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nParam[i].param_cur, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nParam[i].param_min, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nParam[i].param_max, dataLen);
	}
	//Next block (Parts)
	dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_ModelParts, dataLen);
	for (int i=0;i<p_n25dModel->nPartCount;i++)
	{
		//Next block (Part)
		dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_ModelPart_next, dataLen);
		dataC = n25dSaveBufferGrow(dataC, dataLen, &dataLenMax);
		if (dataC == NULL) goto savefail;

		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].pointcount, dataLen); //Vertex count
		//texture?
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].DrawOrder, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].x, dataLen); //x
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].y, dataLen); //y
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].z, dataLen); //z
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].width, dataLen);	//canvas width
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].height, dataLen);	//canvas height
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].anchor_x, dataLen);	
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].anchor_y, dataLen);
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].ParentPartID, dataLen); //rotation parents
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].ParentPartIDC, dataLen); //rotation parents counter

		//Mesh
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.pointcount, dataLen);
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.DrawOrder, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.u, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.v, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.u_end, dataLen);
		dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.v_end, dataLen);
		for (int i2 = 0; i2 < p_n25dModel->nPart[i].pointcount; i2++)
		{
			dataC = n25dSaveBufferGrow(dataC, dataLen, &dataLenMax);
			if (dataC == NULL) goto savefail;
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.x[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.y[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.z[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.r[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.g[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.b[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.a[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.tu[i2], dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshOrigin.tv[i2], dataLen);
		}
		//Params for mesh
		dataLen = n25dFileHelperAdd_int32(dataC, p_n25dModel->nPart[i].n25dMeshParamC, dataLen);
		for (int i2 = 0; i2 < p_n25dModel->nPart[i].n25dMeshParamC; i2++)
		{
			for (int i3 = 0; i3 < p_n25dModel->nPart[i].n25dMeshOrigin.pointcount; i3++)
			{
				dataC = n25dSaveBufferGrow(dataC, dataLen, &dataLenMax);
				if (dataC == NULL) goto savefail;
				dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].x_hi[i3], dataLen);
				dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].x_low[i3], dataLen);
				dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].y_hi[i3], dataLen);
				dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].y_low[i3], dataLen);
			}
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].angle_hi, dataLen);
			dataLen = n25dFileHelperAdd_f32(dataC, p_n25dModel->nPart[i].n25dMeshParam[i2].angle_low, dataLen);
		}
	}

	dataLen = n25dFileHelperAdd_int32(dataC, n25FileT_EOF, dataLen);
	*out_dataLen = dataLen;
	return dataC;

savefail:
	*out_dataLen = 0;
	free(dataC);
	return NULL;
}
//Loading
int32_t n25dLoadFromFileA(n25dModel** p_n25dModel, char* PathA)
{
	*p_n25dModel = NULL;
	if (p_n25dModel == NULL)
		return -1;

	int32_t dataLen = 0;
	FILE* f_id = n25dFileOpenA(PathA, "rb");
	if (f_id == NULL)
		return -1;

	int8_t* dataC = n25dFileRead(f_id, &dataLen);
	n25dFileClose(f_id);
	if (dataC == NULL)
		return -1;

	*p_n25dModel = n25dLoadFromMemory(dataC, dataLen);
	free(dataC);
	return (*p_n25dModel != NULL) ? 0 : -1;
}
n25dModel* n25dLoadFromMemory(int8_t* dataC, int32_t dataLenMax)
{
	int32_t dataLen = 0;
	n25dModel* p_n25dModel = NULL;

	if (dataC == NULL || dataLenMax < (int32_t)sizeof(n25dFileHeader))
		return NULL;

	//Get Header information
	n25dFileHeader Header;
	memset(&Header, 0, sizeof(n25dFileHeader));
	dataLen = n25dFileHelperGet_blob(dataC, &Header, sizeof(Header), dataLen);
	Header.n25dIdentify = (int32_t)ftohl((uint32_t)Header.n25dIdentify);
	Header.n25dFileVersion = (int32_t)ftohl((uint32_t)Header.n25dFileVersion);
	Header.n25dModelPartCount = (int32_t)ftohl((uint32_t)Header.n25dModelPartCount);
	Header.n25dModelParamCount = (int32_t)ftohl((uint32_t)Header.n25dModelParamCount);
	Header.Reserved = (int32_t)ftohl((uint32_t)Header.Reserved);

	//Verify signature and file version
	if (Header.n25dIdentify != (int32_t)n25dSignature)
		return NULL;
	if (Header.n25dFileVersion != (int32_t)n25dVersionFile)
		return NULL;

	//Create new model
	p_n25dModel = n25dModelNew((float)0.0f, (float)0.0f);
	if (p_n25dModel == NULL)
		return NULL;

	//Get blocks
	int32_t n25FileTdata = (int32_t)-1;

	while (dataLen + (int32_t)sizeof(int32_t) <= dataLenMax)
	{
		dataLen = n25dFileHelperGet_int32(dataC, &n25FileTdata, dataLen);
		switch (n25FileTdata)
		{
			case n25FileT_ModelName: //ModelName block
			{
				//Check string length
				if (dataLen + (int32_t)sizeof(int32_t) > dataLenMax)
					goto fail;
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nModelNameLen, dataLen);
				if (p_n25dModel->nModelNameLen < 0 || p_n25dModel->nModelNameLen > (int32_t)MAX_MODEL_NAME ||
					dataLen + p_n25dModel->nModelNameLen > dataLenMax)
					goto fail;
				if (p_n25dModel->nModelNameLen != 0)
				{
					//Get name
					char ModelName[MAX_MODEL_NAME + 1];
					dataLen = n25dFileHelperGet_blob(dataC, ModelName, p_n25dModel->nModelNameLen, dataLen);
					n25dModelSetName(p_n25dModel, ModelName, p_n25dModel->nModelNameLen);
				}
				else
				{
					//No name present
					p_n25dModel->nModelName = NULL;
					p_n25dModel->nModelNameLen = (int32_t)0;
				}
				break;
			}
			case n25FileT_ModelParam_next: //ModelParam block
			{
				if (dataLen + 3 * (int32_t)sizeof(float) > dataLenMax)
					goto fail;
				float param_cur, param_min, param_max;
				dataLen = n25dFileHelperGet_f32(dataC, &param_cur, dataLen);
				dataLen = n25dFileHelperGet_f32(dataC, &param_min, dataLen);
				dataLen = n25dFileHelperGet_f32(dataC, &param_max, dataLen);
				if (n25dModelParamNew(p_n25dModel, param_cur, param_min, param_max) < 0)
					goto fail;
				break;
			}
			case n25FileT_ModelPart_next: //ModelParts block
			{
				if (dataLen + (int32_t)sizeof(int32_t) > dataLenMax)
					goto fail;
				int32_t tmp_id = n25dPartNew(p_n25dModel, n25dTexture_id((uint32_t)0), 0.0f, 0.0f, 0.0f, 0.0f); // fill with default values
				if (tmp_id < 0)
					goto fail;
				int32_t cur_part = p_n25dModel->nPartCount-1;
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].pointcount, dataLen); //Vertex count
				int32_t part_points = p_n25dModel->nPart[cur_part].pointcount;
				if (part_points <= 0 || part_points > (int32_t)N25D_FILE_MAX_POINTS)
					goto fail;
				//Remaining fixed fields (17 ints) + vertex payload must fit
				if (dataLen + 17 * (int32_t)sizeof(int32_t) + part_points * 9 * (int32_t)sizeof(float) > dataLenMax)
					goto fail;
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].DrawOrder, dataLen);
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].x, dataLen); //x
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].y, dataLen); //y
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].z, dataLen); //z
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].width, dataLen);	//canvas width
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].height, dataLen);	//canvas height
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].anchor_x, dataLen);
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].anchor_y, dataLen);
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].ParentPartID, dataLen); //rotation parents
				if (p_n25dModel->nPart[cur_part].ParentPartID < 0 || p_n25dModel->nPart[cur_part].ParentPartID >= p_n25dModel->nPartCount)
					goto fail;
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].ParentPartIDC, dataLen); //rotation parents counter

				//Mesh (Copy data also to transform mesh)
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.pointcount, dataLen);
				if (p_n25dModel->nPart[cur_part].n25dMeshOrigin.pointcount != part_points)
					goto fail;
				p_n25dModel->nPart[cur_part].n25dMeshTransform.pointcount = p_n25dModel->nPart[cur_part].n25dMeshOrigin.pointcount;
				dataLen = n25dFileHelperGet_int32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.DrawOrder, dataLen);
				p_n25dModel->nPart[cur_part].n25dMeshTransform.DrawOrder = p_n25dModel->nPart[cur_part].n25dMeshOrigin.DrawOrder;
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.u, dataLen);
				p_n25dModel->nPart[cur_part].n25dMeshTransform.u = p_n25dModel->nPart[cur_part].n25dMeshOrigin.u;
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.v, dataLen);
				p_n25dModel->nPart[cur_part].n25dMeshTransform.v = p_n25dModel->nPart[cur_part].n25dMeshOrigin.v;
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.u_end, dataLen);
				p_n25dModel->nPart[cur_part].n25dMeshTransform.u_end = p_n25dModel->nPart[cur_part].n25dMeshOrigin.u_end;
				dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.v_end, dataLen);
				p_n25dModel->nPart[cur_part].n25dMeshTransform.v_end = p_n25dModel->nPart[cur_part].n25dMeshOrigin.v_end;
				//reallocate memory for all meshes
				n25dMeshReallocateMemory(&p_n25dModel->nPart[cur_part].n25dMeshOrigin, p_n25dModel->nPart[cur_part].n25dMeshOrigin.pointcount);
				n25dMeshReallocateMemory(&p_n25dModel->nPart[cur_part].n25dMeshTransform, p_n25dModel->nPart[cur_part].n25dMeshTransform.pointcount);
				if (p_n25dModel->nPart[cur_part].n25dMeshOrigin.x == NULL || p_n25dModel->nPart[cur_part].n25dMeshTransform.x == NULL)
					goto fail;
				//Get vertex data
				for (int i2 = 0; i2 < p_n25dModel->nPart[cur_part].pointcount; i2++)
				{
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.x[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.x[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.x[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.y[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.y[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.y[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.z[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.z[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.z[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.r[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.r[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.r[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.g[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.g[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.g[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.b[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.b[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.b[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.a[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.a[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.a[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.tu[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.tu[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.tu[i2];
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshOrigin.tv[i2], dataLen);
					p_n25dModel->nPart[cur_part].n25dMeshTransform.tv[i2] = p_n25dModel->nPart[cur_part].n25dMeshOrigin.tv[i2];
				}
				//Params for mesh
				int32_t MeshParamCount = 0;
				if (dataLen + (int32_t)sizeof(int32_t) > dataLenMax)
					goto fail;
				dataLen = n25dFileHelperGet_int32(dataC, &MeshParamCount, dataLen);
				if (MeshParamCount < 0 || MeshParamCount > (int32_t)N25D_FILE_MAX_PART_PARAMS)
					goto fail;
				//Each param: 4 floats per vertex + 2 angle floats
				if (dataLen + MeshParamCount * (part_points * 4 * (int32_t)sizeof(float) + 2 * (int32_t)sizeof(float)) > dataLenMax)
					goto fail;
				for (int i2 = 0; i2 < MeshParamCount; i2++)
				{
					n25dPartParamNew(&p_n25dModel->nPart[cur_part]);
					for (int i3 = 0; i3 < p_n25dModel->nPart[cur_part].n25dMeshOrigin.pointcount; i3++)
					{
						dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].x_hi[i3], dataLen);
						dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].x_low[i3], dataLen);
						dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].y_hi[i3], dataLen);
						dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].y_low[i3], dataLen);
					}
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].angle_hi, dataLen);
					dataLen = n25dFileHelperGet_f32(dataC, &p_n25dModel->nPart[cur_part].n25dMeshParam[i2].angle_low, dataLen);
				}
				break;
			}

			default:
				break;
		}
	}

	//Cross-check parsed counts against header (catches stream desync)
	if (p_n25dModel->nPartCount != Header.n25dModelPartCount ||
		p_n25dModel->nParamC != Header.n25dModelParamCount)
		goto fail;

	return p_n25dModel;

fail:
	n25dModelFreeInternal(p_n25dModel);
	return NULL;
}