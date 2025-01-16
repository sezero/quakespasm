echo off

mkdir hello 2> NUL

cd qsrc
qcc || goto :error
copy "progdefs.h" "..\src\progdefs.h" || goto :error
copy "progs.dat" "..\hello\progs.dat" || goto :error
cd ..

cd src
cl.exe /DWIN32 /D_MSVC_VER hello.c /link /DLL /OUT:hello_x86_64.dll
copy "hello_x86_64.dll" "..\hello\hello_x86_64.dll" || goto :error
cd ..

goto :EOF

:error
echo ---------- ERROR ----------
cd ..
exit /b 1