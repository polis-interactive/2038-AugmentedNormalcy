
if (AN_PLATFORM STREQUAL JETSON_CC)

    set(TARGET_ROOTFS /home/brucegoose/cross-compile/jetson-orin/rootfs)
    set(CROSS_COMPILE /home/brucegoose/cross-compile/jetson-orin/toolchain/bin/aarch64-buildroot-linux-gnu-)
    set(TEGRA_ARMABI aarch64-linux-gnu)
    set(CUDA_PATH usr/local/cuda)

    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)

    set(CMAKE_SYSROOT ${TARGET_ROOTFS})

    set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}-gcc)
    set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)

    set(
       CMAKE_CXX_FLAGS "\
            ${CMAKE_CXX_FLAGS} \
            -L${TARGET_ROOTFS}/${CUDA_PATH}/lib64 \
            -L${TARGET_ROOTFS}/usr/lib/${TEGRA_ARMABI} \
            -L${TARGET_ROOTFS}/usr/lib/${TEGRA_ARMABI}/tegra \
            -Wl,-rpath-link=${TARGET_ROOTFS}/lib/${TEGRA_ARMABI} \
            -Wl,-rpath-link=${TARGET_ROOTFS}/usr/lib/${TEGRA_ARMABI} \
            -Wl,-rpath-link=${TARGET_ROOTFS}/usr/lib/${TEGRA_ARMABI}/tegra \
            -Wl,-rpath-link=${TARGET_ROOTFS}/${CUDA_PATH}/lib64 \
            -I${TARGET_ROOTFS}/${CUDA_PATH}/include \
            -I${TARGET_ROOTFS}/usr/include/${TEGRA_ARMABI} \
    ")

    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

elseif (AN_PLATFORM STREQUAL JETSON)

    set(
        CMAKE_CXX_FLAGS "\
            ${CMAKE_CXX_FLAGS} \
	        -L/usr/local/cuda/lib64 \
	        -L/usr/lib/aarch64-linux-gnu \
	        -L/usr/lib/aarch64-linux-gnu \
    ")

endif()
