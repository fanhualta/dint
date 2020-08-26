file(GLOB TEST_SOURCES test_*.cpp)
foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component (TEST_SRC_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_SRC_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_SRC_NAME}
    succinct
    ${Boost_LIBRARIES}
    )
  add_test(${TEST_SRC_NAME} ${TEST_SRC_NAME})
endforeach(TEST_SRC)

add_executable(tgcencoding tgc_encoding.cpp ../include/tgc/tgccoding.cpp)
target_link_libraries(tgcencoding
        succinct
        ${Boost_LIBRARIES}
        )
add_test(tgcencoding tgcencoding)

add_executable(multi_tgc_coding multi_tgc_encoding.cpp ../include/tgc/tgccoding.cpp)
target_link_libraries(multi_tgc_coding
        succinct
        ${Boost_LIBRARIES}
        )
add_test(multi_tgc_encoding multi_tgc_coding)

target_link_libraries(test_block_codecs
    FastPFor_lib)

target_link_libraries(test_block_posting_list
    FastPFor_lib)

target_link_libraries(test_block_freq_index
    FastPFor_lib)

