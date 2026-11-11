set(PACK_BENCH_LIBS
    join::pack
    msgpack::cxx
    mpack::mpack
    nlohmann_json::nlohmann_json
    taocpp::json
    jsoncons::jsoncons
)

add_subdirectory(conformance)
add_subdirectory(performance)
