include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.4
    GIT_SHALLOW    TRUE
)
set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

FetchContent_Declare(
    join
    GIT_REPOSITORY https://github.com/joinframework/join.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(join)
add_library(join_pack STATIC
    ${join_SOURCE_DIR}/core/src/error.cpp
    ${join_SOURCE_DIR}/data/src/sax_error.cpp
    ${join_SOURCE_DIR}/data/src/json.cpp
)
add_library(join::pack ALIAS join_pack)
target_include_directories(join_pack PUBLIC
    ${join_SOURCE_DIR}/core/include
    ${join_SOURCE_DIR}/data/include
)
target_compile_features(join_pack PUBLIC cxx_std_20)

FetchContent_Declare(
    dconv
    GIT_REPOSITORY https://github.com/joinframework/dconv.git
    GIT_TAG        v1.2.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(dconv)
target_include_directories(join_pack PUBLIC ${dconv_SOURCE_DIR}/dconv/include)

FetchContent_Declare(
    msgpack_cxx
    GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
    GIT_TAG        cpp-7.0.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(msgpack_cxx)
add_library(msgpack_cxx_iface INTERFACE)
add_library(msgpack::cxx ALIAS msgpack_cxx_iface)
target_include_directories(msgpack_cxx_iface SYSTEM INTERFACE ${msgpack_cxx_SOURCE_DIR}/include)
target_compile_definitions(msgpack_cxx_iface INTERFACE MSGPACK_NO_BOOST)

FetchContent_Declare(
    mpack
    GIT_REPOSITORY https://github.com/ludocode/mpack.git
    GIT_TAG        v1.1.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(mpack)
file(GLOB MPACK_SOURCES ${mpack_SOURCE_DIR}/src/mpack/*.c)
add_library(mpack STATIC ${MPACK_SOURCES})
add_library(mpack::mpack ALIAS mpack)
target_include_directories(mpack SYSTEM PUBLIC ${mpack_SOURCE_DIR}/src)
target_compile_definitions(mpack PUBLIC MPACK_EXTENSIONS=1)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    taocpp_pegtl
    GIT_REPOSITORY https://github.com/taocpp/PEGTL.git
    GIT_TAG        3.2.8
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(taocpp_pegtl)
add_library(taocpp_pegtl_iface INTERFACE)
add_library(taocpp::pegtl ALIAS taocpp_pegtl_iface)
target_include_directories(taocpp_pegtl_iface SYSTEM INTERFACE ${taocpp_pegtl_SOURCE_DIR}/include)

FetchContent_Declare(
    taocpp_json
    GIT_REPOSITORY https://github.com/taocpp/json.git
    GIT_TAG        1.0.0-beta.14
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(taocpp_json)
add_library(taocpp_json_iface INTERFACE)
add_library(taocpp::json ALIAS taocpp_json_iface)
target_include_directories(taocpp_json_iface SYSTEM INTERFACE ${taocpp_json_SOURCE_DIR}/include)
target_link_libraries(taocpp_json_iface INTERFACE taocpp::pegtl)

FetchContent_Declare(
    jsoncons
    GIT_REPOSITORY https://github.com/danielaparker/jsoncons.git
    GIT_TAG        v0.176.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  none
)
FetchContent_MakeAvailable(jsoncons)
add_library(jsoncons_iface INTERFACE)
add_library(jsoncons::jsoncons ALIAS jsoncons_iface)
target_include_directories(jsoncons_iface SYSTEM INTERFACE ${jsoncons_SOURCE_DIR}/include)
