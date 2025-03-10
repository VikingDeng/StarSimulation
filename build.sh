rm -rf build
mkdir build && cd build
cmake ..
make
cd ..
chmod 777 ./build/main
./build/main 2> output.log