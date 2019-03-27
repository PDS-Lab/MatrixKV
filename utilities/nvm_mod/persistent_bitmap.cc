//
//
//
#include <cassert>
#include <cmath>

#include "persistent_bitmap.h"

namespace rocksdb {
PersistentBitMap::PersistentBitMap(pool_base& pop, size_t maplen)
    : pop_(pop), len_(maplen) {
  bytes_ = static_cast<size_t>(ceil(len_ / 8.0));
  assert(bytes_ > 0);
  if (bitmap_ == nullptr){
    transaction::run(pop,[&] { 
      bitmap_ = make_persistent<unsigned char[]>(bytes_); 
    });
  }
  RECORD_LOG("creat PersistentBitMap len:%d\n",maplen);

  for (size_t i = 0; i < bytes_; i++) {
    bitmap_[i] = 0;
  }
}

PersistentBitMap::~PersistentBitMap() {
  RECORD_LOG("PersistentBitMap:close\n");
  /*transaction::run(pop_, [&] { 
      delete_persistent<unsigned char[]>(bitmap_, bytes_); 
  });*/
}
void PersistentBitMap::Delete(){
  transaction::run(pop_, [&] { 
      delete_persistent<unsigned char[]>(bitmap_, bytes_); 
  });
}

int PersistentBitMap::GetBit() {
  for (size_t i = 0; i < bytes_; i++) {
    unsigned char ruler = 0x01;
    for (size_t j = 1; j <= 8; j++) {
      if (!((bitmap_[i] >> (8 - j)) & ruler)) {
        return static_cast<int>(i * 8 + (j - 1));
      }
    }
  }
  return -1;
}

bool PersistentBitMap::SetBit(size_t pos, bool flag) {
  if (pos > len_) {
    return false;
  }

  unsigned char ruler = 0x80;
  size_t byte_pos = pos / 8;
  size_t bit_pos = pos % 8;

  if (flag) {
    ruler >>= bit_pos;
    bitmap_[byte_pos] |= ruler;
  } else {
    ruler >>= bit_pos;
    ruler ^= 0xFF;
    bitmap_[byte_pos] &= ruler;
  }

  return true;
}

bool PersistentBitMap::GetAndSet() {
  int get = GetBit();
  if (get == -1) {
    return false;
  } else {
    return SetBit(get, true);
  }
}

void PersistentBitMap::Reset() {
  for (size_t i = 0; i < bytes_; i++) {
    bitmap_[i] = 0;
  }
}

void PersistentBitMap::Print() {
  for (size_t i = 0; i < len_; i++) {
    size_t byte_pos = i / 8;
    size_t bit_pos = i % 8;

    unsigned char ruler = 0x01;
    printf("%lu[%d] ", i, (bitmap_[byte_pos] >> (7 - bit_pos)) & ruler);
  }
  printf("\n");
}

}  // namespace rocksdb
