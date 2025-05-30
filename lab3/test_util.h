#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <queue>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include "ftl.h"
#include "gtest/gtest.h"

// 워크로드 항목을 나타내는 구조체
struct WorkloadItem {
    char operation;     // 'R' 또는 'W'
    int lpn;            // Logical Page Number
    int data;           // 데이터 (읽기의 경우 비어있음)
};

class FTLTest : public ::testing::TestWithParam<std::tuple<std::string, int, int>> {
protected:
    FlashTranslationLayer* ftl;
    std::vector<WorkloadItem> workload;
    std::string workload_name = std::get<0>(GetParam());
    int total_blocks = std::get<1>(GetParam());
    int block_size = std::get<2>(GetParam());

    void SetUp() override;

    void TearDown() override;

    // CSV 파일에서 워크로드 로드
    void load_workload();

    // 워크로드 실행
    void run_workload(FlashTranslationLayer* ftl);

    // 플래시 상태 출력
    void print_flash_status();

    // 블록 상태 출력
    void print_mapping_table();

    // WAF 통계 출력
    void print_WAF_stats();
    
    // 정답 확인 함수
    void check_answer(const std::string& ftl_name, std::string suffix);

};

#endif // TEST_UTIL_H