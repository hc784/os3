/*
*	DKU Operating System Lab
*	    Lab3 (Flash Translation Layer)
*	    Student id : 32211332 
*	    Student name : 김화창
*	    Date : 25-05-29
*/

#include "ftl_impl.h"
/* ────────────────────────────────────────────────────────── */
/*                 헬퍼 함수 구현                             */
/* ────────────────────────────────────────────────────────── */
int GreedyFTL::countFreeBlocks() const {
    int cnt = 0;
    for (const auto& b : blocks) if (b.is_free) ++cnt;
    return cnt;
}

int GreedyFTL::findFreeBlock() const {
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].is_free) return static_cast<int>(i);
    }
    return -1;
}

void GreedyFTL::allocateNewActiveBlock() {
    int idx = findFreeBlock();
    if (idx == -1) {
        /* 이론상 여기 오면 안 되지만 방어 코드 */
        throw std::runtime_error("No free block available!");
    }
    active_block  = idx;
    active_offset = 0;
    blocks[idx].is_free = false;
}

void GreedyFTL::invalidateOldMapping(int logicalPage) {
    int old_ppn = L2P[logicalPage];
    if (old_ppn == -1) return;           // 최초 쓰기
    int ob = old_ppn / block_size;
    int op = old_ppn % block_size;

    Page& pg          = blocks[ob].pages[op];
    pg.state          = INVALID;
    blocks[ob].valid_page_cnt--;
    blocks[ob].invalid_page_cnt++;
}

void GreedyFTL::internalWrite(int logicalPage, int data, bool isHostWrite) {


    Page& tgt = blocks[active_block].pages[active_offset];

    /* 실제 데이터 기록 */
    tgt.logical_page_num = logicalPage;
    tgt.data             = data;
    tgt.state            = VALID;

    /* 블록 메타정보 갱신 */
    blocks[active_block].valid_page_cnt++;
    blocks[active_block].is_free = false;

    /* L2P 테이블 갱신 */
    int new_ppn     = active_block * block_size + active_offset;
    L2P[logicalPage] = new_ppn;

    /* 오프셋 증가 */
    ++active_offset;

        /* 활성 블록이 꽉 찼으면 새 블록 확보 */
    if (active_offset == block_size) allocateNewActiveBlock();

    /* WAF 통계 */
    ++total_physical_writes;
    if (isHostWrite) ++total_logical_writes;
}


/* ────────────────────────────────────────────────────────── */
/*                 생성자                                    */
/* ────────────────────────────────────────────────────────── */
GreedyFTL::GreedyFTL(int total_blocks, int block_size)
    : FlashTranslationLayer(total_blocks, block_size) {
    name = "GreedyFTL";
}

/* ────────────────────────────────────────────────────────── */
/*                 Garbage Collection                        */
/* ────────────────────────────────────────────────────────── */
void GreedyFTL::garbageCollect() {
    /* 1. victim 선정 – INVALID 페이지가 가장 많은 블록 */
    int victim     = -1;
    int max_invalid = -1;
    for (size_t i = 0; i < blocks.size(); ++i) {
        /* FREE 블록·활성 블록 제외 */
        if (blocks[i].is_free || static_cast<int>(i) == active_block) continue;

        if (blocks[i].invalid_page_cnt > max_invalid) {
            max_invalid = blocks[i].invalid_page_cnt;
            victim      = static_cast<int>(i);
        }
    }

    /* invalid 페이지가 하나도 없으면 굳이 GC 안 함 */
    if (victim == -1 || max_invalid <= 0) return;

    /* 2. victim 블록의 VALID 페이지 백업 */
    struct ValPg { int lpn; int data; };
    std::vector<ValPg> valid_pages;

    for (const auto& pg : blocks[victim].pages) {
        if (pg.state == VALID)
            valid_pages.push_back({pg.logical_page_num, pg.data});
    }

    /* 3. 블록 Erase (실제 HW라면 지우기) */
    blocks[victim].gc_cnt++;
    blocks[victim].valid_page_cnt   = 0;
    blocks[victim].invalid_page_cnt = 0;
    blocks[victim].is_free          = true;
    for (auto& pg : blocks[victim].pages) pg = Page();  // 전체 FREE 초기화

    /* 4. VALID 페이지 재배치 */
    for (auto [lpn, data] : valid_pages) {
        internalWrite(lpn, data, /*isHostWrite=*/false);
    }
}

/* ────────────────────────────────────────────────────────── */
/*                 호스트 Write                              */
/* ────────────────────────────────────────────────────────── */
void GreedyFTL::writePage(int logicalPage, int data) {
    /* 필요시 GC – 새 블록을 할당해야 하는 시점에서 FREE 블록이 2개 이하 */
    if (countFreeBlocks() <= 2) garbageCollect();

    /* 덮어쓰기면 기존 매핑 무효화 */
    invalidateOldMapping(logicalPage);

    /* 실제 기록 */
    internalWrite(logicalPage, data, /*isHostWrite=*/true);
}

/* ────────────────────────────────────────────────────────── */
/*                 호스트 Read                               */
/* ────────────────────────────────────────────────────────── */
void GreedyFTL::readPage(int logicalPage) {
    int ppn = L2P[logicalPage];
    if (ppn == -1) {
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → 매핑 없음\n";
        return;
    }

    int blk = ppn / block_size;
    int off = ppn % block_size;
    const Page& pg = blocks[blk].pages[off];

    if (pg.state != VALID) {
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → INVALID/FREE\n";
        return;
    }

    std::cout << "[READ] LPN " << logicalPage
              << " → PPN " << ppn
              << " , DATA = " << pg.data << "\n";
}

void CostBenefitFTL::garbageCollect() {
   /*
   CostBenefitFTL's garbage collection policy
   - 가장 invalid한 페이지가 많은 블록들중, 쓰여진지 가장 오래된 블록을 선택한다. 
   - WAF 측정을 위해 total_logical_writes와 total_physical_writes를 업데이트한다.
   */
}

void CostBenefitFTL::writePage(int logicalPage, int data) {
   /*
   write operation in CostBenefitFT
   - 만약 남아있는 free block이 2개 이하라면, GC를 수행한다. 
   - 현재 활성 블록에 페이지를 쓰고, L2P 테이블을 업데이트한다.
   - 만약 활성 블록이 꽉 찼다면, 다음 새로운 블록을 활성 블록으로 설정한다.
   - WAF 측정을 위해 total_logical_writes와 total_physical_writes를 업데이트한다.
   */
}

void CostBenefitFTL::readPage(int logicalPage) {
   /*
   read operation in CostBenefitFT
   - L2P 테이블을 통해 논리 페이지 번호에 해당하는 물리 페이지 번호를 찾는다.
   - 해당 페이지의 데이터를 출력한다.
   - 만일 invalid or Free 페이지라면, 에러문구를 출력한다.
   */
}
