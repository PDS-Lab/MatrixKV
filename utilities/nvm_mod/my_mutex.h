#pragma once

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace rocksdb {

class MyMutex {
 public:
  MyMutex();
  ~MyMutex();

  void Lock();
  void Unlock();
  void AssertHeld() { }

 private:
  pthread_mutex_t mu_;

};


}