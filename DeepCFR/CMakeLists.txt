add_library(DeepCFR STATIC
        DeepCFR.cpp
        Net.hpp
)
if(UNIX AND NOT APPLE)
    set(CUDAToolkit_ROOT "/opt/cuda")
    set(CMAKE_CUDA_COMPILER "/opt/cuda/bin/nvcc")
    set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89;90")
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_HOST_COMPILER "/usr/bin/gcc-14")
    set(nvtx3_dir "/usr/lib/cmake/nvtx3/")
    find_package(CUDAToolkit REQUIRED)
    find_package(Torch REQUIRED PATHS "/opt/libtorch/share/cmake")
elseif(APPLE)
    find_package(Torch REQUIRED PATHS "/Library/Frameworks/Python.framework/Versions/3.12/lib/python3.12/site-packages")
endif()



target_link_libraries(DeepCFR PUBLIC Utility  ${TORCH_LIBRARIES})
target_include_directories(DeepCFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TORCH_INCLUDE_DIRS})