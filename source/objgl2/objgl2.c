#include "objgl2.h"
#include <stdlib.h>

#define OBJGL_DEBUG 0

#if OBJGL_DEBUG
static uint_fast32_t _debugmemory;
static uint_fast32_t _debugallocs;

#define objgl_free(ptr) free((void*)ptr)
#define objgl_alloc(size) malloc(size); _debugmemory += size; ++_debugallocs
#define objgl_realloc(ptr, size, oldsize) realloc((void*)ptr, size); _debugmemory += size - oldsize; ++_debugallocs
#else
#define objgl_free(ptr) free((void*)ptr)
#define objgl_alloc(size) malloc(size)
#define objgl_realloc(ptr, size, oldsize) realloc((void*)ptr, size)
#endif

typedef struct{
	float a[4];
} objgl_vec4;

typedef struct{
	float a[3];
} objgl_vec3;

typedef struct{
	float a[2];
} objgl_vec2;

typedef struct{
	uint_least32_t a[3];
} objfacevert;

typedef struct{
	objfacevert vert[3];
} objface;

typedef struct{
	objfacevert vert;
	uint_least64_t hash;
	uint_fast32_t index;
} objhashentry;

typedef struct{
	objfacevert* indices;
	char* name;
	uint_least32_t hash;
	uint_least32_t resIndices;
	uint_least32_t numIndices;
} objmaterial_internal;

typedef struct{
	objfacevert *cache;
	uint_least32_t cacheSize;
} objcache_internal;

typedef struct{
	uint_fast32_t numPositions, numNormals, numTexcoords;
	uint_fast32_t resPositions, resNormals, resTexcoords;
} objinfo_internal;

