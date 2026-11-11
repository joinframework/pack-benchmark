function(add_pack_benches library header link)
    cmake_parse_arguments(ARG "" "" "PARSE;SERIALIZE;STATISTICS" ${ARGN})
    foreach(op PARSE SERIALIZE STATISTICS)
        if(ARG_${op})
            string(TOLOWER ${op} name)
            set(adapters "")
            foreach(adapter ${ARG_${op}})
                string(APPEND adapters "X(${adapter}) ")
            endforeach()
            set(target bench_${name}_${library})
            add_executable(${target} bench_${name}.cpp)
            target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
            target_compile_definitions(${target} PRIVATE
                DATA_DIR="${CMAKE_SOURCE_DIR}/data"
                ADAPTER_HEADER="${header}"
                "BENCH_ADAPTERS=${adapters}"
            )
            target_link_libraries(${target} PRIVATE benchmark::benchmark ${link})
            target_compile_options(${target} PRIVATE -Wall -Wextra)
        endif()
    endforeach()
endfunction()

add_pack_benches(join adapters/join.hpp join::pack
    PARSE      JoinAdapter
    SERIALIZE  JoinAdapter
    STATISTICS JoinAdapter
)
add_pack_benches(jsoncons adapters/jsoncons.hpp jsoncons::jsoncons
    PARSE      JsonConsAdapter
    SERIALIZE  JsonConsAdapter
    STATISTICS JsonConsAdapter
)
add_pack_benches(mpack adapters/mpack.hpp mpack::mpack
    PARSE      MpackZeroCopyAdapter
    SERIALIZE  MpackZeroCopyAdapter
    STATISTICS MpackZeroCopyAdapter
)
add_pack_benches(msgpack_cxx adapters/msgpack_cxx.hpp msgpack::cxx
    PARSE      MsgpackCxxAdapter MsgpackCxxRefAdapter
    SERIALIZE  MsgpackCxxAdapter MsgpackCxxRefAdapter
    STATISTICS MsgpackCxxAdapter MsgpackCxxRefAdapter
)
add_pack_benches(nlohmann adapters/nlohmann.hpp nlohmann_json::nlohmann_json
    PARSE      NlohmannAdapter
    SERIALIZE  NlohmannAdapter
    STATISTICS NlohmannAdapter
)
add_pack_benches(taocpp adapters/taocpp.hpp taocpp::json
    PARSE      TaoCppAdapter
    SERIALIZE  TaoCppAdapter
    STATISTICS TaoCppAdapter
)
