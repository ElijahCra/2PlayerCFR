add_library(DeepCFR STATIC
        DeepCFR.cpp
        Net.cpp
)

set(CUDAToolkit_ROOT "/opt/cuda")
set(CMAKE_CUDA_COMPILER "/opt/cuda/bin/nvcc")
set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89;90")
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_HOST_COMPILER "/usr/bin/gcc-14")
set(nvtx3_dir "/usr/lib/cmake/nvtx3/")
find_package(CUDAToolkit REQUIRED)
find_package(Torch REQUIRED PATHS "/opt/libtorch/share/cmake")

target_link_libraries(DeepCFR PUBLIC Utility Preflop Texas ${TORCH_LIBRARIES})
target_include_directories(DeepCFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TORCH_INCLUDE_DIRS})