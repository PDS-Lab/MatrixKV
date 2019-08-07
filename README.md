# RocksDB for nvm


## 记录与问题
**2019_2_18:**

`commit 641fae60f63619ed5d0c9d9e4c4ea5a0ffa3e253`

初始版本：未修改的rocksdb

**2019_3_30:**

第一版完成：

`commit 620c490f874ce54deda9aef19939401b99f96660`

存在的问题：
1. 版本控制version使用的比较勉强，暂时无bug，没有完全融入进去，有待改善；
2. 读请求时对key的type没有解析，例如删除、合并等，有待改善。
