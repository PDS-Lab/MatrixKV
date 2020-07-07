// 
// File:        
// Description: 
// Authors:     
//
#include "my_log.h"
#include "nvm_pager.h"
#include <cassert>
// #include <libpmem.h>

namespace rocksdb {

    NVMPager::NVMPager(std::string &path, uint64_t each_size, uint64_t number)
    {
        char* pmemaddr=nullptr;
        size_t mapped_len;
        int is_pmem;
        // each_size 向上取整 PAGE_SIZE 的整数倍
        if (each_size & (NVMPager::PAGE_SIZE - 1))
            each_size = ((each_size >> NVMPager::PAGE_SIZE_LOG) + 1) << NVMPager::PAGE_SIZE_LOG;

        uint64_t all_size = each_size * number;
        each_size_ = each_size;

        // 预留第一个空白页面给 Page 元数据信息持久化到 NVM
        // 按照一个页面 256KB 算，一个页可以管理 64K 个页面
        // 8GB 的 L0 大小，大概有 32K 个页面
        // 这样剩下一半可以存放 bitmap 元数据
        pmemaddr = (char *)(pmem_map_file(path.c_str(), all_size + NVMPager::PAGE_SIZE,
                             PMEM_FILE_CREATE, 0666,
                             &mapped_len, &is_pmem));

        table_num_ = number;
        page_num_per_table_ = each_size >> NVMPager::PAGE_SIZE_LOG;
        nvm_base_ = pmemaddr;
		free_list_dump_ = nullptr;
        free_page_num_ = 0;

        page_table_.resize(table_num_);
        first_index_.resize(table_num_);

        page_bitmap_ = new BitMap((all_size >> NVMPager::PAGE_SIZE_LOG));
        table_index_bitmap_ = new BitMap(table_num_); 
		free_page_num_ = (all_size >> NVMPager::PAGE_SIZE_LOG);
		init = false;
    }

    NVMPager::~NVMPager()
    {
        // 释放留存在 free list 上的 pd
        NVMPageDescriptor* tmp = nullptr;
        NVMPageDescriptor* cur_ptr = free_list_dump_;
        while (cur_ptr) {
            tmp = cur_ptr->next_free_;
            delete cur_ptr;
            cur_ptr = tmp;
        }

        // 释放掉被占用的 NVM page 的 pd
        for (uint i = 0; i < page_table_.size(); i++) {
            for (uint j = 0; j < page_table_[i].size(); j++) {
                if (page_table_[i][j])
                    delete page_table_[i][j];
            }
        }
    }

    char* NVMPager::AllocSstable(int& index, FileMetaData *meta)
    {
		if(!init)
			RecoverNVMPager();
        char* table_addr = nullptr;
        if (free_page_num_ < page_num_per_table_)
            return table_addr;
        for (uint i = 0; i < table_num_; i++) {
            if (table_index_bitmap_->get(i) == 0) {
                index = i;
                table_index_bitmap_->set(i);
		for (uint j = 0; j < page_num_per_table_; j++) {
			NVMPageDescriptor* tmp = free_list_dump_;
			page_table_[index].push_back(tmp);
				if (meta != nullptr)
					meta->file_page.push_back(tmp->page_num_);
			page_bitmap_->set(tmp->page_num_);
			free_list_dump_ = free_list_dump_->next_free_;
			tmp->next_free_ = nullptr;
			free_page_num_--;
		}
                first_index_[index] = 0;
                table_addr =  page_table_[index][0]->page_addr_;
                break;
            }
	}
        return table_addr;
    }

    char* NVMPager::GetIndexPtr(int index)
    {
        return page_table_[index][0]->page_addr_;
    }

    // 从段页表中彻底删除掉 index 号的 Table 的 页面信息
    void NVMPager::DeleteSstable(int index)
    {
        for (uint i = first_index_[index]; i < page_num_per_table_; i++) {
            if (page_table_[index][i]) {
                page_table_[index][i]->next_free_ = free_list_dump_;
                page_bitmap_->clr(free_list_dump_->page_num_);
                free_list_dump_ = page_table_[index][i];
                free_page_num_++;
            }
        }
        table_index_bitmap_->clr(index);
        page_table_[index].clear();
    }

