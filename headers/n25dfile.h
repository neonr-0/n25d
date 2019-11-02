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
#ifndef N25D_FILE_H
#define N25D_FILE_H

//Definitions
#define n25dSignature 0xFE0D2525
#define n25dVersionFile 1UL
#define MAX_BUFFER 15360 //15Kb initial buffer
#define MAX_MODEL_NAME 8192

struct n25dFileHeader
{
	//file identify
	int32_t n25dIdentify;
	int32_t n25dFileVersion;
	//Part count
	int32_t n25dModelPartCount;
	//Parameters count
	int32_t n25dModelParamCount;
	//Model name
	int32_t Reserved;
};
typedef struct n25dFileHeader n25dFileHeader;

enum n25FileT
{
	n25FileT_ModelName,
	n25FileT_ModelParts,
	n25FileT_ModelPart_next,
	n25FileT_ModelParams,
	n25FileT_ModelParam_next,
	n25FileT_EOF,
};

//////////////////////////////////////////////////////////////////////////
// n25dFileOpenA(char* PathA, char const* modeA);
// return pointer of allocated memory for file save
//	parameters:
//				PathA - UTF8 path to open file descriptor
//				modeA - file access mode (stdio format rb - read, wb - write)
//////////////////////////////////////////////////////////////////////////
FILE* n25dFileOpenA(char* PathA, char const* modeA);
//////////////////////////////////////////////////////////////////////////
// n25dFileWrite(FILE* in_file, int8_t* in_data, int32_t in_dataLen);
// return zero on success or -1 on failed
//	parameters:
//				in_file - Opened file descriptor
//				in_data - pointer to data
//				in_dataLen - size in bytes to be written 
//////////////////////////////////////////////////////////////////////////
int32_t n25dFileWrite(FILE* in_file, int8_t* in_data, int32_t in_dataLen);
//////////////////////////////////////////////////////////////////////////
// n25dFileClose(FILE* in_file);
// return zero on success or EOF value on failed
//	parameters:
//				in_file - Opened file descriptor
//////////////////////////////////////////////////////////////////////////
int32_t n25dFileClose(FILE* in_file);
//////////////////////////////////////////////////////////////////////////
// n25dSaveToMemory(n25dModel* p_n25dModel);
// return pointer of allocated memory for file save
//	parameters:
//				p_n25dModel - pointer to model
//				out_dataLen - Length data in bytes
//////////////////////////////////////////////////////////////////////////
int8_t* n25dSaveToMemory(n25dModel* p_n25dModel, int32_t* out_dataLen);
//////////////////////////////////////////////////////////////////////////
// n25dSaveToFileA(n25dModel* p_n25dModel, char* PathA);
// return zero if success or -1 (int32_t) on error
//	parameters:
//				p_n25dModel - pointer to model
//				PathA - UTF8 path for saving file
//	notes:
//	need call free() after used
//////////////////////////////////////////////////////////////////////////
int32_t n25dSaveToFileA(n25dModel* p_n25dModel, char* PathA);
//////////////////////////////////////////////////////////////////////////
// n25dLoadFromMemory(int8_t* dataC, int32_t dataLenMax);
// return pointer to n25dModel
//	parameters:
//				dataC - pointer to data
//				dataLenMax - data size
//////////////////////////////////////////////////////////////////////////
n25dModel* n25dLoadFromMemory(int8_t* dataC, int32_t dataLenMax);
//////////////////////////////////////////////////////////////////////////
// n25dLoadFromFileA(n25dModel** p_n25dModel, char* PathA);
// return zero if success or -1 (int32_t) on error
//	parameters:
//				p_n25dModel - double pointer to model (return allocated model)
//				PathA - UTF8 path for reading file
//////////////////////////////////////////////////////////////////////////
int32_t n25dLoadFromFileA(n25dModel** p_n25dModel, char* PathA);

#endif /* N25D_FILE_H */