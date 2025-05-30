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
        // 멤버 변수 추가 선언 가능
    public:
        // 생성자
        CostBenefitFTL(int total_blocks, int block_size) : FlashTranslationLayer(total_blocks, block_size) {
            name = "CostBenefitFTL";
        }
        // 소멸자
        ~CostBenefitFTL() {}
        // 멤버 함수 추가 선언 가능
        void garbageCollect() override;
        void writePage(int logicalPage, int data) override;
        void readPage(int logicalPage) override;
};

#endif // FTL_IMPL_H