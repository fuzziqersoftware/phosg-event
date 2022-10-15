cmake_minimum_required(VERSION 3.10)



# Project setup

project(phosg-audio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")



# Library definitions

find_package(OpenSSL REQUIRED)

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
find_library  (LIBEVENT_THREAD      NAMES event_pthreads)
find_library  (LIBEVENT_SSL         NAMES event_openssl)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE}
        ${LIBEVENT_THREAD}
        ${LIBEVENT_SSL})

add_library(phosg-event
    src/BufferEvent.cc
    src/EvBuffer.cc
    src/EvDNSBase.cc
    src/Event.cc
    src/EventBase.cc
    src/EventConfig.cc
    src/EvHTTPRequest.cc
    src/HTTPServer.cc
    src/Listener.cc
    src/SSL.cc
)
target_include_directories(phosg-event PUBLIC ${LIBEVENT_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(phosg-event phosg pthread ${LIBEVENT_LIBRARIES} ${OPENSSL_LIBRARIES})



# Installation configuration

file(GLOB Headers ${CMAKE_SOURCE_DIR}/src/*.hh)
install(TARGETS phosg-event DESTINATION lib)
install(FILES ${Headers} DESTINATION include/phosg-event)
