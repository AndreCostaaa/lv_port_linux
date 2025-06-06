set -e

PI_URL=$1

docker run -it -v $(pwd):/workspace cross_compilation_image bash -c "\
		cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=./cmake_toolchain_files/aarch64-linux-gnu.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -B build-pi -S .;\
		cmake --build build-pi -j12"

scp -r build-pi/bin $PI_URL:~/neon64-support

# For rendering test purposes
ssh $PU_URL "cd neon64-support && ./bin/render"

# For benchmarking purposes
# echo 'Benchmark Release SW'
# ssh $PI_URL "time ./neon64-support/bin/benchmark_sw"
# echo 'Benchmark Release Neon'
# ssh $PI_URL "time ./neon64-support/bin/benchmark_neon"
