cmake_minimum_required(VERSION 2.8)
PROJECT(ulls)

#set(CMAKE_MODULE_PATH "$(CMAKE_SOURCE_DIR)/CMakeModule")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/src SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})

#set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})

#find_package(Threads REQUIRED)
#if(THREADS_HAVE_PTHREAD_ARG)
#  target_compile_options(${PROJECT_NAME} PUBLIC "-pthread")
#endif()
#if(CMAKE_THREAD_LIBS_INIT)
#  target_link_libraries(${PROJECT_NAME} "${CMAKE_THREAD_LIBS_INIT}")
#endif()

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb -Wall -fno-omit-frame-pointer")

#find_package(glib-2.0 REQUIRED)
#target_include_directories(${PROJECT_NAME} PRIVATE ${Glib_INCLUDE_DIRS})
#target_link_libraries(${PROJECT_NAME} ${Glib_LIBRARIES})

target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} fcgi)

find_package(PkgConfig)

pkg_check_modules (json-glib REQUIRED json-glib-1.0>=1.0)
target_include_directories(${PROJECT_NAME} PRIVATE ${json-glib_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} json-glib-1.0)

pkg_check_modules (LIBSOUP REQUIRED libsoup-2.4>=2.26)
target_include_directories(${PROJECT_NAME} PRIVATE ${libsoup_INCLUDE_DIRS})
#target_compile_options(${PROJECT_NAME} PRIVATE ${libsoup_CFLAGS})
target_link_libraries(${PROJECT_NAME} ${libsoup_LIBRARIES})


pkg_check_modules(GST REQUIRED gstreamer-webrtc-1.0>=1.14
                               gstreamer-sdp-1.0>=1.14)

target_include_directories(${PROJECT_NAME} PRIVATE ${GST_INCLUDE_DIRS})
#target_compile_options(${PROJECT_NAME} PRIVATE ${GST_CFLAGS})
target_link_libraries(${PROJECT_NAME} ${GST_LIBRARIES})


function(print_target_properties tgt)
    if(NOT TARGET ${tgt})
        message("There is no target named '${tgt}'")
        return()
    endif()

    # this list of properties can be extended as needed
    set(CMAKE_PROPERTY_LIST SOURCE_DIR BINARY_DIR COMPILE_DEFINITIONS
             COMPILE_OPTIONS INCLUDE_DIRECTORIES LINK_LIBRARIES)

    message("Configuration for target ${tgt}")

    foreach (prop ${CMAKE_PROPERTY_LIST})
        get_property(propval TARGET ${tgt} PROPERTY ${prop} SET)
        if (propval)
            get_target_property(propval ${tgt} ${prop})
            message (STATUS "${prop} = ${propval}")
        endif()
    endforeach(prop)
endfunction(print_target_properties)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#  message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

print_target_properties(${PROJECT_NAME})

