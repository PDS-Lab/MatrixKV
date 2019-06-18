//
//功能：
//

#pragma once

#include <memory>
#include <string>

#include "db/dbformat.h"
#include "db/version_edit.h"

#include "nvm_option.h"
#include "my_log.h"
#include "my_mutex.h"

namespace rocksdb {
class InternalKeyComparator;

//static const uint64_t MAX_INTERNAL_KEY_SIZE=64;

struct KeysMetadata{
  InternalKey key;   //InternalKey 的key
  int32_t next;   //指向下一个key的index,空为-1,从0开始。
  uint64_t offset;  //key-value结构的offset
  uint64_t size;  //key-value结构的大小

  KeysMetadata(){
    next = -1;
    offset = 0;
    size = 0;
  }
  ~KeysMetadata(){}

};



struct FileEntry{
    uint64_t filenum;
    int sstable_index;
    //p<uint64_t> offset;          /不需要也可，在keys_meta[first_key_index].offset就是
    struct KeysMetadata* keys_meta = nullptr; //指向多个（keys_num个）连续内存的KeysMetadata
    uint64_t keys_num;
   // uint64_t first_key_index;          //index 从0开始,在FileMetaData中保存
    uint64_t key_point_filenum;       //key 指向下一个文件的filenum，防止中间删除了文件
  
    FileEntry(uint64_t a,int b):filenum(a),sstable_index(b){
      keys_num = 0;
      key_point_filenum = 0;
    }
    ~FileEntry(){
      if(keys_meta != nullptr){
        delete []keys_meta;
      }
    }

};

class SstableMetadata {
 public:
  SstableMetadata(const InternalKeyComparator* icmp);
  ~SstableMetadata();
  FileEntry* AddFile(uint64_t filenumber,int index);
  FileEntry* FindFile(uint64_t filenumber,bool forward = true,bool have_error_print = true);
  bool DeteleFile(uint64_t filenumber);

  void DeleteCompactionFile(uint64_t filenumber);

  void UpdateKeyNext(FileEntry* file = nullptr);
  void UpdateCompactionState(std::vector<FileMetaData*>& L0files);
  uint64_t GetFilesNumber();

  //uint64_t GetImmuFileEntryNum();
  std::vector<uint64_t> compaction_files;    //L0 commpaction file,只有单个线程会用到
private:
  std::vector<FileEntry*> files_;   ////vector，新的插入头，旧的在尾，保持所有L0的sstable

///
  const InternalKeyComparator* icmp_;
///
  MyMutex *mu_;   //互斥锁，可优化为读写锁

};

}  // namespace rocksdb