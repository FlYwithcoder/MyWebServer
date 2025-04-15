cd ..
rm -rf build
mkdir build
cd build

# if run mem_pool_test
cmake .. -DMEM_POOL_TEST=1

#if not run mem_pool_test
# cmake ..
make
