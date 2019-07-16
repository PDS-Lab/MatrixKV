#! /bin/sh

#value_array=(1024 4096 16384 65536)
value_array=(4096)
test_all_size=81920000000   #80G


bench_db_path="/mnt/ssd/ceshi"
#bench_level0_file_path="/pmem/ceshi"
bench_level0_file_path=""
bench_value="4096"
bench_compression="none" #"snappy,none"

#bench_benchmarks="fillseq,stats,readseq,readrandom,stats" #"fillrandom,fillseq,readseq,readrandom,stats"
#bench_benchmarks="fillrandom,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,stats,readseq,readrandom,readrandom,readrandom,stats"
#bench_benchmarks="fillrandom,stats,wait,clean_cache,stats,readseq,stats,clean_cache,readrandom,stats"
bench_benchmarks="fillrandom,stats,wait,stats,clean_cache,stats,readrandom,stats"
#bench_benchmarks="fillseq,stats"
bench_num="20000000"
bench_readnum="1000000"
#bench_max_open_files="1000"
max_background_jobs="2"
#max_bytes_for_level_base="`expr 8 \* 1024 \* 1024 \* 1024`" 
max_bytes_for_level_base="`expr 256 \* 1024 \* 1024`" 

perf_level="4"
#perf_level="1"

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
    --level0_file_path=$bench_level0_file_path \
    --perf_level=$perf_level  \
    --value_size=$bench_value \
    --benchmarks=$bench_benchmarks \
    --num=$bench_num \
    --reads=$bench_readnum \
    --compression_type=$bench_compression \
    --max_background_jobs=$max_background_jobs \
    --max_bytes_for_level_base=$max_bytes_for_level_base \
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
    res_dir=$bench_file_dir/result/value-$bench_value
    mkdir $res_dir > /dev/null 2>&1
    \cp -f $bench_file_dir/compaction.csv $res_dir/
    \cp -f $bench_file_dir/OP_DATA $res_dir/
    \cp -f $bench_file_dir/OP_TIME.csv $res_dir/
    \cp -f $bench_file_dir/out.out $res_dir/
    \cp -f $bench_db_path/OPTIONS-* $res_dir/
    #\cp -f $bench_db_path/LOG $res_dir/
}
RUN_ALL_TEST() {
    for value in ${value_array[@]}; do
        CLEAN_CACHE
        bench_value="$value"
        bench_num="`expr $test_all_size / $bench_value`"

        RUN_ONE_TEST
        if [ $? -ne 0 ];then
            exit 1
        fi
        COPY_OUT_FILE
        sleep 5
    done
}

RUN_ALL_TEST
