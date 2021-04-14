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
* No handling of .mtl files

## Data structures
```struct ObjGLData```
* `float *data` - pointer to an array of interleaved vertex attributes (position, [texcoord], [normal], position, [texcoord], [normal] ...)
* `unsigned int *indices` - pointer to an array of indices for use with ```glDrawElements```
* `objglMaterial *materials` - pointer to an array of materials
* `unsigned int`
*  * `numIndices` - total number of indices in the array
*  * `numVertices` - total number of unique indices (size of ```*data``` = numVertices * vertSize)
*  * `vertSize` - size of an one vertex in bytes (12, 20, 24 or 32)
*  * `numMaterials` - number of materials used
*  `unsigned char hasNormals` - boolean, whether the normal data is present
*  `unsigned char hasTexCoords` - boolean, whether the UV data is present
<br/><br/>
`struct objglMaterial`
* `unsigned int *indices` - pointer to the indices with that material, a part of ObjGLData's indices
* `unsigned int len` - number of indices with that material
* `char* name` - null terminated string representing a name of the material
<br/><br/>
### How do materials work?
Really simply - let's say we have a function `drawIndices(float *vertexData, unsigned int *indices, unsigned int numIndices)` and our model has 3 materials:
First we read the OBJ: <br/>
```
char* content = readtextfile("path/to/our/objfile.obj");
ObjGLData objdata = objgl_loadObj(content);
```
And now let's draw the entire model:
`drawIndices(objdata.data, objdata.indices, objdata.numIndices)` and here it is! 
Now let's just render the first material:
`drawIndices(objdata.data, objdata.materials[0].indices, objdata.materials[0].len)` once again, here it is, however now we see only the parts with the first material.

## Why is it like that?
It feels intuitive to me - if you're using an OpenGL OBJ loader, probably you need a data suitable for OpenGL, thus vertex parameter interleaving.
Probably you want to use different shader for different materials, thus such material system and if you don't care about materials,
you just don't care about them in code and just render the model in it's entirety.

## Can I use it for...
Yes, you can. Use it however you want, it's under MIT license so go ahead and read it's "rules". The best reward for me is someone actually using my code, not gotoshaming me for QUESTIONABLE coding style. I just want it to be fast. I always use this type of parsing whenever I need to parse a file, because it's quick, easy for me to understand, no string-splitting bloat, just you and code. Harmony.
