#include "nvm_module.h"

namespace rocksdb {

NvmModule::NvmModule(NvmOptions *nvmoption):nvmoption_(nvmoption){
    std::string pol_path = nvmoption_->pmem_path + "/";
     pol_path = pol_path + AddNvmmoduleName;
    printf("path:%s\n",pol_path.c_str());
    printf("nvmoption.use_nvm_module:%d\n",nvmoption_->use_nvm_module);
    printf("nvmoption.reset_nvm_storage:%d\n",nvmoption_->reset_nvm_storage);
    printf("nvmoption.pmem_path:%s\n",nvmoption_->pmem_path.c_str());
    printf("nvmoption.pmem_size:%ld\n",nvmoption_->pmem_size);
    

    if (nvm_file_exists(pol_path.c_str()) != 0) {
        
        pop_ = pmem::obj::pool<PersistentInfo>::create(pol_path.c_str(), "NvmModule", nvmoption_->pmem_size,
                                                       CREATE_MODE_RW);
        
    } else {
        pop_ = pmem::obj::pool<PersistentInfo>::open(pol_path.c_str(), "NvmModule");
    }

    pinfo_ = pop_.root();
    /*if (!pinfo_->inited_) {
        transaction::run(pop_, [&] {
            persistent_ptr<PersistentBitMap> bitmap = make_persistent<PersistentBitMap>(pop_, total_range_num);
            pinfo_->ptr_log_ = make_persistent<PersistentLog>(, bitmap);
            bitmap = make_persistent<PersistentBitMap>(pop_, total_range_num);
            pinfo_->ptr_log_ = make_persistent<PersistentSstable>(, bitmap);
            pinfo_->inited_ = true;
        });
    } else if (reset) {
        // reset 
        pinfo_->ptr_log_->Reset();
        
    } else {
        // rebuild cache
        DBG_PRINT("recover cache");
        pinfo_->allocator_->Recover();
        FixedRangeTab::base_raw_ = pinfo_->allocator_->raw();
        
    }*/

}
NvmModule::~NvmModule(){
    printf("module:delete\n");
    pop_.close();
}


}