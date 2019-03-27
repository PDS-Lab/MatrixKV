//
//功能：
//

#pragma once

#include <memory>
#include <string>

#include "common.h"


#include "sstable_meta.h"
#include "nvm_cf_mod.h"


namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;

class L0TableBuilder{
public:
    L0TableBuilder(NvmCfModule* nvm_cf,
                   persistent_ptr<FileEntry> &file,
                   char* raw);
    ~L0TableBuilder();
    void Add(const Slice& key, const Slice& value);
    Status Finish();

    uint64_t GetFileSize(){
        return offset_;
    }




private:
    NvmCfModule * nvm_cf_;
    persistent_ptr<FileEntry> file_;
    char* raw_;
    std::vector<KeysMetadata *> keys_;
    uint64_t keys_num_;

    uint64_t offset_;




};






}