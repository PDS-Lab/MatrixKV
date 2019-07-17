//
//
//

#pragma once

#include <memory>
#include <string>
#include "my_log.h"
//#include "db/dbformat.h"

namespace rocksdb {

  //static const int Level0_column_compaction_trigger = 12;     //触发column compaction的文件个数，level0_stop_writes_trigger/8
  //static const int Max_Level0_column_compaction_file = 16;    //最大一起进行column compaction的文件个数,level0_stop_writes_trigger/6
  //static const double Beyond_this_delay_column_compaction = 4;  //当其它层(除了L0层)的数据量/阈值比值超过这个值时，其它层compaction优先
  static const uint64_t Level0_column_compaction_trigger_size = 7ul * 1024 * 1024 * 1024; //7G trigger
  static const uint64_t Level0_column_compaction_slowdown_size = 7ul * 1024 * 1024 * 1024 + 512ul * 1024 * 1024; //7.5G slowdown
  static const uint64_t Level0_column_compaction_stop_size = 8ul * 1024 * 1024 * 1024; //8G stop
  static const int Column_compaction_no_L1_select_L0 = 4;     //column compaction时没有L1文件交集时,至少选择L0数据量进行column compaction的文件个数
  static const int Column_compaction_have_L1_select_L0 = 2;   //column compaction时有L1文件交集时,至少选择L0数据量进行column compaction的文件个数

struct NvmSetup {
  bool use_nvm_module = false;

  //bool reset_nvm_storage = false;

  std::string pmem_path;  //目录

  //uint64_t pmem_size = 0;  //主管理模块大小

  NvmSetup& operator=(const NvmSetup& setup) = default;
  NvmSetup() = default;
};

struct NvmOptions {
  NvmOptions() = delete;

  NvmOptions(const std::shared_ptr<NvmSetup> setup);

  ~NvmOptions() {}

  bool use_nvm_module;
  //bool reset_nvm_storage;
  std::string pmem_path;
  //uint64_t pmem_size;
  uint64_t write_buffer_size;


};
struct NvmCfOptions {
  NvmCfOptions() = delete;

  NvmCfOptions(const std::shared_ptr<NvmSetup> setup,uint64_t s_write_buffer_size,int s_max_write_buffer_number,int s_level0_stop_writes_trigger,uint64_t s_target_file_size_base);

  ~NvmCfOptions() {}

  bool use_nvm_module;
  //bool reset_nvm_storage;
  std::string pmem_path;
  //uint64_t cf_pmem_size;
  uint64_t write_buffer_size;
  int max_write_buffer_number;
  int level0_stop_writes_trigger;
  uint64_t target_file_size_base;

};

}  // namespace rocksdb