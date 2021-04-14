#include "objgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __OBJGL_MODE_POSITION 0
#define __OBJGL_MODE_TEXTURECOORD 1
#define __OBJGL_MODE_NORMAL 2
#define __OBJGL_MAXVERTSPERFACE 32

#define __OBJGL_DEBUG 1

#if __OBJGL_DEBUG
static unsigned long long _debug_operations;
static unsigned int _debug_insertions;
static unsigned int _debug_allocations;
static unsigned int _debug_memoryusage;
#endif

#include <stdlib.h>
#include <string.h>
#if __OBJGL_DEBUG
#define OBJGL_FREE(ptr) free(ptr)
#define OBJGL_ALLOC(size) malloc(size); _debug_memoryusage += size; ++_debug_allocations
#define OBJGL_REALLOC(ptr, newsize, oldsize) realloc(ptr, newsize); _debug_memoryusage += newsize - oldsize; ++_debug_allocations
#else
#define OBJGL_FREE(ptr) free(ptr)
#define OBJGL_ALLOC(size) malloc(size)
#define OBJGL_REALLOC(ptr, newsize, oldsize) realloc(ptr, newsize)
#endif

typedef struct{
	unsigned int a[3];
} objfacevert;

typedef struct{
	float a[4];
} objvec4;

typedef struct{
	float a[3];
} objvec3;

typedef struct{
	float a[2];
} objvec2;

typedef struct{
	objfacevert vert[3];
} objface;

typedef struct{
	objfacevert vert;
	unsigned long long hash;
	unsigned int index;
} objhashentry;

typedef struct{
	objface* pointer;
	char* name;
	unsigned int hash;
	unsigned int resLen;
	unsigned int len;
} objmaterial_internal;

void objgl_delete(ObjGLData* data){
	OBJGL_FREE(data->data);
	OBJGL_FREE(data->indices);

	for(unsigned int i = 0; i < data->numMaterials; ++i){
		OBJGL_FREE(data->materials[i].name);
	}

	OBJGL_FREE(data->materials);

	data->data = NULL;
	data->indices = NULL;
	data->materials = NULL;
	data->numIndices = 0;
	data->numVertices = 0;
	data->vertSize = 0;
	data->hasNormals = 0;
	data->hasTexCoords = 0;
	data->numMaterials = 0;
}

char objgl_checkChar(char c){
	switch(c){
	case '-':
		return 1;
	case '+':
		return 1;
	case '.':
		return 1;
	case '_':
		return 1;
	case '\\':
		return 2;
	case '/':
		return 2;
	case '\n':
		return 3;
	case '\0':
		return 3;
	case ' ':
		return 4;
	default:
		return (c > 47 && c < 58) || (c > 64 && c < 91) || (c > 96 && c < 123);
	}
}

//FNV 64bit
unsigned long long objgl_hashfunc64(objfacevert vert){
	unsigned char *aschar = (unsigned char*)&vert;
	const unsigned long long prime = 1099511628211;
	unsigned long long hash = 14695981039346656037UL;

	for (unsigned int i = 0; i < sizeof(vert); ++i) {
		hash ^= (unsigned long long)aschar[i];
		hash *= prime;
	}

	return ++hash;
}

unsigned int objgl_hashfunc32_string(const char* str, int len){
	const unsigned int prime = 16777619;
	unsigned int hash = 2166136261;

	for (unsigned int i = 0; i < (unsigned int)len && str[i]; ++i) {
		hash ^= (unsigned int)str[i];
		hash *= prime;
	}

	return ++hash;
}

unsigned int hashinsert(objfacevert vert, objhashentry *table, unsigned int size, unsigned int *unique){
	unsigned long long hash = objgl_hashfunc64(vert);
	unsigned int index = hash % (unsigned long long)size;

#if __OBJGL_DEBUG
	++_debug_operations;
	++_debug_insertions;
#endif

	unsigned int i = 0;
	while(table[index].hash != 0){
		if(table[index].hash == hash){
			return table[index].index;
		}
		index = (index + i*i) % size;
		++i;
#if __OBJGL_DEBUG
		++_debug_operations;
#endif
	}

	table[index].hash = hash;
	table[index].vert = vert;
	table[index].index = *unique;

	++*unique;

	return *unique - 1;
}

