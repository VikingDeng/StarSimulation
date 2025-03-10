rm -rf build
mkdir build && cd build
cmake ..
make
cd build
chmod 777 main
./main