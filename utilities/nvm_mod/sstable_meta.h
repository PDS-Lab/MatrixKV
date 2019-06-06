//
//功能：
//

#pragma once

#include <memory>
#include <string>

#include "db/dbformat.h"
#include "db/version_edit.h"

#include "common.h"
#include "nvm_option.h"
#include "persistent_bitmap.h"
#include "persistent_sstable.h"

namespace rocksdb {
using namespace pmem;
using namespace pmem::obj;
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
    p<uint64_t> filenum;
    p<int> sstable_index;
    //p<uint64_t> offset;          /不需要也可，在keys_meta[first_key_index].offset就是
    struct KeysMetadata* keys_meta = nullptr; //指向多个（keys_num个）连续内存的KeysMetadata
    p<uint64_t> keys_num;
   // p<uint64_t> first_key_index;          //index 从0开始,在FileMetaData中保存
    p<uint64_t> key_point_filenum;       //key 指向下一个文件的filenum，防止中间删除了文件
    //persistent_ptr<KeysMetadata> keys_meta = nullptr;  //先不存AEP
    //InternalKey smallest;           
    //InternalKey largest;

    persistent_ptr<FileEntry> next = nullptr;
    persistent_ptr<FileEntry> prev = nullptr;
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
  SstableMetadata(pool_base& pop,const InternalKeyComparator* icmp,int level0_stop_writes_trigger);
  ~SstableMetadata();
  persistent_ptr<FileEntry> AddFile(uint64_t filenumber,int index);
  persistent_ptr<FileEntry> FindFile(uint64_t filenumber,bool forward = true,bool have_error_print = true);
  bool DeteleFile(uint64_t filenumber);

  void DeleteCompactionFile(uint64_t filenumber);

  void UpdateKeyNext(persistent_ptr<FileEntry> &file);
  void UpdateCompactionState(std::vector<FileMetaData*>& L0files);

  //uint64_t GetImmuFileEntryNum();


persistent_ptr<FileEntry> head = nullptr;  //链表，新的插入头，旧的在尾，保持所有L0的sstable
persistent_ptr<FileEntry> tail = nullptr;
p<uint64_t> new_file_num;

//persistent_ptr<FileEntry> immu_head = nullptr;  //L0 commpaction file，只是复制指针，
//persistent_ptr<FileEntry> immu_tail = nullptr;
std::vector<uint64_t> compaction_files;    //L0 commpaction file


 private:
 //persistent_ptr<FileEntry> ImmuFindFile(uint64_t filenumber,bool forward = true);
 //bool ImmuDeteleFile(uint64_t filenumber);

  pool_base& pop_;
  p<int> level0_stop_writes_trigger_;

///
  const InternalKeyComparator* icmp_;
///

};

}  // namespace rocksdb