/*
 * DKU Operating System Lab
 * Lab3 (Flash Translation Layer)
 * Student id : 32211332
 * Student name : 김화창
 * Date : 25-06-07
 */

#include "ftl_impl.h" // FTL 구현 헤더 포함

// ───── GreedyFTL 헬퍼 함수 ─────
int GreedyFTL::countFreeBlocks() const { // free 블록 수를 계산
    int cnt = 0;                        // 카운터 초기화
    for (const auto& b : blocks)        // 모든 블록 순회
        if (b.is_free) ++cnt;           // free 블록이면 ++
    return cnt;                         // 결과 반환
}

int GreedyFTL::findFreeBlock() const {  // 첫 free 블록 인덱스 탐색
    for (size_t i = 0; i < blocks.size(); ++i) // 인덱스 순회
        if (blocks[i].is_free)          // free 블록 발견 시
            return static_cast<int>(i); // 해당 인덱스 반환
    return -1;                          // 없으면 -1
}

void GreedyFTL::allocateNewActiveBlock() { // 새 active 블록 할당
    int idx = findFreeBlock();              // free 블록 검색
    if (idx == -1) {                       // 없으면 예외 발생
        throw std::runtime_error("No free block available!");
    }
    active_block  = idx;                   // 활성 블록 설정
    active_offset = 0;                     // 오프셋 초기화
    blocks[idx].is_free = false;           // 상태 갱신
}

void GreedyFTL::invalidateOldMapping(int logicalPage) { // 기존 매핑 무효화
    int old_ppn = L2P[logicalPage];                     // 기존 PPN 조회
    if (old_ppn == -1) return;                          // 최초 쓰기면 종료
    int ob = old_ppn / block_size;                      // 블록 번호 계산
    int op = old_ppn % block_size;                      // 페이지 오프셋 계산

    Page& pg          = blocks[ob].pages[op];           // 페이지 참조
    pg.state          = INVALID;                        // 상태를 INVALID로
    blocks[ob].valid_page_cnt--;                        // valid 개수 감소
    blocks[ob].invalid_page_cnt++;                      // invalid 개수 증가
}

void GreedyFTL::internalWrite(int logicalPage, int data, bool isHostWrite) {

    if (active_offset == block_size){
        allocateNewActiveBlock();          // 새 블록 할당
        if (countFreeBlocks() <= 1) garbageCollect(); // free 블록 부족 시 GC
    }       // 블록이 가득 차면

    Page& tgt = blocks[active_block].pages[active_offset]; // 대상 페이지

    tgt.logical_page_num = logicalPage;   // LPN 기록
    tgt.data             = data;          // 데이터 기록
    tgt.state            = VALID;         // 상태 VALID

    blocks[active_block].valid_page_cnt++; // 메타데이터 갱신
    blocks[active_block].is_free = false;  // free 아님 표시

    int new_ppn     = active_block * block_size + active_offset; // PPN 계산
    L2P[logicalPage] = new_ppn;            // L2P 테이블 갱신

    ++active_offset;                       // 오프셋 증가
    
        
    ++total_physical_writes;               // 물리적 쓰기 카운트
    if (isHostWrite) ++total_logical_writes; // 호스트 쓰기면 논리적 쓰기도 증가
}

// ───── GreedyFTL 생성자 ─────
GreedyFTL::GreedyFTL(int total_blocks, int block_size)
    : FlashTranslationLayer(total_blocks, block_size) { // 기본 FTL 초기화
    name = "GreedyFTL";                                // 이름 설정
}

// ───── GreedyFTL Garbage Collection ─────
void GreedyFTL::garbageCollect() {
    int victim     = -1;               // victim 블록 인덱스
    int max_invalid = -1;              // 최대 invalid 페이지 수

    for (size_t i = 0; i < blocks.size(); ++i) { // 모든 블록 탐색
        if (blocks[i].is_free || static_cast<int>(i) == active_block) continue; // 제외 조건
        if (blocks[i].invalid_page_cnt > max_invalid) { // 더 많은 invalid 발견
            max_invalid = blocks[i].invalid_page_cnt;   // 최대값 갱신
            victim      = static_cast<int>(i);          // victim 갱신
        }
    }

    if (victim == -1 || max_invalid <= 0) return; // GC 필요 없음

    struct ValPg { int lpn; int data; };          // 유효 페이지 임시 구조체
    std::vector<ValPg> valid_pages;               // 백업용 벡터

    for (const auto& pg : blocks[victim].pages)   // victim 블록 페이지 순회
        if (pg.state == VALID)                    // VALID 만 수집
            valid_pages.push_back({pg.logical_page_num, pg.data});

    blocks[victim].gc_cnt++;                     // GC 카운트 증가
    blocks[victim].valid_page_cnt   = 0;         // 메타 초기화
    blocks[victim].invalid_page_cnt = 0;
    blocks[victim].is_free          = true;
    for (auto& pg : blocks[victim].pages) pg = Page(); // 페이지 초기화

    for (auto [lpn, data] : valid_pages)        // VALID 페이지 재기록
        internalWrite(lpn, data, false);        // 호스트 쓰기 아님
}

// ───── 호스트 Write ─────
void GreedyFTL::writePage(int logicalPage, int data) {
    
    invalidateOldMapping(logicalPage);            // 기존 매핑 무효화
    internalWrite(logicalPage, data, true);       // 실제 쓰기
}

