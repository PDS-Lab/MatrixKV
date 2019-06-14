
//////
//Module function: bitmap management
//////

#pragma once
namespace rocksdb{
    
class BitMap {

    public:
        BitMap();

        BitMap(int n);

        ~BitMap();

        int get(int x);

        int set(int x);

        int clr(int x);

        int reset();
        
    private:
        char *bitmap;
        int gsize;
    }; 
}
