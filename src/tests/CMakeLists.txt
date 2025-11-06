add_executable(astro-tests
  test_types.cpp
)
target_include_directories(astro-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(astro-tests PRIVATE astro_core)

if(GTest_FOUND)
  target_link_libraries(astro-tests PRIVATE GTest::gtest_main)
else()
  target_link_libraries(astro-tests PRIVATE gtest_main)
endif()

add_test(NAME astro-types COMMAND astro-tests)