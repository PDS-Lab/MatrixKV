#pragma once

#include "common.h"

#include "table/internal_iterator.h"
#include "sstable_meta.h"

namespace rocksdb {
using std::vector;


class ColumnCompactionItemIterator : public InternalIterator {
public:
    ColumnCompactionItemIterator(char *raw_data,FileEntry* file,uint64_t first_key_index,uint64_t keys_num):
                                   raw_data_(raw_data),file_(file){
        current_ = -1;
        vKey_.reserve(keys_num);
        vValue_.reserve(keys_num);
        char* data_addr = raw_data_;

        //uint64_t key_value_size = 0;
        uint64_t key_value_offset = 0;
        uint64_t key_size = 0;
        uint64_t value_size = 0;
        uint64_t offset = 0;
        for(size_t i = 0;i < keys_num; i++){  //注意：Slice结构只保留了char *指针和大小，并没有拷贝数据
            //key_value_size = file_->keys_meta[file_->first_key_index + i].size;
            key_value_offset = file_->keys_meta[first_key_index + i].offset;

            offset = key_value_offset;
            key_size = DecodeFixed64(data_addr + offset);
            offset += 8;
            vKey_.emplace_back(data_addr + offset,key_size);
            offset += key_size;
            value_size = DecodeFixed64(data_addr + offset);
            offset += 8;
            vValue_.emplace_back(data_addr + offset,value_size);
        }


    }
    ~ColumnCompactionItemIterator(){

    }

    
    bool Valid() const override {
        //uint64_t xx= file_->filenum;
        //printf("file:%lu xdc:%d m:%lu\n",xx,current_,vKey_.size());
        if(current_ >= 0 && current_ < (int)vKey_.size()){
            return true;
        }
        else{
            //printf("nono\n");
            return false;
        }
    }
  
    void SeekToFirst() override {
        current_ = 0;
    }

 
    void SeekToLast() override{
        current_ = vKey_.size() - 1;
    }

  
    void Seek(const Slice& ) override {
        //TODO
        printf("No seek!\n");
    };

  
    void SeekForPrev(const Slice& ) override {
        //TODO
        printf("No SeekForPrev!\n");
    };

  
    void Next() override {
        //assert(Valid());
        ++current_;
    };

  
    void Prev() override {
        assert(Valid());
        --current_;
    };

  
    Slice key() const override {
        return vKey_.at(current_);
    };

  
    Slice value() const override {
        return vValue_.at(current_);
    };

  
    Status status() const override {
       return Status::OK();
    }

private:
    char *raw_data_;
    FileEntry* file_;
    vector<Slice> vKey_;
    vector<Slice> vValue_;
    int current_;

    
    


};

InternalIterator* NewColumnCompactionItemIterator(char *raw_data,FileEntry* file,uint64_t first_key_index,uint64_t keys_num);
   
}