@echo off

[ -d bin ] || mkdir bin
pushd bin
cl /Zi /nologo ../*.c /Fe:tcpserver.exe /link kernel32.lib ws2_32.lib
popd