# OBJ GL Loader v2
## Quite fast .OBJ loader written in C

## How to use it
Put ```objgl2.c``` and ```objgl2.h``` files in Your project and ```include``` them.
<br/>
To put it simply:
```
//!!!Buffersize must not be smaller than the line length!
//set it to at least 1K bytes, just to be safe
unsigned int bufferSize = 65536;
objgl2StreamInfo strinfo = objgl2_init_filestream("file/name.obj", bufferSize);
objgl2Data objd = objgl2_readobj(&strinfo);

//cleanup
objgl2_deletestream(&strinfo);
objgl2_deleteobj(&objd);
```
In comparison to the older version, the newer one is based on the idea of streaming - either from a file or a buffer.
I'll talk about it in a second. The example above reads the .OBJ file from the file in ~65535 bytes portions but you don't need to read it from file, you can read the .OBJ data from a regular char buffer:
```
char *rawobj = your_filereading_function("file/name.obj");

objgl2StreamInfo strinfo = objgl2_init_bufferstream(rawobj);
objgl2Data objd = objgl2_readobj(&strinfo);
```
Here the stream is created from an already existing buffer.
<br/>
If, for some reason, you don't want to have a default C FILE streaming implementation just ```#define OBJGL_FSTREAM_IMPL 0``` before
including the header file but in that case you'll need to implement own file streaming.

## Features
* For some reason the newer version is faster. Not much but a little bit faster!
* Loads an ~80MB .obj file with 5,626,896 indices in around 2 sec on my machine (AMD Ryzen 5 2600 Six-Core Processor, no compiler optimizations, debug mode, streaming from file, buffer size - 65536 bytes)
* Uses peak 432.964761 MB of memory for loading the said file and 522 allocations/reallocations
* Puts the data in an OpenGL-friendly way (interleaved vertex attributes, indices)
* OpenGL-friendly, easy to use material system
* Uses a hash table to find unique vertices
* Made for indexed rendering (```glDrawElements```)
* Triangulates the faces if needed
* NEGATIVE INDICES!!! Yaaaay!
* Smooth shading, flat shading, auto-smooth, it's not a problem, just remember to generate the normals to the file
The file used for tests was ```vokselia_spawn.obj``` from https://casual-effects.com/data/
## Not-so-much features
* Does not support multiple objects in one file (at the moment, I'll fix it)
* Uses "triangle fan" triangulation algorithm (glitches may appear if the face is not convex)
* Does not generate the normals if not present in file
* Does not care about smoothing groups, flat shading, smooth shading, if no normals are present in the file

## Streams and buffers
As I said above, there are two ways of "feeding" the parser with a data - a file stream or a buffer stream.
It's important to note that the buffer stream is not really a stream.
When to use file stream over the buffer stream? Sometimes you deal with a large file, hundreds of megabytes. Malloc might not be able
to allocate that much memory or there are other reasons not to allocate that much at once, so the idea of streaming comes in handy.
Instead of reading the entire file at once and storing the result in one big buffer the program reads n bytes of data from a file, puts it into
a buffer and then the OBJ Loader processes that buffer. Once it gets to the end of the buffer it requests more data, and the stream reader gets more
bytes from the file to the buffer. <br/><br/>
Reading from an already existing buffer supplied by the programmer is a little bit different - here the "streaming" mechanism is just a wrapper and there's no streaming at all. The OBJ Loader requests the data from the streamer and it gives it an entire buffer at once, so there's no overhead of fetching small portions of data.
<br/><br/>
And what about the buffer size? I recommend you to set it as big as possible - the bigger the buffer the less fetching is done. The OBJ Loader also requires the streamer to return whole lines and if your buffer size is smaller than the length of the line - it won't work! During the tests 10 bytes were too small but 100 bytes did the job, however as I said, set the buffer size as big as possible, 10K, 65K would be optimal.

## How do materials work?
Really simply - let's say we have a function `drawIndices(float *vertexData, unsigned int *indices, unsigned int numIndices)` and our model has 3 materials:
First we read the OBJ: <br/>
And now let's draw the entire model:
`drawIndices(objdata.data, objdata.indices, objdata.numIndices)` and here it is! 
Now let's just render the first material:
`drawIndices(objdata.data, objdata.materials[0].indices, objdata.materials[0].len)` once again, here it is, however now we see only the parts with the first material.

## Implementing your own file streamer
If you think you can do better than me (yes, probably you can) or you just don't want to use the C way of reading files, you can make your own stream reader.
The stream reading function pointer looks like that: ```uint_least32_t (*objgl2_streamreader_ptr)(objgl2StreamInfo*)``` and the declaration of the default
file streamer is ```uint_least32_t objgl2_filestreamreader(objgl2StreamInfo* info)```.
Only four requirements are:
* The streamer must return the buffer length (buffer length is not necessarily equal to bufferLen)
* The streamer must not read more bytes than the bufferSize
* The streamer must read whole lines, thus it will most likely read fewer bytes than the bufferLen, because it cannot read more than that
* The streamer must set the eof field to a non-zero value if the end of the file is reached (set the eof to true basicaly).
My implementation does it this way: it reads as many bytes (= bufferLen) as it can with ```fread```. It goes back from the end of the buffer until it sees the ```\n```. If the buffer already ends with ```\n``` then the length does not change. It returns the length of the buffer (distance from the start of the buffer to the last ```\n```). It's as simple as that. The buffer reader is even simple - it does nothing.
<br/>
```objgl2_init_bufferstream``` and ```objgl2_init_filestream``` are there just to facilitate the creation of the stream info struct.
You'll need to initialize your stream info struct yourself or just change the ```objgl2_streamreader_ptr function``` after initializing the struct.
The code speaks louder than my convoluted descriptions, so please look it up.

## Why is it like that?
It feels intuitive to me - if you're using an OpenGL OBJ loader, probably you need a data suitable for OpenGL, thus vertex parameter interleaving.
Probably you want to use different shader for different materials, thus such material system and if you don't care about materials,
you just don't care about them in code and just render the model in it's entirety.

## Can I use it for...
Yes, you can. Use it however you want, it's under MIT license so go ahead and read it's "rules". The best reward for me is someone actually using my code, not gotoshaming me for QUESTIONABLE coding style. I just want it to be fast. I always use this type of parsing whenever I need to parse a file, because it's quick, easy for me to understand, no string-splitting bloat, just you and code. Harmony.
