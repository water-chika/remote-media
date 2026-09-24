cmake_minimum_required(VERSION 3.15)
project(remote_media VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_library(remote_media src/frame.cpp)
add_library(remote_media::remote_media ALIAS remote_media)
target_compile_features(remote_media PUBLIC cxx_std_17)
target_include_directories(remote_media
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
set_target_properties(remote_media PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(remote_media_tests tests/test.cpp)
    target_link_libraries(remote_media_tests PRIVATE remote_media Threads::Threads)
    target_compile_features(remote_media_tests PRIVATE cxx_std_17)
    add_test(NAME remote_media_tests COMMAND remote_media_tests)
endif()

include(GNUInstallDirs)
install(TARGETS remote_media EXPORT remote_mediaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT remote_mediaTargets
    FILE remote_mediaTargets.cmake
    NAMESPACE remote_media::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/remote_media)
