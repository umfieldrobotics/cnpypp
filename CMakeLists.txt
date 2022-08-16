CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(CNPYpp LANGUAGES CXX C)

include(GNUInstallDirs)

find_package(ZLIB REQUIRED)
find_package(Boost 1.78 COMPONENTS filesystem REQUIRED)

add_library(cnpy++ "cnpy++.cpp" "c_interface.c")
target_compile_features(cnpy++ PUBLIC cxx_std_17)
set_property(TARGET cnpy++ PROPERTY CXX_EXTENSIONS OFF)
target_include_directories(cnpy++ PUBLIC ${Boost_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
target_include_directories(cnpy++ SYSTEM PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_link_libraries(cnpy++ PRIVATE ${ZLIB_LIBRARIES} Boost::filesystem)
if(MSVC)
  target_compile_options(cnpy++ PRIVATE /W4 /WX)
else()
  target_compile_options(cnpy++ PRIVATE -Wall -Wextra -pedantic)
endif()


install(TARGETS "cnpy++" LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "cnpy++.hpp" "cnpy++.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(example1 example1.cpp)
target_link_libraries(example1 cnpy++)


add_executable(example2 example2.cpp)
target_link_libraries(example2 cnpy++)

add_executable(example_c example_c.c)
target_link_libraries(example_c cnpy++)

add_executable(range_example range_example.cpp)
target_link_libraries(range_example cnpy++)
target_compile_features(range_example PRIVATE cxx_std_20)
set_property(TARGET range_example PROPERTY CXX_EXTENSIONS OFF)

find_package(range-v3)
add_executable(range_zip_example range_zip_example.cpp)
target_link_libraries(range_zip_example cnpy++ range-v3::range-v3)
set_property(TARGET range_zip_example PROPERTY CXX_EXTENSIONS OFF)
