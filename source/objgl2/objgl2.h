#ifndef OBJGL2_H_
#define OBJGL2_H_

#define OBJGL_FSTREAM 1
#define OBJGL_BSTREAM 0

#ifndef OBJGL_FSTREAM_IMPL
#define OBJGL_FSTREAM_IMPL 1
#endif

#include <stdint.h>

#ifndef OBJGL_H_
typedef struct{
	uint_least32_t *indices;
	uint_least32_t len;
	char *name;
} objgl2Material;

typedef struct{
	float *data;
	uint_least32_t *indices;
	objgl2Material *materials;
	uint_least32_t numIndices, numVertices, vertSize, numMaterials;
	unsigned char hasNormals, hasTexCoords;
	char *name;
} objgl2Data;
#endif

typedef struct __ObjGLStreamInfo objgl2StreamInfo;

typedef uint_least32_t (*objgl2_streamreader_ptr)(objgl2StreamInfo*);

struct __ObjGLStreamInfo{
	uint_least64_t fOffset; //file offset from beginning SEEK_SET
	uint_least32_t bufferLen; //buffer size
	uint_least32_t buffOffset; //buffer offset
	objgl2_streamreader_ptr function;
	char* filename; //null terminated file path
	char* buffer; //buffer for holding data
	char eof;
	char type;
};

#ifdef __cplusplus
extern "C"{
#endif

objgl2StreamInfo objgl2_init_bufferstream(char* buffer);
void objgl2_deletestream(objgl2StreamInfo* info);
uint_least32_t objgl2_bufferstreamreader(objgl2StreamInfo* info);

#ifdef OBJGL_FSTREAM_IMPL
#if OBJGL_FSTREAM_IMPL
uint_least32_t objgl2_filestreamreader(objgl2StreamInfo* info);
objgl2StreamInfo objgl2_init_filestream(char *filename, unsigned int bufferSize);
#endif
#endif

objgl2Data objgl2_readobj(objgl2StreamInfo *strinfo);
void objgl2_deleteobj(objgl2Data* obj);

#ifdef __cplusplus
}
#endif

#endif
