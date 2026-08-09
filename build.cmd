@echo off
echo building
cd ./src
g++ -std=c++20 -static-libgcc -static-libstdc++ main.cpp uwuifier.cpp -o uwuifier.exe
cd .. 