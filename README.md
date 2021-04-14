# Quite fast .OBJ loader written in C

## How to use it
Put ```.c``` and ```.h``` files in Your project and ```include``` them.
<br/>
```ObjGLData``` is a struct returned by ```objgl_loadObj(const char *buffer)```.
To load an .OBJ pass a const char \* pointing to a buffer containing the content of the file.<br/>
This library does not handle file reading.
<br/>
<br/>
To free the memory call ```objgl_delete(ObjGLData *data)```

## Features
* Quite fast - loading a 1 million vertices .OBJ takes 127,163,507 operations (take it with a grain of salt, I'm mostly talking about loop interations and stuff like that), uses peak ~331 megabytes of memory and on my CPU (AMD Ryzen 5 2600 Six-Core Processor) it takes 2.58287 seconds
* Spaghetti code
* Works with materials
* Outputs a ready by OpenGL to use data (interleaved positions, [texcoords], [normals]) and indices
* Uses hash table to pretty efficiently make vertices unique for space savings
* No dependencies other than ```stdlib.h``` and ```string.h```
* No string-splitting
## Not-so-much features
* Does not generate normals if not present in file (just for now)
* Can't process OBJs with multiple objects in them (I think just for now, you can hack it using materials)
* Spaghetti code

## Data structures
```ObjGLData```
* ```float *data``` - pointer to an array of interleaved vertex attributes (position, [texcoord], [normal], position, [texcoord], [normal] ...)
* ```unsigned int *indices``` - pointer to an array of indices for use with ```glDrawElements```
* ```objglMaterial *materials``` - pointer to an array of materials
* ```unsigned int```
*  * ```numIndices``` - total number of indices in the array
*  * ```numVertices``` - total number of unique indices (size of ```*data``` = numVertices * vertSize)
*  * ```vertSize``` - size of an one vertex in bytes (12, 20, 24 or 32)
*  * ```numMaterials``` - number of materials used
*  ```unsigned char hasNormals``` - boolean, whether the normal data is present
*  ```unsigned char hasTexCoords``` - boolean, whether the UV data is present
