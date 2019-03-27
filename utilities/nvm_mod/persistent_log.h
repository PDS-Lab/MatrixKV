//
//
//

#pragma once

#include <fcntl.h>

#include "common.h"
#include "persistent_bitmap.h"

namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;

class PersistentLog {
 public:
   PersistentLog(std::string path, uint64_t each_size, uint64_t number,
                         persistent_ptr<PersistentBitMap> bitmap) {
    char* pmemaddr;
    size_t mapped_len;
    int is_pmem;
    uint64_t total_size = each_size * number;

    pmemaddr = static_cast<char*>(pmem_map_file(path.c_str(), total_size,
                                                PMEM_FILE_CREATE, 0666,
                                                &mapped_len, &is_pmem));
    assert(pmemaddr != nullptr);
    raw_ = pmemaddr;
    bitmap_ = bitmap;
    each_size_ = each_size;
    num_ = number;
    use_num_ = 0;
  }
  ~PersistentLog() {
    
  }
  char* AllocLog(int& index) {
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
    assert((uint64_t) index < num_ && index >= 0);
    return raw_ + index * each_size_;
  }

  void DeleteLog(int index) {
    bitmap_->SetBit(index, false);
    use_num_=use_num_ - 1;
  }

  uint64_t GetUseNum() { return use_num_; }
  uint64_t GetNum() { return num_; }
  void Reset(std::string path, uint64_t each_size, uint64_t number,
             persistent_ptr<PersistentBitMap> bitmap) {
    char* pmemaddr;
    size_t mapped_len;
    int is_pmem;
    uint64_t total_size = each_size * number;

    pmemaddr = static_cast<char*>(pmem_map_file(path.c_str(), total_size,
                                                PMEM_FILE_CREATE, 0666,
                                                &mapped_len, &is_pmem));
    assert(pmemaddr != nullptr);
    raw_ = pmemaddr;
    bitmap_ = bitmap;
    each_size_ = each_size;
    num_ = number;
    use_num_ = 0;
  }

 private:
  char* raw_;
  persistent_ptr<PersistentBitMap> bitmap_;
  p<uint64_t> total_size_;
  p<uint64_t> each_size_;
  p<uint64_t> num_;
  p<uint64_t> use_num_;
};

}  // namespace rocksdb