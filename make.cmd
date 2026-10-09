@echo off
rem Shim so `make <target>` works without GNU make on PATH.
rem Forwards to Qt's bundled mingw32-make, which reads the Makefile here.
"C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe" %*
