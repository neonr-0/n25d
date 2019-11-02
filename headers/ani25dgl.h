
void CreateGLn25dMesh(int32_t* objectID, n25dMesh* nMesh);

void SetGLn25dMeshPos(int objectID, n25dMesh* nMesh);
void SetGLn25dMeshPosTransform(int objectID, float x, float y);
void SetGLn25dMeshScale(int objectID, n25dMesh* nMesh, float width, float height);
void SetGLn25dMeshPosAngle(int objectID, float angle);
void SetGLn25dMeshPosAnchor(int objectID, float ax, float ay);
void SetGLn25dMeshRotaryParent(int objectID, int PobjectID);
//Texture
void SetGLn25dTexture(int32_t objectID, int TextureID);
void SetGLn25dMeshUV(int32_t objectID, n25dMesh* nMesh);
void AddGLn25dTexture(int32_t objectID, int32_t TextureID);
void SetGLn25dMeshTextureParam(int objectID, int textureNum, float texParam);