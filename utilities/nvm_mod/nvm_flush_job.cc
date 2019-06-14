

#include "nvm_flush_job.h"

#include <inttypes.h>

#include <algorithm>
#include <vector>


#include "db/builder.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/event_helpers.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/memtable_list.h"
#include "db/merge_context.h"
#include "db/range_tombstone_fragmenter.h"
#include "db/version_set.h"
#include "monitoring/iostats_context_imp.h"
#include "monitoring/perf_context_imp.h"
#include "monitoring/thread_status_util.h"
#include "port/port.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/statistics.h"
#include "rocksdb/status.h"
#include "rocksdb/table.h"
#include "table/block.h"
#include "table/block_based_table_factory.h"
#include "table/merging_iterator.h"
#include "table/table_builder.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/event_logger.h"
#include "util/file_util.h"
#include "util/filename.h"
#include "util/log_buffer.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/stop_watch.h"
#include "util/sync_point.h"

namespace rocksdb {

const char* NvmGetFlushReasonString (FlushReason flush_reason) {
  switch (flush_reason) {
    case FlushReason::kOthers:
      return "Other Reasons";
    case FlushReason::kGetLiveFiles:
      return "Get Live Files";
    case FlushReason::kShutDown:
      return "Shut down";
    case FlushReason::kExternalFileIngestion:
      return "External File Ingestion";
    case FlushReason::kManualCompaction:
      return "Manual Compaction";
    case FlushReason::kWriteBufferManager:
      return "Write Buffer Manager";
    case FlushReason::kWriteBufferFull:
      return "Write Buffer Full";
    case FlushReason::kTest:
      return "Test";
    case FlushReason::kDeleteFiles:
      return "Delete Files";
    case FlushReason::kAutoCompaction:
      return "Auto Compaction";
    case FlushReason::kManualFlush:
      return "Manual Flush";
    case FlushReason::kErrorRecovery:
      return "Error Recovery";
    default:
      return "Invalid";
  }
}

NvmFlushJob::NvmFlushJob(const std::string& dbname, 
            ColumnFamilyData* cfd,
            const ImmutableDBOptions& db_options,
            const MutableCFOptions& mutable_cf_options,
            const uint64_t* max_memtable_id,
            const EnvOptions& env_options,
            VersionSet* versions, 
            InstrumentedMutex* db_mutex,
            std::atomic<bool>* shutting_down,
            std::vector<SequenceNumber> existing_snapshots,
            SequenceNumber earliest_write_conflict_snapshot,
            SnapshotChecker* snapshot_checker, 
            JobContext* job_context,
            LogBuffer* log_buffer, 
            Directory* db_directory,
            Directory* output_file_directory, 
            CompressionType output_compression,
            Statistics* stats, 
            EventLogger* event_logger, 
            bool measure_io_stats,
            const bool sync_output_directory, 
            const bool write_manifest)
    :dbname_(dbname),
    cfd_(cfd),
    db_options_(db_options),
    mutable_cf_options_(mutable_cf_options),
    max_memtable_id_(max_memtable_id),
    env_options_(env_options),
    versions_(versions),
    db_mutex_(db_mutex),
    shutting_down_(shutting_down),
    existing_snapshots_(std::move(existing_snapshots)),
    earliest_write_conflict_snapshot_(earliest_write_conflict_snapshot),
    snapshot_checker_(snapshot_checker),
    job_context_(job_context),
    log_buffer_(log_buffer),
    db_directory_(db_directory),
    output_file_directory_(output_file_directory),
    output_compression_(output_compression),
    stats_(stats),
    event_logger_(event_logger),
    measure_io_stats_(measure_io_stats),
    sync_output_directory_(sync_output_directory),
    write_manifest_(write_manifest),
    edit_(nullptr),
    base_(nullptr),
    pick_memtable_called(false) {

        nvm_cf_ = cfd_->nvmcfmodule;
        assert(nvm_cf_ != nullptr);
        ReportStartedFlush();



}
NvmFlushJob::~NvmFlushJob() {
  ThreadStatusUtil::ResetThreadStatus();
}

void NvmFlushJob::ReportStartedFlush() {
  ThreadStatusUtil::SetColumnFamily(cfd_, cfd_->ioptions()->env,
                                    db_options_.enable_thread_tracking);
  ThreadStatusUtil::SetThreadOperation(ThreadStatus::OP_FLUSH);
  ThreadStatusUtil::SetThreadOperationProperty(
      ThreadStatus::COMPACTION_JOB_ID,
      job_context_->job_id);
  IOSTATS_RESET(bytes_written);
}

void NvmFlushJob::ReportFlushInputSize(const autovector<MemTable*>& mems) {
  uint64_t input_size = 0;
  for (auto* mem : mems) {
    input_size += mem->ApproximateMemoryUsage();
  }
  ThreadStatusUtil::IncreaseThreadOperationProperty(
      ThreadStatus::FLUSH_BYTES_MEMTABLES,
      input_size);
}

void NvmFlushJob::RecordFlushIOStats() {
  RecordTick(stats_, FLUSH_WRITE_BYTES, IOSTATS(bytes_written));
  ThreadStatusUtil::IncreaseThreadOperationProperty(
      ThreadStatus::FLUSH_BYTES_WRITTEN, IOSTATS(bytes_written));
  IOSTATS_RESET(bytes_written);
}

void NvmFlushJob::PickMemTable() {
  db_mutex_->AssertHeld();
  assert(!pick_memtable_called);
  pick_memtable_called = true;
  // Save the contents of the earliest memtable as a new Table
  cfd_->imm()->PickMemtablesToFlush(max_memtable_id_, &mems_);
  if (mems_.empty()) {
    return;
  }

  ReportFlushInputSize(mems_);

  // entries mems are (implicitly) sorted in ascending order by their created
  // time. We will use the first memtable's `edit` to keep the meta info for
  // this flush.
  MemTable* m = mems_[0];
  edit_ = m->GetEdits();
  edit_->SetPrevLogNumber(0);
  // SetLogNumber(log_num) indicates logs with number smaller than log_num
  // will no longer be picked up for recovery.
  edit_->SetLogNumber(mems_.back()->GetNextLogNumber());
  edit_->SetColumnFamily(cfd_->GetID());

  // path 0 for level 0 file.
  meta_.fd = FileDescriptor(versions_->NewFileNumber(), 0, 0); //file_meta加入cf

  base_ = cfd_->current();
  base_->Ref();  // it is likely that we do not need this reference
}

Status NvmFlushJob::Run(LogsWithPrepTracker* prep_tracker,
                     FileMetaData* file_meta) { //file_meta加入cf
  //TEST_SYNC_POINT("FlushJob::Start");
  db_mutex_->AssertHeld();
  assert(pick_memtable_called);
  AutoThreadOperationStageUpdater stage_run(
      ThreadStatus::STAGE_FLUSH_RUN);
  if (mems_.empty()) {
    ROCKS_LOG_BUFFER(log_buffer_, "[%s] Nothing in memtable to nvm flush",
                     cfd_->GetName().c_str());
    return Status::OK();
  }

  // I/O measurement variables
  PerfLevel prev_perf_level = PerfLevel::kEnableTime;
  uint64_t prev_write_nanos = 0;
  uint64_t prev_fsync_nanos = 0;
  uint64_t prev_range_sync_nanos = 0;
  uint64_t prev_prepare_write_nanos = 0;
  if (measure_io_stats_) {
    prev_perf_level = GetPerfLevel();
    SetPerfLevel(PerfLevel::kEnableTime);
    prev_write_nanos = IOSTATS(write_nanos);
    prev_fsync_nanos = IOSTATS(fsync_nanos);
    prev_range_sync_nanos = IOSTATS(range_sync_nanos);
    prev_prepare_write_nanos = IOSTATS(prepare_write_nanos);
  }

  // This will release and re-acquire the mutex.
  Status s = WriteLevel0Table();

  if (s.ok() &&
      (shutting_down_->load(std::memory_order_acquire) || cfd_->IsDropped())) {
    s = Status::ShutdownInProgress(
        "Database shutdown or Column family drop during flush");
  }

  if (!s.ok()) {
    cfd_->imm()->RollbackMemtableFlush(mems_, meta_.fd.GetNumber());
  } else if (write_manifest_) {
    //TEST_SYNC_POINT("FlushJob::InstallResults");
    // Replace immutable memtable with the generated Table
    s = cfd_->imm()->TryInstallMemtableFlushResults(
        cfd_, mutable_cf_options_, mems_, prep_tracker, versions_, db_mutex_,
        meta_.fd.GetNumber(), &job_context_->memtables_to_free, db_directory_,
        log_buffer_);
  }

  if (s.ok() && file_meta != nullptr) {
    *file_meta = meta_;
  }
  RecordFlushIOStats();

  auto stream = event_logger_->LogToBuffer(log_buffer_);
  stream << "job" << job_context_->job_id << "event"
         << "nvm flush_finished";
  stream << "output_compression"
         << CompressionTypeToString(output_compression_);
  stream << "lsm_state";
  stream.StartArray();
  auto vstorage = cfd_->current()->storage_info();
  for (int level = 0; level < vstorage->num_levels(); ++level) {
    stream << vstorage->NumLevelFiles(level);
  }
  stream.EndArray();
  stream << "immutable_memtables" << cfd_->imm()->NumNotFlushed();

  if (measure_io_stats_) {
    if (prev_perf_level != PerfLevel::kEnableTime) {
      SetPerfLevel(prev_perf_level);
    }
    stream << "file_write_nanos" << (IOSTATS(write_nanos) - prev_write_nanos);
    stream << "file_range_sync_nanos"
           << (IOSTATS(range_sync_nanos) - prev_range_sync_nanos);
    stream << "file_fsync_nanos" << (IOSTATS(fsync_nanos) - prev_fsync_nanos);
    stream << "file_prepare_write_nanos"
           << (IOSTATS(prepare_write_nanos) - prev_prepare_write_nanos);
  }

  return s;
}

void NvmFlushJob::Cancel() {
  db_mutex_->AssertHeld();
  assert(base_ != nullptr);
  base_->Unref();
}

Status NvmFlushJob::WriteLevel0Table() {
  AutoThreadOperationStageUpdater stage_updater(
      ThreadStatus::STAGE_FLUSH_WRITE_L0);
  db_mutex_->AssertHeld();
  const uint64_t start_micros = db_options_.env->NowMicros();
  Status s;
  {
    //auto write_hint = cfd_->CalculateSSTWriteHint(0);
    db_mutex_->Unlock();
    if (log_buffer_) {
      log_buffer_->FlushBufferToLog();
    }
    // memtables and range_del_iters store internal iterators over each data
    // memtable and its associated range deletion memtable, respectively, at
    // corresponding indexes.
    std::vector<InternalIterator*> memtables;
    std::vector<std::unique_ptr<FragmentedRangeTombstoneIterator>>
        range_del_iters;
    ReadOptions ro;
    ro.total_order_seek = true;
    Arena arena;
    uint64_t total_num_entries = 0, total_num_deletes = 0;
    size_t total_memory_usage = 0;
    for (MemTable* m : mems_) {
      ROCKS_LOG_INFO(
          db_options_.info_log,
          "[%s] [JOB %d] nvm Flushing memtable with next log file: %" PRIu64 "\n",
          cfd_->GetName().c_str(), job_context_->job_id, m->GetNextLogNumber());
      memtables.push_back(m->NewIterator(ro, &arena));
      auto* range_del_iter =
          m->NewRangeTombstoneIterator(ro, kMaxSequenceNumber);
      if (range_del_iter != nullptr) {
        range_del_iters.emplace_back(range_del_iter);
      }
      total_num_entries += m->num_entries();
      total_num_deletes += m->num_deletes();
      total_memory_usage += m->ApproximateMemoryUsage();
    }

    event_logger_->Log()
        << "job" << job_context_->job_id << "event"
        << "nvm flush_started"
        << "num_memtables" << mems_.size() << "num_entries" << total_num_entries
        << "num_deletes" << total_num_deletes << "memory_usage"
        << total_memory_usage << "flush_reason"
        << NvmGetFlushReasonString(cfd_->GetFlushReason());

    {
      ScopedArenaIterator iter(
          NewMergingIterator(&cfd_->internal_comparator(), &memtables[0],
                             static_cast<int>(memtables.size()), &arena));
      ROCKS_LOG_INFO(db_options_.info_log,
                     "[%s] [JOB %d] Level-0 nvm flush table #%" PRIu64 ": started",
                     cfd_->GetName().c_str(), job_context_->job_id,
                     meta_.fd.GetNumber());

      /*TEST_SYNC_POINT_CALLBACK("FlushJob::WriteLevel0Table:output_compression",
                               &output_compression_);*/
      int64_t _current_time = 0;
      auto status = db_options_.env->GetCurrentTime(&_current_time);
      // Safe to proceed even if GetCurrentTime fails. So, log and proceed.
      if (!status.ok()) {
        ROCKS_LOG_WARN(
            db_options_.info_log,
            "Failed to get current time to populate creation_time property. "
            "Status: %s",
            status.ToString().c_str());
      }
      //const uint64_t current_time = static_cast<uint64_t>(_current_time);

     // uint64_t oldest_key_time =
          //mems_.front()->ApproximateOldestKeyTime();

      
        s=BuildInsertNvm(iter.get(),
                         std::move(range_del_iters),
                         cfd_->internal_comparator(),
                         existing_snapshots_,
                         earliest_write_conflict_snapshot_,
                         snapshot_checker_);
      /*s = BuildTable(
          dbname_, db_options_.env, *cfd_->ioptions(), mutable_cf_options_,
          env_options_, cfd_->table_cache(), iter.get(),
          std::move(range_del_iters), &meta_, cfd_->internal_comparator(),
          cfd_->int_tbl_prop_collector_factories(), cfd_->GetID(),
          cfd_->GetName(), existing_snapshots_,
          earliest_write_conflict_snapshot_, snapshot_checker_,
          output_compression_, cfd_->ioptions()->compression_opts,
          mutable_cf_options_.paranoid_file_checks, cfd_->internal_stats(),
          TableFileCreationReason::kFlush, event_logger_, job_context_->job_id,
          Env::IO_HIGH, &table_properties_, 0 level , current_time,
          oldest_key_time, write_hint);*/
      LogFlush(db_options_.info_log);
    }
    ROCKS_LOG_INFO(db_options_.info_log,
                   "[%s] [JOB %d] Level-0 nvm flush table #%" PRIu64 ": %" PRIu64
                   " bytes %s"
                   "%s",
                   cfd_->GetName().c_str(), job_context_->job_id,
                   meta_.fd.GetNumber(), meta_.fd.GetFileSize(),
                   s.ToString().c_str(),
                   meta_.marked_for_compaction ? " (needs compaction)" : "");

    if (s.ok() && output_file_directory_ != nullptr && sync_output_directory_) {
      s = output_file_directory_->Fsync();
    }
    TEST_SYNC_POINT("FlushJob::WriteLevel0Table");
    db_mutex_->Lock();
  }
  base_->Unref();

  // Note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  if (s.ok() && meta_.fd.GetFileSize() > 0) {
    // if we have more than 1 background thread, then we cannot
    // insert files directly into higher levels because some other
    // threads could be concurrently producing compacted files for
    // that key range.
    // Add file to L0

    edit_->AddFile(0 /* level */, meta_.fd.GetNumber(), meta_.fd.GetPathId(),
                   meta_.fd.GetFileSize(), meta_.smallest, meta_.largest,
                   meta_.fd.smallest_seqno, meta_.fd.largest_seqno,
                   meta_.marked_for_compaction, meta_.is_level0, meta_.first_key_index);
  }

