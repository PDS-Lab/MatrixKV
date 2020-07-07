// 
// File:        
// Description: 
// Authors:     
//

#pragma once
#include "bitmap.h"
#include "../../db/version_edit.h"
#include <cstdint>
#include <vector>
#include <string>
#include <libpmem.h>
namespace rocksdb {
    class NVMPageDescriptor {
        friend class NVMPager;
    public:
        // interface
        NVMPageDescriptor(uint32_t page_num, char* page_base);
        ~NVMPageDescriptor();

    private:
        char* page_addr_;                                               // 指向 NVM 上某一页 mmap 的内存地址空间
        uint32_t page_num_;                                             // 对应 NVM 上的页号 
        NVMPageDescriptor* next_free_;                                  // 维护空闲链
    };

    class NVMPager {
        static const uint32_t PAGE_SIZE_LOG = 18;
        static const uint64_t PAGE_SIZE = 1 << PAGE_SIZE_LOG;  // 页面大小
    public:
        // 构造器不对 free list、page_table_、first_index_ 进行初始化
        // page_bitmap_ 和 table_index_bitmap_ 也仅仅只是初始化大小不进行内容赋值
        // 真正的初始化将在 nvm_cf_mod.cc 的 RecoverFromStorageInfo 进行
        NVMPager(std::string &path, uint64_t each_size, uint64_t number);
        ~NVMPager();

        char* AllocSstable(int& index, FileMetaData *meta);

        char* GetIndexPtr(int index);

        void DeleteSstable(int index);

        void RecoverAddSstable(int index, std::vector<int> & file_pages);

		void RecoverNVMPager(const std::vector<FileMetaData*> * L0files = nullptr);

        void ReleasePages(int index, int offset);

        char* GetAddr(int index, int offset);

        uint64_t GetEachSize(){return each_size_;}

        uint64_t ReadData(int index, int offset, uint64_t size, char* buf);

        uint64_t AddData(int index, int offset, uint64_t size, char* buf);
    private:
        uint64_t table_num_;                                            // 管理的空间中最多可以存放的 table 数量
        uint64_t page_num_per_table_;                                   // 每个 table 包含多少页面
        char* nvm_base_;                                                // mmap 后 NVM 空间首地址
        NVMPageDescriptor* free_list_dump_;                             // free list 第一个空闲页面描述器
        uint64_t free_page_num_;                                        // 当前空闲页面的数量
        std::vector<std::vector<NVMPageDescriptor *> > page_table_;     // 变形的段页表
        std::vector<uint32_t> first_index_;                             // 随着 compaction 改变的每一个 Table 的 index
        BitMap* page_bitmap_;                                           // 记录 NVM 中 Page 的分配情况
        BitMap* table_index_bitmap_;                                    // Table 的索引值的分配情况
		uint64_t each_size_;
		bool init;
    };
    

}
