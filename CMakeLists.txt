CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(CNPYpp LANGUAGES CXX C)

include(GNUInstallDirs)

find_package(ZLIB REQUIRED)
find_package(Boost 1.72 COMPONENTS filesystem REQUIRED)

add_library(cnpy++ "cnpy++.cpp" "c_interface.c")
target_compile_features(cnpy++ PUBLIC cxx_std_17)
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

add_executable(example_c example_c.c)
target_link_libraries(example_c cnpy++)
