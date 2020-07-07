#! /bin/sh

#value_array=(1024 4096 16384 65536)
#value_array=(4096)
#test_all_size=81920000000   #8G
nvm_size_array=(2 4 8 16 32)  #GB

bench_db_path="/mnt/ssd/ceshi"
wal_dir="/pmem/log"
bench_value="4096"
bench_compression="none" #"snappy,none"

#bench_benchmarks="fillseq,stats,readseq,readrandom,stats" #"fillrandom,fillseq,readseq,readrandom,stats"
#bench_benchmarks="fillrandom,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,clean_cache,stats,readseq,readrandom,stats"
#bench_benchmarks="fillrandom,stats,sleep20s,clean_cache,stats,readseq,clean_cache,stats,readrandom,stats"
#bench_benchmarks="fillrandom,stats,readrandom,wait,clean_cache,stats,readseq,clean_cache,stats,readrandom,stats"
bench_benchmarks="fillrandom,stats,wait,clean_cache,stats,readrandom,stats"
#bench_benchmarks="fillseq,stats"
bench_num="20000000"
bench_readnum="1000000"
#bench_max_open_files="1000"
max_background_jobs="3"
max_bytes_for_level_base="`expr 8 \* 1024 \* 1024 \* 1024`" 
#max_bytes_for_level_base="`expr 256 \* 1024 \* 1024`" 

pmem_path="/pmem/nvm"
use_nvm="true"

level0_column_compaction_trigger_size="`expr 7 \* 1024 \* 1024 \* 1024`"    #7G
level0_column_compaction_slowdown_size="`expr 7 \* 1024 \* 1024 \* 1024 + 512 \* 1024 \* 1024`"   #7.5G
level0_column_compaction_stop_size="`expr 8 \* 1024 \* 1024 \* 1024`"       #8G

#report_write_latency="true"
report_write_latency="false"

bench_file_path="$(dirname $PWD )/db_bench"

bench_file_dir="$(dirname $PWD )"

if [ ! -f "${bench_file_path}" ];then
bench_file_path="$PWD/db_bench"
bench_file_dir="$PWD"
fi

if [ ! -f "${bench_file_path}" ];then
echo "Error:${bench_file_path} or $(dirname $PWD )/db_bench not find!"
exit 1
fi

RUN_ONE_TEST() {
    const_params="
    --db=$bench_db_path \
    --wal_dir=$wal_dir \
    --value_size=$bench_value \
    --benchmarks=$bench_benchmarks \
    --num=$bench_num \
    --reads=$bench_readnum \
    --compression_type=$bench_compression \
    --max_background_jobs=$max_background_jobs \
    --max_bytes_for_level_base=$max_bytes_for_level_base \
    --report_write_latency=$report_write_latency \
    --use_nvm_module=$use_nvm \
    --pmem_path=$pmem_path \
    --level0_column_compaction_trigger_size=$level0_column_compaction_trigger_size \
    --level0_column_compaction_slowdown_size=$level0_column_compaction_slowdown_size \
    --level0_column_compaction_stop_size=$level0_column_compaction_stop_size \
    "
    cmd="$bench_file_path $const_params >>out.out 2>&1"
    echo $cmd >out.out
    echo $cmd
    eval $cmd
}

CLEAN_CACHE() {
    if [ -n "$bench_db_path" ];then
        rm -f $bench_db_path/*
    fi
    sleep 2
    sync
    echo 3 > /proc/sys/vm/drop_caches
    sleep 2
}

COPY_OUT_FILE(){
    mkdir $bench_file_dir/result > /dev/null 2>&1
    res_dir=$bench_file_dir/result/nvm-size-`expr $level0_column_compaction_stop_size / 1024 / 1024 / 1024`
    mkdir $res_dir > /dev/null 2>&1
    \cp -f $bench_file_dir/compaction.csv $res_dir/
    \cp -f $bench_file_dir/OP_DATA $res_dir/
    \cp -f $bench_file_dir/OP_TIME.csv $res_dir/
    \cp -f $bench_file_dir/out.out $res_dir/
    #\cp -f $bench_file_dir/Latency.csv $res_dir/
    #\cp -f $bench_file_dir/NVM_LOG $res_dir/
    \cp -f $bench_db_path/OPTIONS-* $res_dir/
    #\cp -f $bench_db_path/LOG $res_dir/
}
RUN_ALL_TEST() {
    for nvm_size in ${nvm_size_array[@]}; do
        CLEAN_CACHE
        
        level0_column_compaction_trigger_size="`expr \( $nvm_size - 1 \) \* 1024 \* 1024 \* 1024`"    #7G
        level0_column_compaction_slowdown_size="`expr \( $nvm_size - 1 \) \* 1024 \* 1024 \* 1024 + 512 \* 1024 \* 1024`"   #7.5G
        level0_column_compaction_stop_size="`expr $nvm_size \* 1024 \* 1024 \* 1024`"       #8G

        RUN_ONE_TEST
        if [ $? -ne 0 ];then
            exit 1
        fi
        COPY_OUT_FILE
        sleep 5
    done
}

RUN_ALL_TEST
