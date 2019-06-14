#include <cstring>
#include "bitmap.h"

namespace rocksdb {
    BitMap::BitMap() {
        //default 10000
        gsize = (10000 >> 3) + 1;
        bitmap = new char[gsize];
        memset(bitmap, 0, gsize);
    }

    BitMap::BitMap(int n) {
        gsize = (n >> 3) + 1;
        bitmap = new char[gsize];
        memset(bitmap, 0, gsize);
    }

    BitMap::~BitMap() {
        delete[] bitmap;
    }

    int BitMap::get(int x) {
        int cur = x >> 3;
        int remainder = x & (7);
        if (cur > gsize)return -1;

        return (bitmap[cur] >> remainder) & 1;
    }

    int BitMap::set(int x) {
        int cur = x >> 3;
        int remainder = x & (7);
        if (cur > gsize)return 0;
        bitmap[cur] |= 1 << remainder;
        return 1;
    }

    int BitMap::clr(int x) {
        int cur = x >> 3;
        int remainder = x & (7);
        if (cur > gsize)return 0;
        bitmap[cur] ^= 1 << remainder;
        return 1;
    }

    int BitMap::reset(){
        memset(bitmap, 0, gsize);
        return 1;
    }

}