# MatrixKV
## 1 Introduction
MatrixKV is the implementation of the paper "**MatrixKV: Reducing Write Stalls and Write Amplification in LSM-tree Based KV Stores with a Matrix Container in NVM**" appeared in [ATC 2020](https://www.usenix.org/conference/atc20).
We implement MatrixKV based on [RocksDB](https://github.com/facebook/rocksdb) and evaluate it on a hybrid DRAM/NVM/SSD system using Intel's latest 3D Xpoint NVM device Optane DC PMM. 
****Version Modify****
This code adds NVM page management based on MatrixKV.The code uses the NVM page as the fundamental storage unit.

## 2 Compilation and Run
### 2.1 Tools
MatrixKV acesses NVM via [PMDK](https://github.com/pmem/pmdk). To run MatrixKV, please install PMDK first.


### 2.2 Compilation
We only support Makefile instead of cmake currently.
```
> make -j64   
```


### 2.3 Run
To run MatrixKV, please modify the configuration in ``include/rocksdb/option.h``.
```
std::shared_ptr<NvmSetup> nvm_setup = nullptr;
```
To learn more about ``NvmSetup``, please refer to ``include/rocksdb/nvm_option.h``.


To test with db_bench, please refer to the test script 
``test_sh/matrixkv_test_4value.sh `` and follow the next two steps:
```
> ./tesh_sh/matrixkv_test_4value.sh
> nohup ./tesh_sh/matrixkv_test_4value.sh >out.out 2>&1 &     ##Run in the background
```

## 3 MatrixKV's differences with RocksDB
MatrixKV is implemented based on RocksDB Version v5.18.3. If you want to have RocksDB under the same version, do:
```
> git checkout 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```
To check the difference between RocksDB and MatrixKV, please do:
```
> git reset --mixed 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253
```

## 4 Acknowledgement
We appreciate PingCap and Intel for the hardware support and maintenance.

## 5 Contributors
- Ph.D supervisor: Jiguang Wan  (jgwan@hust.edu.cn)
- Ting Yao (tingyao@hust.edu.cn)
- Zhiwen Liu (993096281@qq.com)
- Yiwen Zhang (zhangyiwen@hust.edu.cn)
