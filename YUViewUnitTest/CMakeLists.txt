find_package(GTest CONFIG QUIET)
if(NOT TARGET GTest::gtest)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/submodules/googletest/CMakeLists.txt")
    message(FATAL_ERROR "Unit tests need GoogleTest. Initialize submodules or install GoogleTest.")
  endif()
  add_subdirectory(${PROJECT_SOURCE_DIR}/submodules/googletest
    ${CMAKE_BINARY_DIR}/googletest)
endif()

file(GLOB_RECURSE YUVIEW_TEST_SOURCES CONFIGURE_DEPENDS *.cpp *.h)

add_executable(YUViewUnitTest ${YUVIEW_TEST_SOURCES})

set_target_properties(YUViewUnitTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewUnitTest"
  AUTOUIC_SEARCH_PATHS "${PROJECT_SOURCE_DIR}/YUViewLib/ui")

target_include_directories(YUViewUnitTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  common)
target_link_libraries(YUViewUnitTest PRIVATE YUViewLib GTest::gtest GTest::gmock)

add_test(NAME YUViewUnitTest COMMAND YUViewUnitTest)
