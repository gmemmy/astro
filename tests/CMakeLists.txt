file(GLOB TEST_SOURCES "*.cpp")

add_executable(astro-tests ${TEST_SOURCES})
target_include_directories(astro-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
if(TARGET astro_core)
  target_link_libraries(astro-tests PRIVATE astro_core)
endif()

if(GTest_FOUND)
  target_link_libraries(astro-tests PRIVATE GTest::gtest_main)
else()
  target_link_libraries(astro-tests PRIVATE gtest_main)
endif()

add_test(NAME astro-tests COMMAND astro-tests)
