#ifndef FTL_H
#define FTL_H

#include <iostream>
#include <array>
#include <vector>
#include <cstring>
#include <iomanip>

// Page 상태를 나타내는 enum 추가
enum PageState {
    FREE,    // 완전히 빈 상태
    VALID,   // 유효한 데이터가 있는 상태
    INVALID  // 무효화된 데이터가 있는 상태
};

// Page 구조체 수정
struct Page {
    int logical_page_num;   // 논리적 페이지 번호
    PageState state;        // 페이지 상태
    int data;              // 페이지 데이터
    
    Page() : logical_page_num(-1), state(FREE), data(-1) {}
};

// Block 구조체 수정
struct Block {
    std::vector<Page> pages;     // 페이지 배열
    int valid_page_cnt;         // 유효한 페이지 수
    int invalid_page_cnt;       // 무효한 페이지 수
    bool is_free;              // 블록이 완전히 비어있는지 여부
    int gc_cnt;                // 가비지 컬렉션 횟수
    int last_write_time;       // 블록 나이
    
    Block() : valid_page_cnt(0), invalid_page_cnt(0), is_free(true), gc_cnt(0), last_write_time(0) {}
};

class FlashTranslationLayer {
    private:
    public:
        std::string name;
        // 블록 배열
        std::vector<Block> blocks;
        int total_blocks;
        int block_size;

        // Page mapping table
        std::vector<int> L2P; // Logical Page Number -> Physical Page Number
        
        int active_block;      // 현재 쓰기 중인 활성 블록
        int active_offset;     // 활성 블록의 다음 쓰기 위치
        
        // WAF 측정을 위한 변수들
        double total_logical_writes;    // 호스트가 요청한 논리적 쓰기 수
        double total_physical_writes;   // 실제 플래시에 쓰여진 페이지 수
        
        // 생성자
        FlashTranslationLayer(int total_blocks, int block_size) 
            : total_blocks(total_blocks), block_size(block_size), L2P(total_blocks * block_size, -1),
            total_logical_writes(0), total_physical_writes(0) {
            name = "Default";

            // L2P 테이블 초기화
            for (int i = 0; i < total_blocks * block_size; i++) {
                L2P[i] = -1;
            }
            
            // 블록 초기화
            blocks.resize(total_blocks);
            for (int i = 0; i < total_blocks; i++) {
                blocks[i].pages.resize(block_size);
            }

            active_block = 0;
            active_offset = 0;        
            blocks[active_block].is_free = false;  // 활성 블록은 더 이상 free가 아님
        }

        // 가비지 컬렉션 수행
        virtual void garbageCollect() = 0;

        // Page Write 연산
        virtual void writePage(int logicalPage, int data) = 0;

        // Page Read 연산
        virtual void readPage(int logicalPage) = 0;

        // 이름 반환 함수
        virtual std::string get_name() final{
            return name;
        };

};

#endif // FTL_H