// ───── 호스트 Read ─────
void GreedyFTL::readPage(int logicalPage) {
    int ppn = L2P[logicalPage];                  // PPN 조회
    if (ppn == -1) {                             // 매핑 없으면 오류
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → 매핑 없음\n";
        return;
    }

    int blk = ppn / block_size;                  // 블록 계산
    int off = ppn % block_size;                  // 오프셋 계산
    const Page& pg = blocks[blk].pages[off];     // 페이지 참조

    if (pg.state != VALID) {                     // 유효성 체크
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → INVALID/FREE\n";
        return;
    }

    std::cout << "[READ] LPN " << logicalPage   // 성공 메시지 출력
              << " → PPN " << ppn
              << " , DATA = " << pg.data << "\n";
}

// ───── CostBenefitFTL 헬퍼 함수 ─────
int CostBenefitFTL::countFreeBlocks() const { // free 블록 수 계산
    int cnt = 0;
    for (const auto& b : blocks) if (b.is_free) ++cnt;
    return cnt;
}

int CostBenefitFTL::findFreeBlock() const {   // free 블록 탐색
    for (size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i].is_free) return static_cast<int>(i);
    return -1;
}

void CostBenefitFTL::allocateNewActiveBlock() { // 새 active 블록 할당
    int idx = findFreeBlock();
    if (idx == -1) throw std::runtime_error("No free block available!");
    active_block        = idx;
    active_offset       = 0;
    blocks[idx].is_free = false;
}

void CostBenefitFTL::invalidateOldMapping(int logicalPage) { // 기존 매핑 무효화
    int old_ppn = L2P[logicalPage];
    if (old_ppn == -1) return;                 // 최초 쓰기
    int ob = old_ppn / block_size;
    int op = old_ppn % block_size;

    Page& pg          = blocks[ob].pages[op];
    pg.state          = INVALID;
    blocks[ob].valid_page_cnt--;
    blocks[ob].invalid_page_cnt++;
}

void CostBenefitFTL::internalWrite(int logicalPage, int data, bool isHostWrite) {
    Page& tgt = blocks[active_block].pages[active_offset]; // 대상 페이지

    tgt.logical_page_num = logicalPage;   // LPN 설정
    tgt.data             = data;          // 데이터 기록
    tgt.state            = VALID;         // 상태 VALID

    blocks[active_block].valid_page_cnt++;     // valid++
    blocks[active_block].is_free         = false; // free 아님
    blocks[active_block].last_write_time = current_time; // 타임스탬프

    if (isHostWrite) ++current_time;     // 호스트 쓰기면 시간 경과

    int new_ppn = active_block * block_size + active_offset; // PPN 계산
    L2P[logicalPage] = new_ppn;            // L2P 기록

    ++active_offset;                       // 오프셋 증가
    if (active_offset == block_size)       // 블록 full 시
        allocateNewActiveBlock();          // 새 블록 할당

    ++total_physical_writes;               // 물리적 쓰기++
    if (isHostWrite) ++total_logical_writes; // 논리적 쓰기++
}

void CostBenefitFTL::garbageCollect() {   // Cost-Benefit GC
    int victim      = -1;                 // victim 인덱스
    int max_invalid = -1;                 // 최대 invalid
    int best_age    = -1;                 // 최대 age

    for (size_t i = 0; i < blocks.size(); ++i) {
        auto& b = blocks[i];
        if (b.is_free || static_cast<int>(i) == active_block) continue; // 제외
        int age = current_time - b.last_write_time;     // 블록 age
        if (age <= 8) continue;                         // 최근 8회 이내 제외
        if (b.invalid_page_cnt == 0) continue;          // 전부 valid 제외

        if (b.invalid_page_cnt > max_invalid ||         // invalid 우선
            (b.invalid_page_cnt == max_invalid && age > best_age)) { // tie → age
            victim      = static_cast<int>(i);
            max_invalid = b.invalid_page_cnt;
            best_age    = age;
        }
    }

    if (victim == -1) return;              // 대상 없으면 종료

    struct ValPg { int lpn; int data; };   // 유효 페이지 구조체
    std::vector<ValPg> valid_pages;        // 백업 벡터

    for (const auto& pg : blocks[victim].pages)        // victim 페이지 순회
        if (pg.state == VALID)
            valid_pages.push_back({pg.logical_page_num, pg.data});

    blocks[victim].gc_cnt++;              // GC 횟수++
    blocks[victim].valid_page_cnt   = 0;  // 메타 리셋
    blocks[victim].invalid_page_cnt = 0;
    blocks[victim].is_free          = true;
    blocks[victim].last_write_time  = 0;
    for (auto& pg : blocks[victim].pages) pg = Page();  // 페이지 초기화

    for (auto [lpn, data] : valid_pages)  // VALID 재배치
        internalWrite(lpn, data, false);  // 호스트 쓰기 아님
}

void CostBenefitFTL::writePage(int logicalPage, int data) { // Host Write
    if (countFreeBlocks() <= 1) garbageCollect(); // free 부족 시 GC
    invalidateOldMapping(logicalPage);            // 기존 매핑 무효화
    internalWrite(logicalPage, data, true);       // 실제 쓰기
}

void CostBenefitFTL::readPage(int logicalPage) {   // Host Read
    int ppn = L2P[logicalPage];                    // PPN 조회
    if (ppn == -1) {                               // 매핑 없음
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → 매핑 없음\n";
        return;
    }

    int blk = ppn / block_size;                    // 블록 번호
    int off = ppn % block_size;                    // 오프셋
    const Page& pg = blocks[blk].pages[off];       // 페이지

    if (pg.state != VALID) {                       // 유효하지 않으면
        std::cout << "[READ  ERROR] LPN " << logicalPage << " → INVALID/FREE\n";
        return;
    }

    std::cout << "[READ] LPN " << logicalPage      // 성공 출력
              << " → PPN " << ppn
              << " , DATA = " << pg.data << "\n";
}
