//
//
//

#pragma once

#include <fcntl.h>

#include "common.h"
#include "persistent_bitmap.h"
#include "my_log.h"

namespace rocksdb {

using namespace pmem;
using namespace pmem::obj;

class PersistentSstable {
 public:
   PersistentSstable(std::string &path, uint64_t each_size,
                             uint64_t number,
                             persistent_ptr<PersistentBitMap> bitmap) {
    char* pmemaddr=nullptr;
    size_t mapped_len;
    int is_pmem;
    uint64_t total_size = each_size * number;
    pmemaddr = (char *)(pmem_map_file(path.c_str(), total_size,
                                                PMEM_FILE_CREATE, 0666,
                                                &mapped_len, &is_pmem));
    RECORD_LOG("creat PersistentSstable path:%s map:%lu is:%d each_size:%lu number:%lu total_size:%lu\n",path.c_str(),mapped_len,is_pmem,each_size,number,total_size);
    assert(pmemaddr != nullptr);
    raw_ = pmemaddr;
    bitmap_ = bitmap;
    each_size_ = each_size;
    num_ = number;
    use_num_ = 0;
    mapped_len_ = mapped_len;
    is_pmem_ = is_pmem;
  }
  ~PersistentSstable() {
    pmem_unmap(raw_, mapped_len_);
  }
  char* AllocSstable(int& index) {
    index = bitmap_->GetBit();
    char* alloc = nullptr;
    if (index != -1) {
      alloc = raw_ + index * each_size_;
      bitmap_->SetBit(index, true);
      use_num_ = use_num_ + 1;
    }
    return alloc;
  }

  char* GetIndexPtr(int index) {
    assert((uint64_t)index < num_ && index >= 0);
    return raw_ + index * each_size_;
  }

  void DeleteSstable(int index) {
    size_t pos = index;
    bitmap_->SetBit(pos, false);
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
    bitmap_->Reset();
    use_num_ = 0;
  }
  uint64_t GetEachSize(){ return each_size_;}


 private:
  char* raw_;
  persistent_ptr<PersistentBitMap> bitmap_;
  p<size_t> mapped_len_;
  p<int> is_pmem_;
  p<uint64_t> total_size_;
  p<uint64_t> each_size_;
  p<uint64_t> num_;
  p<uint64_t> use_num_;
};

}  // namespace rocksdb