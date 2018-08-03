@echo off

REM For Visual Studio Code
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
w:;cd handmade\code;

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4505 -wd4456 -wd4201 -wd4100 -wd4189 -wd4459 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib Winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
del *.pdb >NUL 2> NUL
REM Optimization switches /O2 /Oi /fp:fast
cl %CommonCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%

popd