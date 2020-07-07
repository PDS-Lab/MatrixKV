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
    //ptr_sst_->AddData(file_->sstable_index,0,offset_ + keys_meta_size_,buf_);
    memcpy(nvm_cf_->GetAddr(file_->sstable_index,offset_),key_value.c_str(),total_size);
    
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


L0TableBuilderWithBuffer::L0TableBuilderWithBuffer(NvmCfModule* nvm_cf,
                   FileEntry* file,
                   char* raw):nvm_cf_(nvm_cf),file_(file),raw_(raw){
    offset_ = 0;
    keys_num_ = 0;
    keys_meta_size_ = 0;
    keys_.clear();
    max_size_ = nvm_cf_->GetSstableEachSize();
    buf_ = new char[max_size_];

}
L0TableBuilderWithBuffer::~L0TableBuilderWithBuffer(){
    for(auto key_ : keys_){
        delete key_;
    }
    keys_.clear();
    delete buf_;

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
void L0TableBuilderWithBuffer::Add(const Slice& key, const Slice& value){
    uint64_t total_size = key.size_ + value.size_ + 8 + 8;
    if((offset_ + total_size ) > max_size_){
        printf("error:write l0 sstable size over!\n");
        return;
    }
    std::string key_value;
    PutFixed64(&key_value, key.size_);
    key_value.append(key.data_, key.size_);

    PutFixed64(&key_value, value.size_);
    key_value.append(value.data_, value.size_);

    memcpy(buf_ + offset_,key_value.c_str(),total_size);

    KeysMetadata *tmp = new KeysMetadata();
    tmp->offset = offset_;
    tmp->size = total_size;
    tmp->key.DecodeFrom(key);
    
    keys_.push_back(tmp);
    keys_num_++;

    offset_ += total_size;


}

Status L0TableBuilderWithBuffer::Finish(){
    //assert(file_ != nullptr);
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
    RECORD_LOG("finish L0 table:%lu keynum:%lu size:%.2f MB\n",file_->filenum,file_->keys_num,1.0*offset_/1048576);

    std::string metadatas;
    for(unsigned i=0;i < file_->keys_num;i++){
        Slice key = file_->keys_meta[i].key.Encode();
        PutFixed64(&metadatas,key.size());
        metadatas.append(key.data(),key.size());
        PutFixed32(&metadatas,file_->keys_meta[i].next);
        PutFixed64(&metadatas,file_->keys_meta[i].offset);
        PutFixed64(&metadatas,file_->keys_meta[i].size);
    }
    if((offset_ + metadatas.size()) > max_size_){
        printf("error:write l0 sstable's metadata size over!size:%lu max:%lu\n",offset_ + metadatas.size(),max_size_);
        return Status::IOError();
    }
    keys_meta_size_ = metadatas.size();
    memcpy(buf_ + offset_, metadatas.c_str(), metadatas.size());
    nvm_cf_->AddData(file_->sstable_index,0,offset_ + keys_meta_size_,buf_);
    RECORD_LOG("finish L0 table:%lu keynum:%lu size:%.2f MB metadata:%.2f MB\n",file_->filenum,file_->keys_num,1.0*offset_/1048576,metadatas.size()/1048576.0);

    
return Status();

}




}
