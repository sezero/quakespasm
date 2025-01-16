  Quake Extensions

  ______________________________________________________________________

  1. About

  Quake Extensions (qextensions) are Dynamic Link Library (DLL) files on
  Windows and Shared Object (SO) files on Linux. They allows external
  code to be executed within QC programs. 
  
  In theory it works on macOS using the Dynamic Librariy (dylib), but I do 
  not have a Mac to test with.

  2. Usage

  In order to use qextensions the new built-in functions must be defined.
  This usually happens in `defs.qc`:

    ...
    float(string path) OpenExtension = #93;
    string(float id, string cmd, string arg) CallExtensionString = #94;
    float(float id, string cmd, float arg) CallExtensionNumber = #95;
    vector(float id, string cmd, vector arg) CallExtensionVector = #96;
    float(float id, string cmd, entity arg) CallExtensionEntity = #97;
    ...

  To use a specific extension e.g. `hello`, the following steps needs to
  be made:

    1, Open/load the extension. This needs to be done only once.
       The `void() worldspawn` is a perfect place to do that:

        ....
        float hello_ext;
        ...
        void() worldspawn = 
        {
            ...
            hello_ext = OpenExtension("hello");
            ...
        }
        ...

        Note that the value returned from `OpenExtension` is the
        extension descriptor. It needs to be saved globally, since
        it will be used for calling the extension.

    2, Call the extension functions.

        ...
        num = CallExtensionNumber(hello_ext, "hello", num);
        ...

        This call is blocking, meaning that the game will wait for the
        extension to return before continuing. This may cause FPS drop
        if extension is not optimised. If extension takes too long, 
        consider making asynchronous extension, where the result of 
        the work of the extension is collected in a separate call.

  There are 4 function that can be called:

    + string(float id, string cmd, string arg) CallExtensionString;
    + float(float id, string cmd, float arg) CallExtensionNumber;
    + vector(float id, string cmd, vector arg) CallExtensionVector;
    + float(float id, string cmd, entity arg) CallExtensionEntity;

  The functions can not modify their arguments.
  More then one qextension can be used simultaneously.
    
  3. Creating

  qextensions can be created using any programming language that can 
  output native .DLL/.SO files. This example code is written in C.

  The qextension must export exactly 5 functions:

  + int qextension_version (void)
  + const char* qextension_string (const char *cmd, const char *arg)
  + float qextension_number (const char *cmd, float arg)
  + float* qextension_vector (const char *cmd, vec3_t arg)
  + float qextension_entity (const char *cmd, edict_t *arg)

  In Windows the function must be preceded with `__declspec (dllexport)`
  if MSVC is used.

  The `qextension_version` must return 1. It is the version of the 
  qextension system, not the specific extension.

  Since the qextension is a native file, different platforms require 
  different library files. OpenExtension("hello") will try to find
  the underlying library file depending on the platform quakespasm
  is running on: 

    + hello_x86.dll on 32 bit Windows systems
    + hello_x86_64.dll on 64 bit Windows systems
    + hello_x86.so on 32 bit Linux systems
    + hello_x86_64.so on 64 bit Linux systems

  If support of multiple platforms is required, each library file must 
  be installed. This requires cross-compilation or different build
  systems.

  4. Installation

  qextensions use the filesystem of Quake. They can be placed into the 
  game directory or embedded into a .PAK file, e.g.:

    id1/hello_x86.dll -> OpenExtension("hello");
    id1/pak0.pak/hello_x86_64.dll -> OpenExtension("hello");
    id1/ext/hello_x86.dll -> OpenExtension("ext/hello");

    mymod/hello_x86_64.so -> OpenExtension("hello"); 
    (only if `-game mymod` was used)

  5. qext_hello

  This is an example qextension. It supports 64 bit Linux and Windows
  systems.

    src/  contains the source code of the qextension
    qsrc/ contains the source code of the mod uses the qextension

  It requres gcc and gcc-mingw-w64 to compile on Linux.
  It can be compiled using Visual Studio on Windows.
  It requres qcc to compile progs.dat.
