CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(CNPYpp LANGUAGES CXX)

include(GNUInstallDirs)

option(ENABLE_STATIC "Build static (.a) library" OFF)

find_package(ZLIB REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS})

add_library(cnpy++ SHARED "cnpy++.cpp")
target_compile_features(cnpy++ PUBLIC cxx_std_17)
target_include_directories(cnpy++ PUBLIC ${Boost_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
target_link_libraries(cnpy++ ${ZLIB_LIBRARIES} Boost::filesystem)

install(TARGETS "cnpy++" LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "cnpy++.hpp" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(ENABLE_STATIC)
    add_library(cnpy++-static STATIC "cnpy++.cpp")
    target_compile_features(cnpy++-static PUBLIC cxx_std_17)
    set_target_properties(cnpy++-static PROPERTIES OUTPUT_NAME "cnpy++")
    install(TARGETS "cnpy++-static" ARCHIVE DESTINATION lib)
endif(ENABLE_STATIC)

install(FILES "cnpy++.hpp" DESTINATION include)

add_executable(example1 example1.cpp)
target_link_libraries(example1 cnpy++)
