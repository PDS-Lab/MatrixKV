//
//
//

#include "rocksdb/nvm_option.h"
#include "my_log.h"

namespace rocksdb {

/* NvmOptions::NvmOptions(const std::shared_ptr<NvmSetup> setup) {
  use_nvm_module = setup->use_nvm_module;
  //reset_nvm_storage = setup->reset_nvm_storage;
  pmem_path = setup->pmem_path;
  //pmem_size = setup->pmem_size;

} */
NvmCfOptions::NvmCfOptions(const std::shared_ptr<NvmSetup> setup,uint64_t s_write_buffer_size,int s_max_write_buffer_number,int s_level0_stop_writes_trigger,uint64_t s_target_file_size_base){
  use_nvm_module = setup->use_nvm_module;
  //reset_nvm_storage = setup->reset_nvm_storage;
  pmem_path = setup->pmem_path;

  Level0_column_compaction_trigger_size = setup->Level0_column_compaction_trigger_size;
  Level0_column_compaction_slowdown_size = setup->Level0_column_compaction_slowdown_size;  
  Level0_column_compaction_stop_size = setup->Level0_column_compaction_stop_size;

  Column_compaction_no_L1_select_L0 = setup->Column_compaction_no_L1_select_L0;     
  Column_compaction_have_L1_select_L0 = setup->Column_compaction_have_L1_select_L0;


  write_buffer_size = s_write_buffer_size;
  max_write_buffer_number = s_max_write_buffer_number;
  level0_stop_writes_trigger = s_level0_stop_writes_trigger;
  //cf_pmem_size = 1ul * 1024 * 1024 * 1024;
  target_file_size_base = s_target_file_size_base;
  RECORD_LOG("use_nvm_module:%d pmem_path:%s write_buffer_size:%f MB \n \
            Level0_column_compaction_trigger_size:%f MB  \n \
            Level0_column_compaction_slowdown_size:%f MB  \n \
            Level0_column_compaction_stop_size:%f MB  \n \
            Column_compaction_no_L1_select_L0:%d \n \
            Column_compaction_have_L1_select_L0:%d \n \
            level0_stop_writes_trigger:%d target_file_size_base:%f MB\n",
    use_nvm_module,pmem_path.c_str(),write_buffer_size/1048576.0, Level0_column_compaction_trigger_size/1048576.0,
    Level0_column_compaction_slowdown_size/1048576.0, Level0_column_compaction_stop_size/1048576.0,
    Column_compaction_no_L1_select_L0, Column_compaction_have_L1_select_L0,
    level0_stop_writes_trigger,target_file_size_base/1048576.0);
}

}  // namespace rocksdb