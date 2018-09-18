cmake_minimum_required(VERSION 2.8)
PROJECT(ulls)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(PrintDetails)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/src SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})

find_package(PkgConfig)

pkg_check_modules (json-glib REQUIRED json-glib-1.0>=1.0)
target_include_directories(${PROJECT_NAME} PRIVATE ${json-glib_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} json-glib-1.0)

pkg_check_modules (LIBSOUP REQUIRED libsoup-2.4>=2.26)
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBSOUP_INCLUDE_DIRS})
#target_compile_options(${PROJECT_NAME} PRIVATE ${libsoup_CFLAGS})
target_link_libraries(${PROJECT_NAME} ${LIBSOUP_LIBRARIES})

pkg_check_modules (GTK2 REQUIRED gtk+-3.0)
target_include_directories(${PROJECT_NAME} PRIVATE ${GTK2_INCLUDE_DIRS})
#link_directories (${GTK2_LIBRARY_DIRS}) 
target_link_libraries(${PROJECT_NAME} ${GTK2_LIBRARIES})

pkg_check_modules(GST REQUIRED gstreamer-webrtc-1.0>=1.14
                               gstreamer-sdp-1.0>=1.14
                               gstreamer-video-1.0>=1.14)

target_include_directories(${PROJECT_NAME} PRIVATE ${GST_INCLUDE_DIRS})
#target_compile_options(${PROJECT_NAME} PRIVATE ${GST_CFLAGS})
target_link_libraries(${PROJECT_NAME} ${GST_LIBRARIES})

#print_all_variables()
#print_target_properties(${PROJECT_NAME})

