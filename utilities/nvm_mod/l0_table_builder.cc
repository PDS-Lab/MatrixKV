#include "l0_table_builder.h"

namespace rocksdb {

L0TableBuilder::L0TableBuilder(NvmCfModule* nvm_cf,
                   FileEntry* file,
                   char* raw):nvm_cf_(nvm_cf),file_(file),raw_(raw){
    offset_ = 0;
    keys_num_ = 0;
    keys_.clear();

}
L0TableBuilder::~L0TableBuilder(){
    for(auto key_ : keys_){
        delete key_;
    }
    keys_.clear();

}
/* ------------
 * | key_size | // 64bit
 * ------------
 * |    key   |
 * ------------
 * |value_size| // 64bit
 * ------------
 * |   value  |
 * ------------
 * */
void L0TableBuilder::Add(const Slice& key, const Slice& value){
    uint64_t total_size = key.size_ + value.size_ + 8 + 8;
    if((offset_ + total_size ) > nvm_cf_->GetSstableEachSize()){
        printf("error:write l0 sstable size over!\n");
        return;
    }
    std::string key_value;
    PutFixed64(&key_value, key.size_);
    key_value.append(key.data_, key.size_);

    PutFixed64(&key_value, value.size_);
    key_value.append(value.data_, value.size_);

    memcpy(raw_ + offset_,key_value.c_str(),total_size);

    KeysMetadata *tmp = new KeysMetadata();
    tmp->offset = offset_;
    tmp->size = total_size;
    tmp->key.DecodeFrom(key);
    
    keys_.push_back(tmp);
    keys_num_++;

    offset_ += total_size;


}

Status L0TableBuilder::Finish(){
    file_->keys_num = keys_num_;
    file_->keys_meta = new KeysMetadata[keys_num_];
    int index =0;
    for(auto key_ : keys_){
        file_->keys_meta[index].key = key_->key;
        file_->keys_meta[index].offset=key_->offset;
        file_->keys_meta[index].size = key_->size;
        index++;
    }
///key元数据是否加入sstable
///更新keys的next
    nvm_cf_->UpdateKeyNext(file_);
    RECORD_LOG("finish L0 table:%lu keynum:%lu size:%2.f MB\n",file_->filenum,file_->keys_num,1.0*offset_/1048576);

    
return Status();

}





}