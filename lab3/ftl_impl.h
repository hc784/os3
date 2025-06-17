/*
 * DKU Operating System Lab
 * Lab3 (Flash Translation Layer)
 * Student id : 32211332
 * Student name : 김화창
 * Date : 25-06-07
 */

#include "ftl.h"

#ifndef FTL_IMPL_H
#define FTL_IMPL_H

class GreedyFTL : public FlashTranslationLayer {
private:
    /* ────────── 내부 유틸리티 ────────── */
    int  countFreeBlocks() const;   // FREE 블록 개수
    int  findFreeBlock()   const;   // 첫 FREE 블록 인덱스(없으면 -1)
    void allocateNewActiveBlock();  // 새 활성 블록 확보
    void invalidateOldMapping(int logicalPage);             // 덮어쓰기 시 기존 PPN 무효화
    void internalWrite(int logicalPage, int data,
                       bool isHostWrite); // 공통 write 구현

public:
    /* ────────── 생성 / 소멸 ────────── */
    GreedyFTL(int total_blocks, int block_size);
    ~GreedyFTL() {}

    /* ────────── 필수 오버라이드 ────────── */
    void garbageCollect() override;
    void writePage     (int logicalPage, int data) override;
    void readPage      (int logicalPage)           override;
};

class CostBenefitFTL : public FlashTranslationLayer {
    private:
          // 1) 현재 시각(테스트용 time stamp)를 저장하는 멤버
    int current_time;

    // 2) GreedyFTL과 유사하게 블록 관리를 위한 헬퍼 함수들
    int  countFreeBlocks() const;         // FREE 블록의 개수 세기
    int  findFreeBlock()   const;         // erase(혹은 is_free==true)된 블록 하나 찾기
    void allocateNewActiveBlock();        // 새 활성 블록 할당
    void invalidateOldMapping(int logicalPage); 
    void internalWrite(int logicalPage, int data, bool isHostWrite);

public:
    // 3) 생성자 (ftl_impl.cpp 에서 재정의하지 않고, 헤더에서만 정의해 둠)
    CostBenefitFTL(int total_blocks, int block_size) 
        : FlashTranslationLayer(total_blocks, block_size), current_time(0) {
        name = "CostBenefitFTL";
    }
    ~CostBenefitFTL() {}

    // 4) 오버라이드할 가비지 컬렉션 및 Read/Write 인터페이스
    void garbageCollect() override;
    void writePage(int logicalPage, int data) override;
    void readPage(int logicalPage) override;
};

#endif // FTL_IMPL_H