//
//
//
#pragma once

#include <cstdio>

#include "common.h"
#include "my_log.h"

namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;

class PersistentBitMap {
 public:
  PersistentBitMap(pool_base& pop, size_t maplen);

  ~PersistentBitMap();

  int GetBit();

  bool SetBit(size_t pos, bool flag);

  bool GetAndSet();

  void Reset();

  void Print();
  void Delete();

 private:
  pool_base& pop_;
  p<size_t> len_;
  p<size_t> bytes_;
  persistent_ptr<unsigned char[]> bitmap_ = nullptr;
};
}  // namespace rocksdb
