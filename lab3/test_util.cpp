#include <queue>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <openssl/sha.h>
#include "gtest/gtest.h"
#include "test_util.h"
#include "ftl.h"
#include "ftl_impl.h"

// TEST_P 실행 전, 실행 됨
void FTLTest::SetUp() {
    load_workload();
}

// TEST_P 실행 후, 실행 됨
void FTLTest::TearDown() {
    run_workload(ftl);
    check_answer(ftl->get_name(), "mapping_table");
    check_answer(ftl->get_name(), "flash_status");
    print_flash_status();
    print_mapping_table();
    print_WAF_stats(); 
}

// 스케줄러 실행함수
void FTLTest::run_workload (FlashTranslationLayer* ftl_){
    for (const auto& item : workload) {
        if (item.operation == 'W') {
            ftl->writePage(item.lpn, item.data);
        } else if (item.operation == 'R') {
            ftl->readPage(item.lpn);            
        }
    }
}

void FTLTest::load_workload() {
    std::string filename = "data/workload_" + workload_name + ".csv";
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        FAIL() << "워크로드 파일을 열 수 없습니다: " << filename;
    }
    workload.clear();
    std::string line;
    // 헤더 라인 스킵
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        WorkloadItem item;

        // Operation
        std::getline(ss, token, ',');
        item.operation = token[0];

        // LPN
        std::getline(ss, token, ',');
        item.lpn = std::stoi(token);

        switch (item.operation) {
            case 'W':
                std::getline(ss, token, ',');
                item.data = std::stoi(token);
                break;
            case 'R':
                item.data = 0;
                break;
        }

        workload.push_back(item);
    }
}

// 통계정보 출력함수 Job 구조체에 저장된 정보를 통해 계산하고 출력
void FTLTest::print_flash_status() {
    // FTL 타입 출력
    std::cout << std::endl;
    if (ftl->get_name() == "GreedyFTL") {
        std::cout << "[ FTL Type: Greedy," << " Workload: " << workload_name << " ]" << std::endl << std::endl;
    } else if (ftl->get_name() == "CostBenefitFTL") {
        std::cout << "[ FTL Type: Cost-Benefit," << " Workload: " << workload_name << " ]" << std::endl << std::endl;
    }

    std::cout << "\n[ Flash Memory Status ]" << std::endl;

    int n_blocks = ftl->total_blocks;
    int block_size = ftl->block_size;
    const int cell_width = 9; // 칸 너비 (블록명, 데이터 모두 동일하게)

    // 블록 헤더 한 줄에 출력
    for (int b = 0; b < n_blocks; ++b) {
        std::cout << "+" << std::string(cell_width, '-');
    }
    std::cout << "+" << std::endl;

    // 블록 번호 한 줄에 출력 (가운데 정렬)
    for (int b = 0; b < n_blocks; ++b) {
        std::ostringstream oss;
        oss << "Block " << b;
        std::string name = oss.str();
        int pad = cell_width - name.size();
        int left = pad / 2, right = pad - left;
        std::cout << "|" << std::string(left, ' ') << name << std::string(right, ' ');
    }
    std::cout << "|" << std::endl;

    // 블록 내부 구분선
    for (int b = 0; b < n_blocks; ++b) {
        std::cout << "+" << std::string(cell_width, '-');
    }
    std::cout << "+" << std::endl;

    // 각 페이지별로 한 줄에 여러 블록의 같은 위치 페이지를 출력
    for (int p = 0; p < block_size; ++p) {
        for (int b = 0; b < n_blocks; ++b) {
            const Page& page = ftl->blocks[b].pages[p];
            std::ostringstream oss;
            if (page.state == VALID) {
                oss << page.logical_page_num << " V";
            } else if (page.state == INVALID) {
                oss << page.logical_page_num << " I";
            } else {
                oss << "";
            }
            std::string val = oss.str();
            int pad = cell_width - val.size();
            int left = pad / 2, right = pad - left;
            std::cout << "|" << std::string(left, ' ') << val << std::string(right, ' ');
        }
        std::cout << "|" << std::endl;
    }

    // 블록 내부 구분선
    for (int b = 0; b < n_blocks; ++b) {
        std::cout << "+" << std::string(cell_width, '-');
    }
    std::cout << "+" << std::endl;

    // GC 횟수 출력
    for (int b = 0; b < n_blocks; ++b) {
        std::ostringstream oss;
        oss << "GC:" << ftl->blocks[b].gc_cnt;
        std::string val = oss.str();
        int pad = cell_width - val.size();
        int left = pad / 2, right = pad - left;
        std::cout << "|" << std::string(left, ' ') << val << std::string(right, ' ');
    }
    std::cout << "|" << std::endl;

    // 마지막 구분선
    for (int b = 0; b < n_blocks; ++b) {
        std::cout << "+" << std::string(cell_width, '-');
    }
    std::cout << "+\n" << std::endl;
}

