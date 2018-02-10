rd /q /s build
md build
cd lib\\win-process-injector
rd /q /s build
rd /q /s include
rd /q /s bin
md build include bin
cd build
cmake .. -G"Visual Studio 15 Win64"
cmake --build . --target INSTALL --config Release
cd ..\\..\\..
node-gyp configure & node-gyp build
pause