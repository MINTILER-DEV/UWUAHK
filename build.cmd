@echo off
echo building
cd ./src
g++ -std=c++20 main.cpp uwuifier.cpp -o ../build/uwuifier.exe
cd .. 