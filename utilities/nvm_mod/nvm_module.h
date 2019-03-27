//
//功能：nvm管理模块
//

#pragma once

#include <memory>
#include <string>

#include "common.h"
#include "nvm_option.h"
#include "persistent_bitmap.h"
#include "persistent_log.h"
#include "persistent_sstable.h"

namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;
struct NvmOptions;

class NvmModule {
 public:
  NvmModule(NvmOptions* nvmoption);
  ~NvmModule();
  bool WriteLogFile();
  bool WriteL0File();
  bool ReadLogFile();
  bool ReadL0File();

 private:
  NvmOptions* nvmoption_;

  struct PersistentInfo {
    p<bool> inited_;
    persistent_ptr<PersistentBitMap> log_bitmap_;
    persistent_ptr<PersistentBitMap> sst_bitmap_;
    persistent_ptr<PersistentLog> ptr_log_;
    persistent_ptr<PersistentSstable> ptr_sst_;
  };

  pool<PersistentInfo> pop_;
  persistent_ptr<PersistentInfo> pinfo_;
};

}  // namespace rocksdb