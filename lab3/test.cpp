#include "test_util.h"
#include "gtest/gtest.h"
#include "ftl.h"
#include "ftl_impl.h"

// 각 TEST_P는 FTLTest::SetUp() 이후 실행됨
// 각 TEST_P 실행 이후, FTLTest::TearDow()이 실행됨
TEST_P(FTLTest, Greedy) {
  ftl = new GreedyFTL(total_blocks, block_size);
}

TEST_P(FTLTest, Cost_benefit) {
  ftl = new CostBenefitFTL(total_blocks, block_size);
}


INSTANTIATE_TEST_CASE_P(Default, FTLTest,
  ::testing::Values(
    // std::make_tuple("워크로드 파일", "총 블록 수", "블록당 페이지 수")
    std::make_tuple("A", 10, 4),
    std::make_tuple("B", 10, 4),
    std::make_tuple("C", 10, 4)
  )
);

int main() {
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
