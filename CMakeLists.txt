CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(CNPYpp LANGUAGES CXX C)

include(GNUInstallDirs)

find_package(ZLIB REQUIRED)
find_package(Boost 1.74 COMPONENTS filesystem REQUIRED)

include(FetchContent)

FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v3.1.0"
    GIT_SHALLOW True
)

FetchContent_MakeAvailable(GSL)

add_library(cnpy++ "src/cnpy++.cpp" "src/c_interface.c")
target_compile_features(cnpy++ PUBLIC cxx_std_17)
set_property(TARGET cnpy++ PROPERTY CXX_EXTENSIONS OFF)
target_include_directories(cnpy++ PUBLIC ${Boost_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
target_include_directories(cnpy++ SYSTEM PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_include_directories(cnpy++ SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
target_link_libraries(cnpy++ PRIVATE ${ZLIB_LIBRARIES} Boost::filesystem)
target_link_libraries(cnpy++ PUBLIC GSL)
if(MSVC)
  target_compile_options(cnpy++ PRIVATE /W4 /WX)
else()
  target_compile_options(cnpy++ PRIVATE -Wall -Wextra -pedantic -Wfatal-errors)
endif()


install(TARGETS "cnpy++" LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "tuple_util.hpp" "stide_iterator" "map_type.hpp" "cnpy++.hpp" "cnpy++.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(example1 "examples/example1.cpp")
target_link_libraries(example1 cnpy++)

add_executable(example_c "examples/example_c.c")
target_link_libraries(example_c cnpy++)

add_executable(range_example "examples/range_example.cpp")
target_link_libraries(range_example cnpy++)
target_compile_features(range_example PRIVATE cxx_std_20)
set_property(TARGET range_example PROPERTY CXX_EXTENSIONS OFF)

FetchContent_Declare(range-v3
    GIT_REPOSITORY "https://github.com/ericniebler/range-v3"
    GIT_TAG "0.12.0"
    GIT_SHALLOW True
)
FetchContent_MakeAvailable(range-v3)

add_executable(range_zip_example "examples/range_zip_example.cpp")
target_link_libraries(range_zip_example cnpy++ range-v3)
target_compile_features(range_zip_example PRIVATE cxx_std_17)
set_property(TARGET range_zip_example PROPERTY CXX_EXTENSIONS OFF)

add_executable(tuple_ex "examples/tuple_ex.cpp")
target_link_libraries(tuple_ex PRIVATE cnpy++)
target_compile_features(tuple_ex PRIVATE cxx_std_17)
set_property(TARGET tuple_ex PROPERTY CXX_EXTENSIONS OFF)
target_include_directories(tuple_ex PRIVATE ${Boost_INCLUDE_DIR})
