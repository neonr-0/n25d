#pragma once
#include "ani25d.h"


void CreateDX9n25dMesh(int* objectID, n25dMesh* nMesh);
void SetDX9n25dMeshPos(int objectID, n25dMesh* nMesh, float x, float y);
void SetDX9n25dMeshDataXY(n25dMesh* nMesh);
void SetDX9n25dMeshUV(int objectID, n25dMesh* nMesh);
void SetDX9n25dTexture(int objectID, int TextureID);
void AddDX9n25dTexture(int objectID, int TextureID);

void SetDX9n25dMeshTextureParam(int objectID, int textureNum, float texParam);
//Transformations
void SetDX9n25dMeshPosTransform(int objectID, float x, float y);
void SetDX9n25dMeshScale(int objectID, n25dMesh* nMesh, float width, float height);
void SetDX9n25dMeshPosAngle(int objectID, float angle);
void SetDX9n25dMeshPosAnchor(int objectID, float ax, float ay);
void SetDX9n25dMeshRotaryParent(int objectID, int PobjectID);

void SetDX9n25dFlip(int objectID, n25dMesh* nMesh, int flip_mode);