    // offset: 释放的空间的结束位置相对于 table_base_ 的偏移
    void NVMPager::ReleasePages(int index, int offset)
    {
        int offset_page_num = offset >> NVMPager::PAGE_SIZE_LOG;
        if (first_index_[index] >= (uint)offset_page_num)
            return;
        for (int i = first_index_[index]; i < offset_page_num; i++) {
            if (page_table_[index][i]) {
                page_table_[index][i]->next_free_ = free_list_dump_;
                page_bitmap_->clr(free_list_dump_->page_num_);
                free_list_dump_ = page_table_[index][i];
                page_table_[index][i] = nullptr;
                free_page_num_++;
            }
        }
    }

    char* NVMPager::GetAddr(int index, int offset)
    {
        uint offset_page_num = offset >> NVMPager::PAGE_SIZE_LOG;
        uint page_offset = offset & ((NVMPager::PAGE_SIZE) - 1);
        return page_table_[index][offset_page_num]->page_addr_ + page_offset;
    }

    //读取size大小的数据，返回成功读取的字节数
    uint64_t NVMPager::ReadData(int index, int offset, uint64_t size, char* buf){ 
		int offset_page_num = offset >> NVMPager::PAGE_SIZE_LOG;
		uint64_t page_offset = offset & ((NVMPager::PAGE_SIZE) - 1);
		uint64_t page_left = NVMPager::PAGE_SIZE - page_offset;
		char* addr;
		uint64_t sz=0,data_size = 0;
		while(data_size < size){
			addr =  page_table_[index][offset_page_num]->page_addr_ + page_offset;
				sz = page_left < size-data_size ? page_left : size-data_size;
				memcpy(buf+data_size,addr,sz);
			data_size += sz;
			offset_page_num+= 1;
			page_offset = 0;
			page_left = NVMPager::PAGE_SIZE;
		}
        return size;
    }

    uint64_t NVMPager::AddData(int index, int offset, uint64_t size, char* buf){ 
		int offset_page_num = offset >> NVMPager::PAGE_SIZE_LOG;
		uint64_t page_offset = offset & ((NVMPager::PAGE_SIZE) - 1);
		uint64_t page_left = NVMPager::PAGE_SIZE - page_offset;
		uint64_t sz;
		char* addr;
		for(uint64_t data_size = 0;data_size < size;){
			addr = page_table_[index][offset_page_num]->page_addr_ + page_offset;
			sz = page_left < size-data_size ? page_left : size-data_size;
			pmem_memcpy_persist(addr , buf + data_size, sz);
			data_size += sz;
			offset_page_num += 1;
			page_offset = 0;
			page_left = NVMPager::PAGE_SIZE;
		}
        return size;
    }
    NVMPageDescriptor::NVMPageDescriptor(uint32_t page_num, char* page_base)
    {
        page_addr_ = page_base;
        page_num_ = page_num;
        next_free_ = nullptr;
    }
	void NVMPager::RecoverNVMPager(const std::vector<FileMetaData*> * L0files){
		init = true;
		if(L0files != nullptr){
		//恢复page_table_
			for(auto filemeta:(*L0files)){
				RecoverAddSstable(filemeta->nvm_sstable_index,filemeta->file_page);
			}
		}
		//恢复freelist
		int page_num = free_page_num_;
        char* page_base = nvm_base_;
        NVMPageDescriptor** pre_page_ptr = &free_list_dump_;
        for (int i = 0; i < page_num; i++) {
			if(page_bitmap_->get(i) == 0){
            	NVMPageDescriptor* new_pd = new NVMPageDescriptor(i, page_base);
            	*pre_page_ptr = new_pd;
            	pre_page_ptr = &(new_pd->next_free_);
			}else
				free_page_num_--;
			page_base += NVMPager::PAGE_SIZE;
        }
	}
	void NVMPager::RecoverAddSstable(int index, std::vector<int> &filepage) {
		if ((uint64_t)index >= table_num_) {
			printf("error:recover file index >= nvm index!\n");
			return;
		}
		if (table_index_bitmap_->get(index) != 0 ) {
			printf("error:recover file index is used !\n");
			return;
		}
        table_index_bitmap_->set(index);
	    int page_index;
        for (uint i = 0; i < page_num_per_table_; i++) {
		    page_index = filepage.at(i);
            NVMPageDescriptor* tmp = new NVMPageDescriptor(page_index, nvm_base_ + NVMPager::PAGE_SIZE*page_index);
			page_table_[index].push_back(tmp);
			page_bitmap_->set(tmp->page_num_);
            free_page_num_--;
        }
        first_index_[index] = 0;
    }

    NVMPageDescriptor::~NVMPageDescriptor()
    {
        // do nothing
    }

}
