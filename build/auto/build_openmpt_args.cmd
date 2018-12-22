@echo off

set BATCH_DIR=%~dp0
cd %BATCH_DIR%
cd ..\..

set MY_DIR=%CD%

call "build\auto\setup_arguments.cmd" %1 %2 %3 %4

call "build\auto\setup_%MPT_VS_VER%.cmd"



cd "build\%MPT_VS_WITHTARGET%" || goto error

msbuild OpenMPT.sln /target:Build /property:Configuration=%MPT_VS_CONF%;Platform=%MPT_VS_ARCH%;XPDeprecationWarning=false /maxcpucount /verbosity:minimal || goto error
msbuild PluginBridge.sln /target:Build /property:Configuration=%MPT_VS_CONF%;Platform=%MPT_VS_ARCH_OTHER%;XPDeprecationWarning=false /maxcpucount /verbosity:minimal || goto error

cd ..\.. || goto error



goto noerror

:error
cd "%MY_DIR%"
exit 1

:noerror
cd "%MY_DIR%"
exit 0
