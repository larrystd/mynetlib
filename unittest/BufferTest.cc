
#include <gtest/gtest.h>
#include <util/Buffer.h>
using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;
using namespace ananas;
// g++ -o BufferTest BufferTest.cc -I /home/larry/myproject/ananas/install/include -L /home/larry/myproject/ananas/install/lib -lananas_net -lananas_util -lgtest -lpthread

TEST(buffer, push) {
    Buffer buf;

    size_t ret = buf.PushData("hello", 5);
    EXPECT_EQ(ret, 5);

    ret = buf.PushData("world\n", 6);
    EXPECT_EQ(ret, 6);
}


TEST(buffer, peek) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    char tmp[12];
    size_t ret = buf.PeekDataAt(tmp, 5, 0);
    EXPECT_EQ(ret, 5);
    EXPECT_EQ(tmp[0], 'h');
    EXPECT_EQ(tmp[4], 'o');

    ret = buf.PeekDataAt(tmp, 2, 6);
    EXPECT_EQ(ret, 2);
    EXPECT_EQ(tmp[0], 'w');
    EXPECT_EQ(tmp[1], 'o');

    EXPECT_EQ(buf.ReadableSize(), 12);
}

TEST(buffer, pop) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    size_t cap = buf.Capacity();

    char tmp[12];
    size_t ret = buf.PopData(tmp, 6);
    EXPECT_EQ(ret, 6);
    EXPECT_EQ(tmp[0], 'h');
    EXPECT_EQ(tmp[5], ' ');

    EXPECT_EQ(buf.ReadableSize(), 6);

    ret = buf.PopData(tmp, 6);
    EXPECT_EQ(ret, 6);
    EXPECT_EQ(tmp[0], 'w');
    EXPECT_EQ(tmp[5], '\n');

    EXPECT_TRUE(buf.IsEmpty());
    EXPECT_EQ(buf.Capacity(), cap); // pop does not change capacity
}

TEST(buffer, shrink) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    EXPECT_NE(buf.Capacity(), 12);

    buf.Shrink();
    EXPECT_EQ(buf.Capacity(), 16);

    buf.PushData("abcd", 4);
    EXPECT_EQ(buf.Capacity(), 16);

    char tmp[16];
    buf.PopData(tmp, sizeof tmp);

    EXPECT_EQ(buf.Capacity(), 16);
}

TEST(buffer, push_pop) {
    Buffer buf;

    buf.PushData("hello ", 6);

    char tmp[8];
    size_t ret = buf.PopData(tmp, 5);

    EXPECT_EQ(ret, 5);
    EXPECT_EQ(buf.Capacity(), Buffer::kDefaultSize);

    buf.Shrink();
    EXPECT_EQ(buf.Capacity(), 1);
}


int main(int argc, char **argv) {
  InitGoogleTest(&argc, argv);

  bool check_for_leaks = false;
  if (argc > 1 && strcmp(argv[1], "--check_for_leaks") == 0 )
    check_for_leaks = true;
  else
    printf("%s\n", "Run this program with --check_for_leaks to enable "
           "custom leak checking in the tests.");

  return RUN_ALL_TESTS();
}

