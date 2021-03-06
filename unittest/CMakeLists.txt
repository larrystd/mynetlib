INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

INCLUDE(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.10.0
)
FetchContent_MakeAvailable(googletest)

SET(TEST_TARGET ananas_unittests)
ADD_EXECUTABLE(${TEST_TARGET} "")
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/unittests)

TARGET_SOURCES(${TEST_TARGET}
  PRIVATE
  BufferTest.cc
  CallUnitTests.cc
  DelegateTest.cc
  HttpParserTest.cc
  ThreadPoolTest.cc
)

TARGET_LINK_LIBRARIES(${TEST_TARGET}
  PRIVATE
  ananas_util
  gtest_main
)
ENABLE_TESTING()
ADD_TEST(
  NAME google_test
  COMMAND $<TARGET_FILE:${TEST_TARGET}>)
