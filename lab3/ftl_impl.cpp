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
    if (countFreeBlocks() <= 1) garbageCollect();

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

/* ────────── 헬퍼 ────────── */
int CostBenefitFTL::countFreeBlocks() const {
    int cnt = 0;
    for (const auto& b : blocks) if (b.is_free) ++cnt;
    return cnt;
}

int CostBenefitFTL::findFreeBlock() const {
    for (size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i].is_free) return static_cast<int>(i);
    return -1;
}

void CostBenefitFTL::allocateNewActiveBlock() {
    int idx = findFreeBlock();
    if (idx == -1) throw std::runtime_error("No free block available!");
    active_block        = idx;
    active_offset       = 0;
    blocks[idx].is_free = false;
}

void CostBenefitFTL::invalidateOldMapping(int logicalPage) {
    int old_ppn = L2P[logicalPage];
    if (old_ppn == -1) return;                 // 최초 쓰기
    int ob = old_ppn / block_size;
    int op = old_ppn % block_size;

    Page& pg          = blocks[ob].pages[op];
    pg.state          = INVALID;
    blocks[ob].valid_page_cnt--;
    blocks[ob].invalid_page_cnt++;
}

/* 공통 write 로직  */
void CostBenefitFTL::internalWrite(int logicalPage,
                                   int data,
                                   bool isHostWrite) {
    Page& tgt = blocks[active_block].pages[active_offset];

    tgt.logical_page_num = logicalPage;
    tgt.data             = data;
    tgt.state            = VALID;

    /* 블록 메타 갱신 */
    blocks[active_block].valid_page_cnt++;
    blocks[active_block].is_free         = false;
    blocks[active_block].last_write_time = current_time;   // ★ 타임스탬프
    ++current_time;                                        // 전역 시간 증가

    /* L2P 테이블 갱신 */
    int new_ppn   = active_block * block_size + active_offset;
    L2P[logicalPage] = new_ppn;

    /* 오프셋 이동 */
    ++active_offset;
    if (active_offset == block_size) allocateNewActiveBlock();

    /* WAF */
    ++total_physical_writes;
    if (isHostWrite) ++total_logical_writes;
}

/* ────────── 생성자 ────────── */
CostBenefitFTL::CostBenefitFTL(int total_blocks, int block_size)
    : FlashTranslationLayer(total_blocks, block_size),
      current_time(0) {
    name = "CostBenefitFTL";
}

/* ────────── Garbage Collection ────────── */
void CostBenefitFTL::garbageCollect() {
    /* ① 후보 필터링: 최근 8 쓰기 이내 블록·FREE·활성 블록 제외 */
    int victim      = -1;
    int max_invalid = -1;
    int best_age    = -1;

    for (size_t i = 0; i < blocks.size(); ++i) {
        auto& b = blocks[i];

        if (b.is_free || static_cast<int>(i) == active_block) continue;
        int age = current_time - b.last_write_time;
        if (age <= 8) continue;                       // 최근 8 이내면 skip
        if (b.invalid_page_cnt == 0) continue;        // 전부 VALID면 skip

        if (b.invalid_page_cnt > max_invalid ||
           (b.invalid_page_cnt == max_invalid && age > best_age)) {
            victim      = static_cast<int>(i);
            max_invalid = b.invalid_page_cnt;
            best_age    = age;
        }
    }

    if (victim == -1) return;   // 대상 없음

    /* ② VALID 페이지 백업 */
    struct ValPg { int lpn; int data; };
    std::vector<ValPg> valid_pages;
    for (const auto& pg : blocks[victim].pages)
        if (pg.state == VALID)
            valid_pages.push_back({pg.logical_page_num, pg.data});

    /* ③ Erase */
    blocks[victim].gc_cnt++;
    blocks[victim].valid_page_cnt   = 0;
    blocks[victim].invalid_page_cnt = 0;
    blocks[victim].is_free          = true;
    blocks[victim].last_write_time  = 0;
    for (auto& pg : blocks[victim].pages) pg = Page();

    /* ④ VALID 재배치 */
    for (auto [lpn, data] : valid_pages)
        internalWrite(lpn, data, /*isHostWrite=*/false);
}

/* ────────── Host Write ────────── */
void CostBenefitFTL::writePage(int logicalPage, int data) {
    /* FREE 블록이 2개 이하 → GC */
    if (countFreeBlocks() <= 2) garbageCollect();

    invalidateOldMapping(logicalPage);
    internalWrite(logicalPage, data, /*isHostWrite=*/true);
}

/* ────────── Host Read ────────── */
void CostBenefitFTL::readPage(int logicalPage) {
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