#ifdef __cplusplus
extern "C"{
#endif

uint_least64_t objgl2_hashfunc64(objfacevert vert){
	unsigned char *aschar = (unsigned char*)&vert;
	const uint_least64_t prime = 1099511628211;
	uint_least64_t hash = 14695981039346656037UL;

	for (uint_fast32_t i = 0; i < sizeof(vert); ++i) {
		hash ^= (uint_least64_t)aschar[i];
		hash *= prime;
	}

	return ++hash;
}

uint_least32_t objgl2_hashfunc32_string(const char* str, uint_fast32_t len){
	const uint_least32_t prime = 16777619;
	uint_least32_t hash = 2166136261;

	for (uint_fast32_t i = 0; i < len; ++i) {
		hash ^= (uint_least32_t)str[i];
		hash *= prime;
	}

	return ++hash;
}

uint_least32_t objgl_insert(objfacevert *vert, objhashentry *table, uint_least32_t size, uint_least32_t *unique){
	uint_least64_t hash = objgl2_hashfunc64(*vert);
	uint_least32_t index = hash % (uint_least64_t)size;

	unsigned int i = 0;
	while(table[index].hash != 0){
		if(table[index].hash == hash){
			return table[index].index;
		}
		index = (index + i*i) % size;
		++i;
	}

	table[index].hash = hash;
	table[index].vert = *vert;
	table[index].index = *unique;

	++*unique;

	return *unique - 1;
}

void objgl2_deletestream(objgl2StreamInfo* info){
	if(info->type == OBJGL_FSTREAM){
		objgl_free(info->buffer);
		objgl_free(info->filename);
	}

	*info = (const objgl2StreamInfo){0};
}

uint_least32_t objgl2_bufferstreamreader(objgl2StreamInfo* info){
	return 0xFFFFFFFF;
}

objgl2StreamInfo objgl2_init_bufferstream(char* buffer){
	objgl2StreamInfo s;
	s.buffOffset = 0;
	s.fOffset = 0;
	s.eof = 1;
	s.function = objgl2_bufferstreamreader;
	s.buffer = buffer;
	s.bufferLen = 0xFFFFFFFF;
	s.type = OBJGL_BSTREAM;

	return s;
}

void objgl2_deleteobj(objgl2Data* obj){
	objgl_free(obj->data);
	objgl_free(obj->indices);
	objgl_free(obj->name);

	for(uint_fast32_t i = 0; i < obj->numMaterials; ++i){
		objgl_free(obj->materials[i].name);
	}

	objgl_free(obj->materials);

	*obj = (const objgl2Data){0};
}

#ifdef OBJGL_FSTREAM_IMPL
#if OBJGL_FSTREAM_IMPL
#include <stdio.h>
objgl2StreamInfo objgl2_init_filestream(char *filename, unsigned int bufferSize){
	objgl2StreamInfo s;
	s.buffOffset = 0;
	s.fOffset = 0;
	s.eof = 0;
	s.function = objgl2_filestreamreader;
	s.buffer = (char*)objgl_alloc(bufferSize);
	s.bufferLen = bufferSize;
	s.type = OBJGL_FSTREAM;

	uint_fast32_t len = 0;
	while(filename[len]){++len;}

	s.filename = (char*)objgl_alloc(++len);

	for(; len + 1; --len){
		s.filename[len] = filename[len];
	}

	return s;
}

uint_least32_t objgl2_filestreamreader(objgl2StreamInfo* info){
	FILE *f = fopen(info->filename, "rb");

	info->buffOffset = 0;

	fseek(f, info->fOffset, SEEK_SET);
	fread(info->buffer, sizeof(char), info->bufferLen, f);

	uint_least64_t len = ftell(f) - info->fOffset;

	info->eof = feof(f);

	fclose(f);

	while(info->buffer[--len] != '\n' && len + 1 > 0); //read fragment must end with new line!!!

	info->fOffset += len + 1;

	return len;
}
#endif
#endif

char objgl_isnumber(char c){
	return (c > 47 && c < 58) || c == '-' || c == '.' || c == '+';
}

char objgl_isletter(char c){
	return (c > 64 && c < 91) || (c > 96 && c < 123) || c == '_';
}

char objgl_strcmp(char *a, char *b){
	uint_fast32_t ptr = 0;

	while(a[ptr] && b[ptr]){
		if(a[ptr] != b[ptr]){
			return 0;
		}
		++ptr;
	}

	return 1;
}

uint_least32_t objgl_strlen(char *a){
	uint_least32_t len = 0;

	while(a[len] != ' ' && a[len] != '\n' && a[len] != '\0'){
		++len;
	}

	return len;
}

float objgl_atof(char* data, uint_fast32_t *pos){
	static double fractLookup[16] = {
		0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001, 0.000000001, 0.0000000001,
		0.00000000001, 0.000000000001, 0.0000000000001, 0.00000000000001, 0.000000000000001, 0.0000000000000001
	};
	char sign = 1;
	uint_least32_t a = 0;
	float b = 0;

	while(data[*pos] == ' ' && data[*pos] != '\n' && data[*pos] != '\0'){++*pos;} //skip to number

	if(!objgl_isnumber(data[*pos])){
		return 0;
	}

	switch(data[*pos]){
	case '-':
		sign = -1;
	case '+':
		++*pos;
	default:
		break;
	}

	for(; objgl_isnumber(data[*pos]) && data[*pos] != '.'; ++*pos){
		a = a * 10 + data[*pos] - '0';
	}

	if(data[*pos]== '.'){
		++*pos;

		for(uint_fast32_t i = 0; objgl_isnumber(data[*pos]); ++*pos, ++i){
			b += (data[*pos] - '0') * fractLookup[i & 15] * (i < 10);
		}
	}

	return ((float)a + b) * sign;
}

uint_least32_t objgl_atoi(char* data, uint_fast32_t *pos){
	int_least32_t sign = 1;
	uint_least32_t a = 0;

	while(data[*pos] == ' ' && data[*pos] != '\n' && data[*pos] != '\0'){++*pos;} //skip to number

	if(!objgl_isnumber(data[*pos])){
		return 0;
	}

	switch(data[*pos]){
	case '-':
		sign = -1;
	case '+':
		++*pos;
	default:
		break;
	}

	for(; objgl_isnumber(data[*pos]) && data[*pos] != '.'; ++*pos){
		a = a * 10 + data[*pos] - '0';
	}

	if(data[*pos]== '.'){
		++*pos;

		for(; objgl_isnumber(data[*pos]); ++*pos){

		}
	}

	return (int_least32_t)a * sign;
}

objgl_vec4 objgl_parsevector(char* data, uint_fast32_t *pos){
	objgl_vec4 vector = {{1}};
	uint_least32_t component = 0;

	while(data[*pos] != '\n' && data[*pos] != '\0' && component < 3){
		vector.a[component] = objgl_atof(data, pos);
		++component;
	}

	while(data[*pos] != '\n' && data[*pos] != '\0'){++*pos;}
	++*pos;

	return vector;
}

uint_least32_t objgl_parseindices(char* data, uint_fast32_t *pos, objcache_internal *cache, objinfo_internal *info){
	uint_fast32_t numVerts = 0;
	uint_fast32_t component = 0;

	uint_fast32_t cacheSize = cache->cacheSize;
	char wasSpace = 0;

	while(data[*pos] == ' '){++*pos;}

	while(data[*pos] != '\n'){
		if(numVerts >= cacheSize){
			uint_fast32_t oldsize = cacheSize;
			cacheSize += 32;
			cache->cacheSize = cacheSize;
			cache->cache = (objfacevert*)objgl_realloc(cache->cache, cacheSize * sizeof(objfacevert), oldsize * sizeof(objfacevert));
		}

		if(wasSpace){
			wasSpace = 0;
			component = 0;
			++numVerts;
		}

		int_least32_t ind = objgl_atoi(data, pos);

		if(ind < 0){
			switch(component){
			case 0:
				ind = info->numPositions + ind + 1;
				break;
			case 1:
				ind = info->numTexcoords + ind + 1;
				break;
			case 2:
				ind = info->numNormals + ind + 1;
				break;
			default:
				break;
			}
		}

		cache->cache[numVerts].a[component] = ind;

		switch(data[*pos]){
		case ' ':
			wasSpace = 1;
			while(data[*pos] == ' '){++*pos;}
			break;
		case '/':
			++component;
			++*pos;
			break;
		}
	}

	return ++numVerts;
}

objgl2Data objgl2_readobj(objgl2StreamInfo *strinfoptr){
#if OBJGL_DEBUG
	_debugmemory = 0;
	_debugallocs = 0;
#endif
	objgl2_streamreader_ptr streamreader = strinfoptr->function;
	objgl2StreamInfo strinfo = *strinfoptr;
	uint_least32_t bufferLen = streamreader(&strinfo);
	uint_fast32_t buffPos = strinfo.buffOffset;

	objinfo_internal info = (const objinfo_internal){0, 0, 0, 256, 256, 256};

	objgl_vec3 *positions = (objgl_vec3*)objgl_alloc(sizeof(objgl_vec3) * info.resPositions);
	objgl_vec3 *normals = (objgl_vec3*)objgl_alloc(sizeof(objgl_vec3) * info.resNormals);
	objgl_vec2 *texcoords = (objgl_vec2*)objgl_alloc(sizeof(objgl_vec2) * info.resTexcoords);

	objmaterial_internal curMaterial = (const objmaterial_internal){0};
	curMaterial.resIndices = 256;
	curMaterial.numIndices = 0;
	curMaterial.indices = (objfacevert*)objgl_alloc(sizeof(objfacevert) * curMaterial.resIndices);

	uint_fast32_t resMaterials = 64;
	uint_fast32_t numMaterials = 1;
	objmaterial_internal *materials = (objmaterial_internal*)objgl_alloc(sizeof(objmaterial_internal) * resMaterials);
	materials[0] = curMaterial;

	uint_fast32_t matIndex = 0;

	void *xyz = objgl_alloc(sizeof(objfacevert) * 32);
	objcache_internal vertCache = (objcache_internal){(objfacevert*)xyz, 32};

	uint_fast32_t numIndices = 0;
	char* name = NULL;
	char hasTextures = 1;
	char hasNormals = 1;

	while(1){
		while(buffPos < bufferLen){
			switch(strinfo.buffer[buffPos]){
			case 'v':{
				++buffPos;
				unsigned int mode = 0;

				switch(strinfo.buffer[buffPos]){
				case 'n':
					++buffPos;
					mode = 1;
					break;
				case 't':
					++buffPos;
					mode = 2;
					break;
				default:
					break;
				}

				objgl_vec4 vec = objgl_parsevector(strinfo.buffer, &buffPos);

				switch(mode){
				case 0:
					if(info.numPositions >= info.resPositions){
						uint_fast32_t oldsize = info.resPositions;
						info.resPositions += info.resPositions;
						positions = (objgl_vec3*)objgl_realloc(positions, sizeof(objgl_vec3) * info.resPositions, sizeof(objgl_vec3) * oldsize);
					}

					positions[info.numPositions++] = *((objgl_vec3*)&vec);
					break;
				case 1:
					if(info.numNormals >= info.resNormals){
						uint_fast32_t oldsize = info.resNormals;
						info.resNormals += info.resNormals;
						normals = (objgl_vec3*)objgl_realloc(normals, sizeof(objgl_vec3) * info.resNormals, sizeof(objgl_vec3) * oldsize);
					}

					normals[info.numNormals++] = *((objgl_vec3*)&vec);
					break;
				case 2:
					if(info.numTexcoords >= info.resTexcoords){
						uint_fast32_t oldsize = info.resTexcoords;
						info.resTexcoords += info.resTexcoords;
						texcoords = (objgl_vec2*)objgl_realloc(texcoords, sizeof(objgl_vec2) * info.resTexcoords, sizeof(objgl_vec2) * oldsize);
					}

					texcoords[info.numTexcoords++] = *((objgl_vec2*)&vec);
					break;
				}
				break;
			}
			case 'o':{
				uint_fast32_t temp = buffPos;
				++temp;

				while(strinfo.buffer[temp] == ' '){++temp;}
				char* strstart = &strinfo.buffer[temp];

				if(name && !objgl_strcmp(strstart, name)){
					goto loopexit;
				} else if(!name){
					uint_least32_t strlen = objgl_strlen(strstart);
					name = (char*)objgl_alloc(strlen + 1);
					name[strlen] = '\0';
					for(; strlen; --strlen){
						name[strlen - 1] = strstart[strlen - 1];
					}

					buffPos = temp;
					while(strinfo.buffer[++buffPos] != '\n');
					++buffPos;
				}
				break;
			}
			case 'u':{
				const char *usemtl = "usemtl";
				char isTrue = 1;

				while(*usemtl && isTrue){
					isTrue = isTrue && (*usemtl == strinfo.buffer[buffPos++]);
					++usemtl;
				}

				if(!isTrue){
					while(strinfo.buffer[buffPos] != '\n'){++buffPos;}
					++buffPos;
					break;
				}

				while(strinfo.buffer[buffPos] == ' '){++buffPos;}

				char* matname = &strinfo.buffer[buffPos];
				uint_least32_t matnamelen = objgl_strlen(matname);
				uint_least32_t hash = objgl2_hashfunc32_string(matname, matnamelen);

				if(!curMaterial.name && !curMaterial.hash){
					curMaterial.hash = hash;
					curMaterial.name = (char*)objgl_alloc(matnamelen + 1);
					curMaterial.name[matnamelen] = '\0';

					for(; matnamelen; --matnamelen){
						curMaterial.name[matnamelen - 1] = matname[matnamelen - 1];
					}
				} else if(hash != curMaterial.hash){
					materials[matIndex] = curMaterial;

					char found = 0;
					for(matIndex = 0; matIndex < numMaterials; ++matIndex){
						if(materials[matIndex].hash == hash){
							found = 1;
							break;
						}
					}

					if(!found){
						if(numMaterials >= resMaterials){
							uint_fast32_t oldsize = resMaterials;
							resMaterials += resMaterials;
							materials = (objmaterial_internal*)objgl_realloc(materials, sizeof(objmaterial_internal) * resMaterials, sizeof(objmaterial_internal) * oldsize);
						}

						curMaterial.hash = hash;
						curMaterial.resIndices = 256;
						curMaterial.numIndices = 0;
						curMaterial.indices = (objfacevert*)objgl_alloc(sizeof(objfacevert) * curMaterial.resIndices);
						curMaterial.name = (char*)objgl_alloc(matnamelen + 1);
						curMaterial.name[matnamelen] = '\0';

						for(; matnamelen; --matnamelen){
							curMaterial.name[matnamelen - 1] = matname[matnamelen - 1];
						}
						matIndex = numMaterials;
						++numMaterials;
					} else{
						curMaterial = materials[matIndex];
					}
				}

				break;
			}
			case 'f':{
				++buffPos;

				uint_fast32_t numindices = objgl_parseindices(strinfo.buffer, &buffPos, &vertCache, &info);
				uint_fast32_t totalindices = (numindices - 2) * 3;
				uint_fast32_t tris = (numindices - 2);

				if(curMaterial.numIndices + totalindices >= curMaterial.resIndices){
					uint_fast32_t oldsize = curMaterial.resIndices;
					curMaterial.resIndices += curMaterial.resIndices + totalindices;
					curMaterial.indices = (objfacevert*)objgl_realloc(curMaterial.indices, sizeof(objfacevert) * curMaterial.resIndices, sizeof(objfacevert) * oldsize);
				}

				if(numindices < 4){
					curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[0];
					curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[1];
					curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[2];

					numIndices += 3;
				} else{
					for(uint_fast32_t i = 0, j = 1; i < tris; ++i){
						curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[0];
						curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[j];
						curMaterial.indices[curMaterial.numIndices++] = vertCache.cache[++j];
					}

					numIndices += totalindices;
				}

				hasNormals = hasNormals && vertCache.cache[0].a[2];
				hasTextures = hasTextures && vertCache.cache[0].a[1];

				break;
			}
			case '\0':
				goto loopexit;
			default:
				while(strinfo.buffer[buffPos] != '\n' && strinfo.buffer[buffPos] != '\0'){++buffPos;}
				++buffPos;
				break;
			}
		}

		if(!strinfo.eof){
			if(buffPos >= bufferLen){
				bufferLen = streamreader(&strinfo);
				buffPos = strinfo.buffOffset;
			}
		} else{
			goto loopexit;
		}
	}

	loopexit:
	materials[matIndex] = curMaterial;
	strinfo.buffOffset = buffPos;
	objgl_free(vertCache.cache);

	uint_least32_t uniques = 0;
	uint_least32_t *uniqueIndices = (uint_least32_t*)objgl_alloc(sizeof(uint_least32_t) * numIndices);
	objhashentry *hashtable = (objhashentry*)objgl_alloc(sizeof(objhashentry) * numIndices);
	objgl2Material *mats = (objgl2Material*)objgl_alloc(sizeof(objgl2Material) * numMaterials);

	for(uint_fast32_t i = 0, k = 0; i < numMaterials; ++i){
		objmaterial_internal mat = materials[i];
		mats[i].indices = &uniqueIndices[k];

		for(uint_fast32_t j = 0; j < mat.numIndices; ++j, ++k){
			uniqueIndices[k] = objgl_insert(&mat.indices[j], hashtable, numIndices, &uniques);
		}

		mats[i].len = mat.numIndices;
		mats[i].name = mat.name;
		objgl_free(mat.indices);
	}

	objgl_free(materials);

	unsigned int vertSize = sizeof(float) * (3 + 3 * hasNormals + 2 * hasTextures);
	void *data = objgl_alloc(vertSize * uniques);

	if(hasTextures && hasNormals){
		for(uint_fast32_t i = 0, j = 0; i < numIndices && j < uniques; ++i){
			objhashentry glhash = hashtable[i];

			if(glhash.hash){
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize)) = positions[glhash.vert.a[0] - 1];
				*((objgl_vec2*)((size_t)data + glhash.index * vertSize + sizeof(float) * 3)) = texcoords[glhash.vert.a[1] - 1];
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize + sizeof(float) * 5)) = normals[glhash.vert.a[2] - 1];
				++j;
			}
		}
	} else if(hasTextures){
		for(uint_fast32_t i = 0, j = 0; i < numIndices && j < uniques; ++i){
			objhashentry glhash = hashtable[i];

			if(glhash.hash){
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize)) = positions[glhash.vert.a[0] - 1];
				*((objgl_vec2*)((size_t)data + glhash.index * vertSize + sizeof(float) * 3)) = texcoords[glhash.vert.a[1] - 1];
				++j;
			}
		}
	} else if(hasNormals){
		for(uint_fast32_t i = 0, j = 0; i < numIndices && j < uniques; ++i){
			objhashentry glhash = hashtable[i];

			if(glhash.hash){
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize)) = positions[glhash.vert.a[0] - 1];
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize + sizeof(float) * 3)) = normals[glhash.vert.a[2] - 1];
				++j;
			}
		}
	} else{
		for(uint_fast32_t i = 0, j = 0; i < numIndices && j < uniques; ++i){
			objhashentry glhash = hashtable[i];

			if(glhash.hash){
				*((objgl_vec3*)((size_t)data + glhash.index * vertSize)) = positions[glhash.vert.a[0] - 1];
				++j;
			}
		}
	}

	objgl2Data obj;
	obj.data = (float*)data;
	obj.hasNormals = hasNormals;
	obj.hasTexCoords = hasTextures;
	obj.indices = uniqueIndices;
	obj.name = name;
	obj.numIndices = numIndices;
	obj.numMaterials = numMaterials;
	obj.numVertices = uniques;
	obj.materials = mats;
	obj.vertSize = vertSize;

	objgl_free(positions);
	objgl_free(normals);
	objgl_free(texcoords);
	objgl_free(hashtable);

	*strinfoptr = strinfo;
	return obj;
}

#ifdef __cplusplus
}
#endif