  // Note that here we treat flush as level 0 compaction in internal stats
  InternalStats::CompactionStats stats(CompactionReason::kFlush, 1);
  stats.micros = db_options_.env->NowMicros() - start_micros;
  stats.bytes_written = meta_.fd.GetFileSize();
  MeasureTime(stats_, FLUSH_TIME, stats.micros);
  cfd_->internal_stats()->AddCompactionStats(0 /* level */, stats);
  cfd_->internal_stats()->AddCFStats(InternalStats::BYTES_FLUSHED,
                                     meta_.fd.GetFileSize());
  RecordFlushIOStats();
  return s;
}

Status NvmFlushJob::BuildInsertNvm(InternalIterator *iter,
                        std::vector<std::unique_ptr<FragmentedRangeTombstoneIterator>> range_del_iters,
                        const InternalKeyComparator &internal_comparator,
                        std::vector<SequenceNumber> snapshots,
                        SequenceNumber earliest_write_conflict_snapshot,
                        SnapshotChecker *snapshot_checker){
    //const size_t kReportFlushIOStatsEvery = 1048576;
  Env* env=db_options_.env;
  Status s;
  meta_.fd.file_size = 0;
  iter->SeekToFirst();
  std::unique_ptr<CompactionRangeDelAggregator> range_del_agg(
      new CompactionRangeDelAggregator(&internal_comparator, snapshots));
  for (auto& range_del_iter : range_del_iters) {
    range_del_agg->AddTombstones(std::move(range_del_iter));
  }

  /*std::string fname = TableFileName(ioptions.cf_paths, meta->fd.GetNumber(),
                                    meta->fd.GetPathId());*/
/*#ifndef ROCKSDB_LITE
  EventHelpers::NotifyTableFileCreationStarted(
      ioptions.listeners, dbname, column_family_name, fname, job_id, reason);
#endif  // !ROCKSDB_LITE*/
 // TableProperties tp;

  if (iter->Valid() || !range_del_agg->IsEmpty()) {
    /*TableBuilder* builder;
    std::unique_ptr<WritableFileWriter> file_writer;
    {
      std::unique_ptr<WritableFile> file;
#ifndef NDEBUG
      bool use_direct_writes = env_options.use_direct_writes;
      TEST_SYNC_POINT_CALLBACK("BuildTable:create_file", &use_direct_writes);
#endif  // !NDEBUG*/
      /*s = NewWritableFile(env, fname, &file, env_options);
      if (!s.ok()) {
        EventHelpers::LogAndNotifyTableFileCreationFinished(
            event_logger, ioptions.listeners, dbname, column_family_name, fname,
            job_id, meta->fd, tp, reason, s);
        return s;
      }
      file->SetIOPriority(io_priority);
      file->SetWriteLifeTimeHint(write_hint);

      file_writer.reset(new WritableFileWriter(std::move(file), fname,
                                               env_options, ioptions.statistics,
                                               ioptions.listeners));*/
      /*builder = NewTableBuilder(
          ioptions, mutable_cf_options, internal_comparator,
          int_tbl_prop_collector_factories, column_family_id,
          column_family_name, file_writer.get(), compression, compression_opts,
          level, nullptr * compression_dict *, false * skip_filters ,
          //creation_time, oldest_key_time);
    //}*/
    FileEntry* file = nullptr;
    char* raw = nullptr;
    nvm_cf_->AddL0TableRoom(meta_.fd.GetNumber(),&raw,&file);
    L0TableBuilder* L0builder = new L0TableBuilder(nvm_cf_,file,raw);

    MergeHelper merge(env, internal_comparator.user_comparator(),
                      cfd_->ioptions()->merge_operator, nullptr, cfd_->ioptions()->info_log,
                      true /* internal key corruption is not ok */,
                      snapshots.empty() ? 0 : snapshots.back(),
                      snapshot_checker);

    CompactionIterator c_iter(
        iter, internal_comparator.user_comparator(), &merge, kMaxSequenceNumber,
        &snapshots, earliest_write_conflict_snapshot, snapshot_checker, env,
        ShouldReportDetailedTime(env, cfd_->ioptions()->statistics),
        true /* internal key corruption is not ok */, range_del_agg.get());
    c_iter.SeekToFirst();
    uint64_t number_entry = 0;
    for (; c_iter.Valid(); c_iter.Next()) {
      const Slice& key = c_iter.key();
      const Slice& value = c_iter.value();
      //builder->Add(key, value);
      L0builder->Add(key,value);
      
      number_entry++;
      meta_.UpdateBoundaries(key, c_iter.ikey().sequence);

      // TODO(noetzli): Update stats after flush, too.
      /*if (io_priority == Env::IO_HIGH &&
          IOSTATS(bytes_written) >= kReportFlushIOStatsEvery) {
        ThreadStatusUtil::SetThreadOperationProperty(
            ThreadStatus::FLUSH_BYTES_WRITTEN, IOSTATS(bytes_written));
      }*/
    }

    auto range_del_it = range_del_agg->NewIterator();
    for (range_del_it->SeekToFirst(); range_del_it->Valid();
         range_del_it->Next()) {
      auto tombstone = range_del_it->Tombstone();
      auto kv = tombstone.Serialize();

      ROCKS_LOG_INFO(db_options_.info_log,
                     "[%s] [JOB %d] Level-0 nvm flush table #%" PRIu64 ": have range_del_agg ",
                     cfd_->GetName().c_str(), job_context_->job_id,
                     meta_.fd.GetNumber());

      //builder->Add(kv.first.Encode(), kv.second);
      meta_.UpdateBoundariesForRange(kv.first, tombstone.SerializeEndKey(),
                                     tombstone.seq_, internal_comparator);
    }

    bool empty = number_entry == 0;
    // Finish and check for builder errors
    /*tp = builder->GetTableProperties();
    bool empty = builder->NumEntries() == 0 && tp.num_range_deletions == 0;
    s = c_iter.status();
    if (!s.ok() || empty) {
      builder->Abandon();
    } else {
      s = builder->Finish();
    }*/
    if (!s.ok() || empty) {
      printf("error:abandon L0builder\n");
    } else {
      s = L0builder->Finish();
    }

    if (s.ok() && !empty) {
      uint64_t file_size = L0builder->GetFileSize();
      meta_.fd.file_size = file_size;
      meta_.is_level0 = true;
      meta_.first_key_index = 0;
      /*uint64_t file_size = builder->FileSize();
      meta_->fd.file_size = file_size;
      meta_->marked_for_compaction = builder->NeedCompact();
      assert(meta_->fd.GetFileSize() > 0);
      tp = builder->GetTableProperties(); // refresh now that builder is finished
      if (table_properties) {
        *table_properties = tp;
      }*/
    }
    //delete builder;

    // Finish and check for file errors
    if (s.ok() && !empty) {
      StopWatch sw(env, cfd_->ioptions()->statistics, TABLE_SYNC_MICROS);
      //s = file_writer->Sync(cfd_->ioptions()->use_fsync);
    }
    /*if (s.ok() && !empty) {
      s = file_writer->Close();
    }*/

    /*if (s.ok() && !empty) {
      // Verify that the table is usable
      // We set for_compaction to false and don't OptimizeForCompactionTableRead
      // here because this is a special case after we finish the table building
      // No matter whether use_direct_io_for_flush_and_compaction is true,
      // we will regrad this verification as user reads since the goal is
      // to cache it here for further user reads
      std::unique_ptr<InternalIterator> it(table_cache->NewIterator(
          ReadOptions(), env_options, internal_comparator, *meta,
          nullptr * range_del_agg ,
          mutable_cf_options.prefix_extractor.get(), nullptr,
          (internal_stats == nullptr) ? nullptr
                                      : internal_stats->GetFileReadHist(0),
          false * for_compaction *, nullptr * arena *,
          false * skip_filter *, level));
      s = it->status();
      if (s.ok() && paranoid_file_checks) {
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
        }
        s = it->status();
      }
    }*/
  }

  // Check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  /*if (!s.ok() || meta_->fd.GetFileSize() == 0) {
    env->DeleteFile(fname);
  }*/

  // Output to event logger and fire events.
  /*EventHelpers::LogAndNotifyTableFileCreationFinished(
      event_logger, ioptions.listeners, dbname, column_family_name, fname,
      job_id, meta_->fd, tp, reason, s);*/

  return s;
}





}