ObjGLData objgl_loadObj(const char* obj){
#if __OBJGL_DEBUG
	_debug_operations = 0;
	_debug_insertions = 0;
	_debug_allocations = 0;
	_debug_memoryusage = 0;
#endif
	unsigned int mode;

	char hasNormals = 0;
	char hasTex = 0;

	unsigned int numPositions = 0;
	unsigned int resPositions = 64;
	objvec3 *positions = (objvec3*)OBJGL_ALLOC(sizeof(objvec3) * resPositions);

	unsigned int numNormals = 0;
	unsigned int resNormals = 64;
	objvec3 *normals = (objvec3*)OBJGL_ALLOC(sizeof(objvec3) * resNormals);

	unsigned int numTexcoords = 0;
	unsigned int resTexcoords = 64;
	objvec2 *texcoords = (objvec2*)OBJGL_ALLOC(sizeof(objvec2) * resTexcoords);

	unsigned int numFaces = 0;

	unsigned int resMaterials = 8;
	unsigned int numMaterials = 0;
	objmaterial_internal *materials = OBJGL_ALLOC(sizeof(objmaterial_internal) * resMaterials);
	objmaterial_internal curMaterial = materials[0];
	curMaterial.resLen = 64;
	curMaterial.len = 0;
	curMaterial.hash = 0;
	curMaterial.name = NULL;
	curMaterial.pointer = OBJGL_ALLOC(sizeof(objface) * curMaterial.resLen);
	unsigned int curMatIndex = 0;

	objfacevert *fvertcache = OBJGL_ALLOC(sizeof(objfacevert) * __OBJGL_MAXVERTSPERFACE);

	for(unsigned int i = 0; i < __OBJGL_MAXVERTSPERFACE; ++i){
		fvertcache[i] = (objfacevert){{0, 0, 0}};

#if __OBJGL_DEBUG
		++_debug_operations;
#endif
	}

	char cache[64];

	/*
	 * Parsation!!!!
	 */

	unsigned int pointer = 0;
	while(1){
#if __OBJGL_DEBUG
		++_debug_operations;
#endif
		switch(obj[pointer]){
		case 'v':{
		switch(obj[++pointer]){
			case 't':
				mode = __OBJGL_MODE_TEXTURECOORD;
				++pointer;
				break;
			case 'n':
				mode = __OBJGL_MODE_NORMAL;
				++pointer;
				break;
			default:
				mode = __OBJGL_MODE_POSITION;
			}

			unsigned int start, len;

			objvec4 v = {{1, 1, 1, 1}};
			int component = 0;

			while(obj[pointer] != '\n'){
					while(1){
						switch(objgl_checkChar(obj[pointer])){
						case 4:
						case 0:
							++pointer;
							break;
						default:
							goto exitloop1;
						}

#if __OBJGL_DEBUG
						++_debug_operations;
#endif
					}
					exitloop1:
					start = pointer;
					while(1){
						switch(objgl_checkChar(obj[pointer])){
						case 1:
							++pointer;
							break;
						default:
							goto exitloop2;
						}
#if __OBJGL_DEBUG
						++_debug_operations;
#endif
					}
					exitloop2:
					len = pointer - start;

					if(len > 0 && component < 4){
						memcpy(cache, &obj[start], len);
						cache[len] = '\0';
						v.a[component++] = atof(cache);
					}
				}

				switch(mode){
				case __OBJGL_MODE_POSITION:
					if(numPositions >= resPositions){
						unsigned int oldSize = resPositions;
						resPositions *= 2; positions = OBJGL_REALLOC(positions, sizeof(objvec3) * resPositions, sizeof(objvec3) * oldSize);
					}
					positions[numPositions++] = *((objvec3*)&v);

					break;
				case __OBJGL_MODE_NORMAL:
					if(numNormals >= resNormals){
						unsigned int oldSize = resNormals;
						resNormals *= 2; normals = OBJGL_REALLOC(normals, sizeof(objvec3) * resNormals, sizeof(objvec3) * oldSize);
					}
					normals[numNormals++] = *((objvec3*)&v);
					break;
				case __OBJGL_MODE_TEXTURECOORD:
					if(numTexcoords >= resTexcoords){
						unsigned int oldSize = resTexcoords;
						resTexcoords *= 2; texcoords = OBJGL_REALLOC(texcoords, sizeof(objvec2) * resTexcoords, sizeof(objvec2) * oldSize);
					}
					texcoords[numTexcoords++] = *((objvec2*)&v);
					break;
				}
			}

			break;
		case 'f':{
				++pointer;

				unsigned int component = 0;
				unsigned int numVerts = 0;
				char advanceVertex;

				while(obj[pointer] != '\n'){
					while(1){
						switch(objgl_checkChar(obj[pointer])){
						case 4:
						case 0:
							++pointer;
							break;
						case 2:
							++component;
							++pointer;
							goto exitloop3;
						default:
							goto exitloop3;
						}
#if __OBJGL_DEBUG
						++_debug_operations;
#endif
					}
					unsigned int start, len;
					exitloop3:
					start = pointer;

					while(1){
						switch(objgl_checkChar(obj[pointer])){
						case 1:
							++pointer;
							break;
						case 4:
							advanceVertex = 1;
						default:
							goto exitloop4;
						}
#if __OBJGL_DEBUG
						++_debug_operations;
#endif
					}
					exitloop4:
					len = pointer - start;

					if(len > 0){
						cache[len] = '\0';
						memcpy(cache, &obj[start], len);

						fvertcache[numVerts].a[component] = atoi(cache); //0 is illegal state
					}

					if(advanceVertex){
						component = 0;
						advanceVertex = 0;
						++numVerts;

						fvertcache[numVerts].a[1] = 0;
						fvertcache[numVerts].a[2] = 0;
					}
				}

				int xyzw = numVerts - 1;

				if(curMaterial.len + xyzw >= curMaterial.resLen){
					unsigned int oldSize = curMaterial.resLen;
					curMaterial.resLen += curMaterial.resLen + xyzw;
					curMaterial.pointer = OBJGL_REALLOC(curMaterial.pointer, curMaterial.resLen * sizeof(objface), oldSize * sizeof(objface));
				}

				if(numVerts < 4){
					curMaterial.pointer[curMaterial.len].vert[0] = fvertcache[0];
					curMaterial.pointer[curMaterial.len].vert[1] = fvertcache[1];
					curMaterial.pointer[curMaterial.len].vert[2] = fvertcache[2];
					++curMaterial.len;

					++numFaces;
					hasTex = hasTex || fvertcache[0].a[1] || fvertcache[1].a[1] || fvertcache[2].a[1];
					hasNormals = hasNormals || fvertcache[0].a[2] || fvertcache[1].a[2] || fvertcache[2].a[2];
				}
				//triangulate!!!!!!!!!!!!!!!!!

			}

			break;
		case 'u':
			++pointer;
			const char *usemtl = "semtl";
			char isUseMtl = 1;
			for(unsigned int i = 0; i < 5 && isUseMtl; ++i, ++pointer){
				isUseMtl = isUseMtl && obj[pointer] == usemtl[i];
			}

			if(isUseMtl){
				while(1){
					switch(objgl_checkChar(obj[pointer])){
					case 4:
					case 0:
						++pointer;
						break;
					case 2:
						++pointer;
						goto exitloop5;
					default:
						goto exitloop5;
					}
#if __OBJGL_DEBUG
					++_debug_operations;
#endif
				}
				unsigned int start, len;
				exitloop5:
				start = pointer;
				while(1){
					switch(objgl_checkChar(obj[pointer])){
					case 1:
						++pointer;
						break;
					default:
						goto exitloop6;
					}
#if __OBJGL_DEBUG
					++_debug_operations;
#endif
				}
				exitloop6:
				len = pointer - start;

				unsigned int hash = objgl_hashfunc32_string(&obj[start], len);
				if(hash != curMaterial.hash){
					materials[curMatIndex] = curMaterial;

					char exists = 0;
					for(int i = 0; i < numMaterials && !exists; ++i){
						exists = materials[i].hash == hash;
						curMatIndex = i;
					}

					curMaterial = materials[curMatIndex];

					if(!exists){
						if(numMaterials + 1 >= resMaterials){
							unsigned int oldSize = resMaterials;
							resMaterials *= 2;
							materials = OBJGL_REALLOC(materials, sizeof(objmaterial_internal) * resMaterials, sizeof(objmaterial_internal) * oldSize);
						}

						char* name = OBJGL_ALLOC(len + 1);
						name[len] = '\0';
						memcpy(name, &obj[start], len);

						curMaterial = materials[numMaterials];
						curMaterial.hash = hash;
						curMaterial.name = name;

						if(numMaterials > 1){
							curMaterial.resLen = 64;
							curMaterial.len = 0;
							curMaterial.pointer = OBJGL_ALLOC(sizeof(objface) * curMaterial.resLen);
						}

						curMatIndex = numMaterials;

						++numMaterials;
					}
				}

			}

			while(obj[pointer] != '\n'){
				++pointer;
			};
			break;
		case '\0':
			goto exitloop;
		default:
			while(obj[++pointer] != '\n');
		}

		++pointer;
	}
	exitloop:
	OBJGL_FREE(fvertcache);
	materials[curMatIndex] = curMaterial;

	numMaterials += !numMaterials; //if 0 then 1

	/*
	 * End of parsing!!!!!
	 */

	unsigned int indices = numFaces * 3;
	objhashentry* hashtable = (objhashentry*)OBJGL_ALLOC(sizeof(objhashentry) * indices);
	unsigned int uniques = 0;

	unsigned int *uniqueIndices = (unsigned int*)OBJGL_ALLOC(sizeof(unsigned int) * indices);

	for(unsigned int i = 0; i < indices; ++i){
		hashtable[i].hash = 0;
#if __OBJGL_DEBUG
		++_debug_operations;
#endif
	}

	objglMaterial *mats = OBJGL_ALLOC(sizeof(objglMaterial) * numMaterials);

	for(unsigned int i = 0, k = 0; i < numMaterials; ++i){
		objmaterial_internal current = materials[i];
		mats[i] = (objglMaterial){&uniqueIndices[k], current.len * 3, current.name};
		for(unsigned int j = 0; j < current.len; ++j, k += 3){
			uniqueIndices[k] = hashinsert(current.pointer[j].vert[0], hashtable, indices, &uniques);
			uniqueIndices[k + 1] = hashinsert(current.pointer[j].vert[1], hashtable, indices, &uniques);
			uniqueIndices[k + 2] = hashinsert(current.pointer[j].vert[2], hashtable, indices, &uniques);

#if __OBJGL_DEBUG
			++_debug_operations;
#endif
		}
		OBJGL_FREE(current.pointer);
	}

	unsigned int vertSize = sizeof(float) * (3 + 3 * hasNormals + 2 * hasTex);
	char* data = OBJGL_ALLOC(vertSize * uniques);

	if(hasNormals && !hasTex){
		for(unsigned int i = 0, added = 0; i < indices && added < uniques; ++i){
			objhashentry *temp = &hashtable[i];
			unsigned int index = temp->index;

			if(temp->hash){
				*((objvec3*)(data + index*vertSize)) = positions[temp->vert.a[0] - 1];
				*((objvec3*)(data + index*vertSize + sizeof(float) * 3)) = normals[temp->vert.a[2] - 1];
				++added;
			}
#if __OBJGL_DEBUG
			++_debug_operations;
#endif
		}
	}else if(hasTex && !hasNormals){
		for(unsigned int i = 0, added = 0; i < indices && added < uniques; ++i){
			objhashentry *temp = &hashtable[i];
			unsigned int index = temp->index;

			if(temp->hash){
				*((objvec3*)(data + index*vertSize)) = positions[temp->vert.a[0] - 1];
				*((objvec2*)(data + index*vertSize + sizeof(float) * 3)) = texcoords[temp->vert.a[1] - 1];
				++added;
			}
#if __OBJGL_DEBUG
			++_debug_operations;
#endif
		}
	}else if(hasTex && hasNormals){
		for(unsigned int i = 0, added = 0; i < indices && added < uniques; ++i){
			objhashentry *temp = &hashtable[i];
			unsigned int index = temp->index;

			if(temp->hash){
				*((objvec3*)(data + index*vertSize)) = positions[temp->vert.a[0] - 1];
				*((objvec2*)(data + index*vertSize + sizeof(float) * 3)) = texcoords[temp->vert.a[1] - 1];
				*((objvec3*)(data + index*vertSize + sizeof(float) * 5)) = normals[temp->vert.a[2] - 1];
				++added;
			}
#if __OBJGL_DEBUG
			++_debug_operations;
#endif
		}
	}else{
		for(unsigned int i = 0, added = 0; i < indices && added < uniques; ++i){
			objhashentry *temp = &hashtable[i];
			unsigned int index = temp->index;

			if(temp->hash){
				*((objvec3*)(data + index*vertSize)) = positions[temp->vert.a[0] - 1];
				++added;
			}
#if __OBJGL_DEBUG
			++_debug_operations;
#endif
		}
	}

	OBJGL_FREE(hashtable);
	OBJGL_FREE(positions);
	OBJGL_FREE(normals);
	OBJGL_FREE(texcoords);

	ObjGLData final;
	final.data = (float*)data;
	final.indices = uniqueIndices;
	final.numIndices = indices;
	final.numVertices = uniques;
	final.vertSize = vertSize;
	final.hasNormals = hasNormals;
	final.hasTexCoords = hasTex;
	final.numMaterials = numMaterials;
	final.materials = mats;

	return final;
}

#ifdef __cplusplus
}
#endif



