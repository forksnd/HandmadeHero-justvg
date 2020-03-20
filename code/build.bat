@echo off

REM For Visual Studio Code
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
c:;cd Users\georg\source\repos\HandmadeHero\handmade\code

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -fp:fast -Od -Oi -WX -W4 -wd4324 -wd4312 -wd4505 -wd4456 -wd4457 -wd4201 -wd4100 -wd4189 -wd4459 -wd4127 -wd4311 -wd4302 -FC -Z7 
set CommonCompilerFlags=-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 %CommonCompilerFlags%
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib Winmm.lib opengl32.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

del *.pdb >NUL 2> NUL

REM Simple preprocessor
REM cl %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS ..\handmade\code\simple_preprocessor.cpp /link %CommonLinkerFlags%
REM pushd ..\handmade\code
REM ..\..\build\simple_preprocessor.exe > handmade_generated.h
REM popd

REM Asset file builder build
REM cl %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS ..\handmade\code\test_asset_builder.cpp /link %CommonLinkerFlags%

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
REM Optimization switches /O2 
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -I..\iaca-win64\ ..\handmade\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples -EXPORT:DEBUGFrameEnd
del lock.tmp
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%

popd