//
//
//

#include "nvm_option.h"

namespace rocksdb {

NvmOptions::NvmOptions(const std::shared_ptr<NvmSetup> setup) {
  use_nvm_module = setup->use_nvm_module;
  reset_nvm_storage = setup->reset_nvm_storage;
  pmem_path = setup->pmem_path;
  //pmem_size = setup->pmem_size;

}
NvmCfOptions::NvmCfOptions(const std::shared_ptr<NvmSetup> setup,uint64_t s_write_buffer_size,int s_max_write_buffer_number,int s_level0_stop_writes_trigger,uint64_t s_target_file_size_base){
  use_nvm_module = setup->use_nvm_module;
  reset_nvm_storage = setup->reset_nvm_storage;
  pmem_path = setup->pmem_path;
  write_buffer_size = s_write_buffer_size;
  max_write_buffer_number = s_max_write_buffer_number;
  level0_stop_writes_trigger = s_level0_stop_writes_trigger;
  //cf_pmem_size = 1ul * 1024 * 1024 * 1024;
  target_file_size_base = s_target_file_size_base;
}

}  // namespace rocksdb