void FTLTest::print_WAF_stats() {
    double WAF = 0.0;
    if (ftl->total_logical_writes != 0) 
        WAF = ftl->total_physical_writes / ftl->total_logical_writes;

    std::cout << "\n[ WAF Statistics ]" << std::endl;
    std::cout << "Total Logical Writes: " << ftl->total_logical_writes << std::endl;
    std::cout << "Total Physical Writes: " << ftl->total_physical_writes << std::endl;
    std::cout << "Write Amplification Factor (WAF): " << std::fixed << std::setprecision(3) << WAF << std::endl;
}

// 통계정보 출력함수 Job 구조체에 저장된 정보를 통해 계산하고 출력
void FTLTest::print_mapping_table() {
    std::cout << "\n[ LBA to PPN Mapping Table ]" << std::endl;
    // 워크로드에서 최대 LPN 찾기
    int max_lpn = 0;
    for (const auto& item : workload) {
        max_lpn = std::max(max_lpn, item.lpn);
    }
    
    const int cell_width = 4;
    // 윗 테두리
    std::cout << "+";
    for (int i = 0; i <= max_lpn+1; ++i) std::cout << std::string(cell_width, '-') << "+";
    std::cout << "\n|";
    // LBA 라벨
    std::cout << std::setw(cell_width) << "LBA:" << "|";
    for (int i = 0; i <= max_lpn; ++i)
        std::cout << std::setw(cell_width) << i << "|";
    std::cout << "\n+";
    for (int i = 0; i <= max_lpn+1; ++i) std::cout << std::string(cell_width, '-') << "+";
    std::cout << "\n|";
    // PPN 라벨
    std::cout << std::setw(cell_width) << "PPN:" << "|";
    for (int i = 0; i <= max_lpn; ++i) {
        if (ftl->L2P[i] == -1)
            std::cout << std::setw(cell_width) << "-" << "|";
        else
            std::cout << std::setw(cell_width) << ftl->L2P[i] << "|";
    }
    // 아랫 테두리
    std::cout << "\n+";
    for (int i = 0; i <= max_lpn+1; ++i) std::cout << std::string(cell_width, '-') << "+";
    std::cout << std::endl;
}


void FTLTest::check_answer(const std::string& ftl_name, std::string suffix) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    if (ftl_name != "GreedyFTL") {
        return; // GreedyFTL만 정답 체크
    }
    if (suffix == "mapping_table"){
        int* data = ftl->L2P.data();
        size_t data_size = ftl->L2P.size();
        SHA256_Update(&sha256, data, data_size*sizeof(int));
    } else if (suffix == "flash_status"){
        for (const auto& block : ftl->blocks) {
            // 블록의 각 페이지를 순차적으로 처리
            for (const auto& page : block.pages) {
                SHA256_Update(&sha256, &page.state, sizeof(PageState));
                SHA256_Update(&sha256, &page.logical_page_num, sizeof(int));
                SHA256_Update(&sha256, &page.data, sizeof(int));
            }
            // GC 카운트 추가
            SHA256_Update(&sha256, &block.gc_cnt, sizeof(int));
        }
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    std::string filename = "./data/answer/" + ftl_name + "_" + suffix + "_" + workload_name;

    unsigned char hashFromFile[SHA256_DIGEST_LENGTH];

    std::ifstream file(filename, std::ios::binary);
    file.read(reinterpret_cast<char*>(hashFromFile), SHA256_DIGEST_LENGTH);
    file.close();

    ASSERT_TRUE(memcmp(hash, hashFromFile, SHA256_DIGEST_LENGTH) == 0);
}
