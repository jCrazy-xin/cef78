set CEF_USE_GN=1
set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GN_DEFINES=use_jumbo_build=true proprietary_codecs=true ffmpeg_branding=Chrome
set GN_ARGUMENTS=--ide=vs2017 --sln=cef --filters=//cef/*
set SDK_ROOT="C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0"
call cef_create_projects.bat