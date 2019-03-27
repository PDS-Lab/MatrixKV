//
//
//

#pragma once

#include <memory>
#include <string>

//#include "db/dbformat.h"

namespace rocksdb {

  static const int Level0_column_compaction_trigger = 12;     //触发column compaction的文件个数
  static const int Column_compaction_no_L1_select_L0 = 4;     //column compaction时没有L1文件交集时,选择L0数据量进行column compaction的文件个数
  static const int Column_compaction_have_L1_select_L0 = 2;   //column compaction时有L1文件交集时,选择L0数据量进行column compaction的文件个数

struct NvmSetup {
  bool use_nvm_module = false;

  bool reset_nvm_storage = false;

  std::string pmem_path;  //目录

  uint64_t pmem_size = 0;  //主管理模块大小

  NvmSetup& operator=(const NvmSetup& setup) = default;
  NvmSetup() = default;
};

struct NvmOptions {
  NvmOptions() = delete;

  NvmOptions(const std::shared_ptr<NvmSetup> setup);

  ~NvmOptions() {}

  bool use_nvm_module;
  bool reset_nvm_storage;
  std::string pmem_path;
  uint64_t pmem_size;
  uint64_t write_buffer_size;


};
struct NvmCfOptions {
  NvmCfOptions() = delete;

  NvmCfOptions(const std::shared_ptr<NvmSetup> setup,uint64_t s_write_buffer_size,int s_level0_stop_writes_trigger,uint64_t s_target_file_size_base);

  ~NvmCfOptions() {}

  bool use_nvm_module;
  bool reset_nvm_storage;
  std::string pmem_path;
  uint64_t cf_pmem_size;
  uint64_t write_buffer_size;
  int level0_stop_writes_trigger;
  uint64_t target_file_size_base;

};

}  // namespace rocksdb