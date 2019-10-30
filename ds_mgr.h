#if !defined(DS_MGR_H)
#define DS_MGR_H

#include "bframe.h"
#include "buffer_mgr.h"
#include "logger.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using std::string;

const int CONTENT_ITEM_SIZE = 4;
const int CONTENT_MAX_SIZE = (PAGE_SIZE / CONTENT_ITEM_SIZE);
const int PAGE_NUM_IN_CONTENT = (CONTENT_MAX_SIZE - 1);

class DataStorageMgr {
private:
    FILE *currFile;
    int numPages;
    int contents[CONTENT_MAX_SIZE];
    unsigned int io_cnt_total;

    const string filename = "data.dbf";
    const string filepath = "./";
    void OpenFile(string filename) {
        LOG_DEBUG("DataStorageMgr.OpenFile");
        currFile = fopen((filepath + filename).c_str(), "wb+");
        if (currFile == nullptr)
            FAIL;
    }
    void CloseFile() {
        LOG_DEBUG("DataStorageMgr.CloseFile");
        if (fclose(currFile) == -1)
            FAIL;
    }
    void Seek(int offset) {
        LOG_DEBUG("DataStorageMgr.Seek");
        int ret = fseek(currFile, offset, SEEK_SET);
        if (ret != 0 || ferror(currFile))
            FAIL;
    }
    // 增加一次IO
    void IncIOCnt() {
        LOG_DEBUG("DataStorageMgr.IncIOCnt");
        io_cnt_total += 1;
    }
    // fread 的包装函数，会将 [io_cnt_total] 的值 加 1
    void FRead(void *dst, size_t sz, size_t cnt, FILE *fp) {
        LOG_DEBUG("DataStorageMgr.FRead");
        int ret = fread(dst, sz, cnt, fp);
        if (ferror(fp)) {
            FAILARG("file error");
            perror("error occured: ");
        }
        if (feof(fp))
            FAILARG("fread eof");
        if (ret < cnt)
            FAILARG("ret < cnt");
        IncIOCnt();
    }
    // fwrite 的包装函数，会将 [io_cnt_total] 的值 加 1
    void FWrite(void *str, size_t sz, size_t cnt, FILE *fp) {
        LOG_DEBUG("DataStorageMgr.FWrite");
        int ret = fwrite(str, sz, cnt, fp);
        if (ferror(fp)) {
            FAILARG("file error");
            perror("error occured: ");
        }
        if (feof(fp))
            FAILARG("fread eof");
        if (ret < cnt)
            FAILARG("ret < cnt");
        IncIOCnt();
        // if (io_cnt_total % 100 == 0)
        //     fflush(currFile);
    }
    // 读取目录，seek到offset并读取一个目录到 contents[] 中，第一个目录的 offset=0
    void ReadContent(int offset) {
        LOG_DEBUG("DataStorageMgr.ReadContent");
        Seek(offset);
        memset(contents, 0, sizeof(contents));
        FRead(contents, 1, sizeof(contents), currFile);
    }
    // 更新目录, seek到offset并将目录写入磁盘
    void WriteContent(int offset) {
        LOG_DEBUG("DataStorageMgr.WriteContent");
        Seek(offset);
        FWrite(contents, 1, sizeof(contents), currFile);
    }
    // 跳过[num_skip]个节点，最终[contents]中是第 [num_skip+1] 个目录，返回最终目录的offset
    int SkipContent(int num_skip) {
        LOG_DEBUG("DataStorageMgr.SkipContent");
        int cont_off = 0;
        while (num_skip >= 0) {
            ReadContent(cont_off);
            num_skip -= 1;
            if (num_skip < 0)
                return cont_off;
            cont_off = contents[CONTENT_MAX_SIZE - 1];
        }
        return cont_off;
    }
    // numpages += 1
    void IncNumpage() {
        LOG_DEBUG("DataStorageMgr.IncNumpage");
        numPages += 1;
    }

public:
    DataStorageMgr() {
        currFile = nullptr;
        numPages = 0;
        io_cnt_total = 0;
        memset(contents, 0, sizeof(contents));
        OpenFile(filename);
    }
    ~DataStorageMgr() {
        LOG_DEBUG("DataStorageMgr.~DataStorageMgr");
        CloseFile();
    }
    // 读取某个页
    int ReadPage(int page_id, bFrame &frame) {
        LOG_DEBUG("DataStorageMgr.ReadPage");
        int num_skip = page_id / PAGE_NUM_IN_CONTENT;
        SkipContent(num_skip);
        int target_offset = contents[page_id % PAGE_NUM_IN_CONTENT];
        Seek(target_offset);
        FRead(frame.field, 1, FRAME_SIZE, currFile);
        return 0;
    }
    // 更新某个页，页page_id一定存在，对于新加入的页，调用 [WriteNewPage]
    int WritePage(int page_id, bFrame &frame) {
        LOG_DEBUG("DataStorageMgr.WritePage");
        int num_skip = page_id / PAGE_NUM_IN_CONTENT;
        SkipContent(num_skip);
        int target_offset = contents[page_id % PAGE_NUM_IN_CONTENT];
        Seek(target_offset);
        FWrite(frame.field, 1, FRAME_SIZE, currFile);
        return 0;
    }
    // 写入一个新页
    int WriteNewPage(bFrame &frame) {
        LOG_DEBUG("DataStorageMgr.WriteNewPage");
        if (numPages == 0) {
            LOG_DEBUG("numpages == 0");
            memset(contents, 0, sizeof(contents));
            const int target_offset = sizeof(contents);
            contents[0] = target_offset;
            WriteContent(0);
            Seek(target_offset);
            FWrite(frame.field, 1, FRAME_SIZE, currFile);
            IncNumpage();
            return 0;
        }
        const int page_id = numPages;
        int num_skip = page_id / PAGE_NUM_IN_CONTENT;
        int target_index = page_id % PAGE_NUM_IN_CONTENT;
        // 需要新建目录
        if (target_index == 0) {
            num_skip -= 1;
            int cont_offset = SkipContent(num_skip);
            int new_content_offset = contents[PAGE_NUM_IN_CONTENT - 1] + PAGE_SIZE;
            // 更新最后一个目录的 next
            contents[CONTENT_MAX_SIZE - 1] = new_content_offset;
            WriteContent(cont_offset);
            // 建立新目录，更新内容并写到磁盘
            memset(contents, 0, sizeof(contents));
            int new_page_offset = new_content_offset + sizeof(contents);
            contents[0] = new_page_offset;
            WriteContent(new_content_offset);
            Seek(new_page_offset);
            FWrite(frame.field, 1, FRAME_SIZE, currFile);
        } else {
            int cont_offset = SkipContent(num_skip);
            int target_offset = contents[target_index - 1] + PAGE_SIZE;
            contents[target_index] = target_offset;
            WriteContent(cont_offset);
            Seek(target_offset);
            FWrite(frame.field, 1, FRAME_SIZE, currFile);
        }
        IncNumpage();
        return 0;
    }
    int GetNumPages() {
        LOG_DEBUG("DataStorageMgr.GetNumPages");
        return numPages;
    }
    int GetTotalIO() {
        LOG_DEBUG("DataStorageMgr.GetTotalIO");
        return io_cnt_total;
    }

    // ???
    void SetUse(int index, int use_bit) {}
    int GetUse(int index) { return 0; }
};

#endif // DS_MGR_H
