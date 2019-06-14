//
//
//

#pragma once

#include <fcntl.h>
#include <libpmem.h>

#include "common.h"
#include "bitmap.h"
#include "my_log.h"

namespace rocksdb {

class PersistentSstable {
 public:
   PersistentSstable(std::string &path, uint64_t each_size,
                             uint64_t number) {
    char* pmemaddr=nullptr;
    size_t mapped_len;
    int is_pmem;
    uint64_t total_size = each_size * number;
    pmemaddr = (char *)(pmem_map_file(path.c_str(), total_size,
                                                PMEM_FILE_CREATE, 0666,
                                                &mapped_len, &is_pmem));
    RECORD_LOG("creat PersistentSstable path:%s map_len:%f MB is:%d each_size:%f MB number:%lu total_size:%f MB\n",
      path.c_str(),mapped_len/1048576.0,is_pmem,each_size/1048576.0,number,total_size/1048576.0);
    assert(pmemaddr != nullptr);
    raw_ = pmemaddr;
    bitmap_ = new BitMap(number);
    each_size_ = each_size;
    num_ = number;
    use_num_ = 0;
    mapped_len_ = mapped_len;
    is_pmem_ = is_pmem;
  }
  ~PersistentSstable() {
    //Sync();
    delete bitmap_;
    pmem_unmap(raw_, mapped_len_);
  }
  char* AllocSstable(int& index) {
    char* alloc = nullptr;
    for(unsigned int i=0; i < num_; i++){
      if(bitmap_->get(i) == 0) {
        index = (int)i;
        alloc = raw_ + index * each_size_;
        bitmap_->set(index);
        use_num_ = use_num_ + 1;
        return alloc;
      }
    }
    return alloc;
  }

  char* GetIndexPtr(int index) {
    assert((uint64_t)index < num_ && index >= 0);
    return raw_ + index * each_size_;
  }

  void DeleteSstable(int index) {
    size_t pos = index;
    bitmap_->clr(pos);
    use_num_ = use_num_ - 1;
  }
  void Sync(){
    if (is_pmem_)
		pmem_persist(raw_, mapped_len_);
	else
		pmem_msync(raw_, mapped_len_);
  }

  uint64_t GetUseNum() { return use_num_; }
  uint64_t GetNum() { return num_; }
  void Reset() {
    bitmap_->reset();
    use_num_ = 0;
  }
  uint64_t GetEachSize(){ return each_size_;}


 private:
  char* raw_;
  BitMap* bitmap_;
  size_t mapped_len_;
  int is_pmem_;
  uint64_t total_size_;
  uint64_t each_size_;
  uint64_t num_;
  uint64_t use_num_;
};

}  // namespace rocksdb