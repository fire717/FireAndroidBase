project(libpose)

cmake_minimum_required(VERSION 3.10)

set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.8.1-android/sdk/native/jni)
find_package(OpenCV REQUIRED core imgproc highgui)

set(ncnn_DIR ${CMAKE_SOURCE_DIR}/ncnn-20240102-android-vulkan/${ANDROID_ABI}/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

add_library(libaimodel SHARED pose_jni.cpp ndkcamera.cpp yolov7.cpp)

target_link_libraries(libaimodel ncnn ${OpenCV_LIBS} camera2ndk mediandk)
