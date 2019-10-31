#if !defined(BFRAME_H)
#define BFRAME_H

#include "logger.h"
#include <cstdio>

const int PAGE_SIZE = 4 * 1024;
const int FRAME_SIZE = PAGE_SIZE;
const int NUM_PAGE_TOTAL = 50000;

struct bFrame {
    char field[FRAME_SIZE];
    int size = FRAME_SIZE;
};
struct BCB {
    int page_id;
    int frame_id;
    int count;
    bool dirty;
    int next_frame_id, prev_frame_id;

    BCB() { page_id = frame_id = next_frame_id = prev_frame_id - 1, count = 0, dirty = false; }
    void update(int pid, int fid, int cnt, bool dirty) {
        LOG_DEBUG("BCB.update");
        page_id = pid, frame_id = fid, count = cnt, this->dirty = dirty;
    }
};

#endif // BFRAME_H
