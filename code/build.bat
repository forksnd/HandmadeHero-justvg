@echo off

set CompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4459 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 -Fmwin32_handmade.map
set LinkerFlags=/link -opt:ref -subsystem:windows,5.02 user32.lib gdi32.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl %CompilerFlags% ..\handmade\code\win32_handmade.cpp %LinkerFlags%
popd