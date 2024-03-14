#!/usr/bin/env python3

# Auto-generate statistics #defines, with initialization, clear and aggregate
# functions.
#
# NOTE: Statistics reports show individual objects as operations per second.
# All objects where that does not make sense should have the word 'currently'
# or the phrase 'in the cache' in their text description, for example, 'files
# currently open'.
# NOTE: All statistics descriptions must have a prefix string followed by ':'.
#
# Data-source statistics are normally aggregated across the set of underlying
# objects. Additional optional configuration flags are available:
#       cache_walk      Only reported when statistics=cache_walk is set
#       tree_walk       Only reported when statistics=tree_walk is set
#       max_aggregate   Take the maximum value when aggregating statistics
#       no_clear        Value not cleared when statistics cleared
#       no_scale        Don't scale value per second in the logging tool script
#       size            Used by timeseries tool, indicates value is a byte count
#       user_facing     Only report stats that impact mongo users
#
# The no_clear and no_scale flags are normally always set together (values that
# are maintained over time are normally not scaled per second).

from operator import attrgetter
import sys

class Stat:
    def __init__(self, name, tag, desc, user_facing: bool, flags=''):
        self.name = name
        self.desc = tag + ': ' + desc
        self.user_facing = user_facing
        self.flags = flags

    def __cmp__(self, other):
        return cmp(self.desc.lower(), other.desc.lower())

class AutoCommitStat(Stat):
    prefix = 'autocommit'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, AutoCommitStat.prefix, desc, user_facing, flags)

class BackgroundCompactStat(Stat):
    prefix = 'background-compact'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, BackgroundCompactStat.prefix, desc, user_facing, flags)

class BlockCacheStat(Stat):
    prefix = 'block-cache'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, BlockCacheStat.prefix, desc, user_facing, flags)
class BlockStat(Stat):
    prefix = 'block-manager'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, BlockStat.prefix, desc, user_facing, flags)
class BtreeStat(Stat):
    prefix = 'btree'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, BtreeStat.prefix, desc, user_facing, flags)
class CacheStat(Stat):
    prefix = 'cache'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, CacheStat.prefix, desc, user_facing, flags)
class CacheWalkStat(Stat):
    prefix = 'cache_walk'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        flags += ',cache_walk'
        Stat.__init__(self, name, CacheWalkStat.prefix, desc, user_facing, flags)
class CapacityStat(Stat):
    prefix = 'capacity'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, CapacityStat.prefix, desc, user_facing, flags)
class CheckpointStat(Stat):
    prefix = 'checkpoint'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, CheckpointStat.prefix, desc, user_facing, flags)
class ChunkCacheStat(Stat):
    prefix = 'chunk-cache'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, ChunkCacheStat.prefix, desc, user_facing, flags)
class CompressStat(Stat):
    prefix = 'compression'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, CompressStat.prefix, desc, user_facing, flags)
class ConnStat(Stat):
    prefix = 'connection'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, ConnStat.prefix, desc, user_facing, flags)
class CursorStat(Stat):
    prefix = 'cursor'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, CursorStat.prefix, desc, user_facing, flags)
class DhandleStat(Stat):
    prefix = 'data-handle'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, DhandleStat.prefix, desc, user_facing, flags)
class JoinStat(Stat):
    prefix = 'join'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, JoinStat.prefix, desc, user_facing, flags)
class LockStat(Stat):
    prefix = 'lock'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, LockStat.prefix, desc, user_facing, flags)
class LogStat(Stat):
    prefix = 'log'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, LogStat.prefix, desc, user_facing, flags)
class LSMStat(Stat):
    prefix = 'LSM'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, LSMStat.prefix, desc, user_facing, flags)
class SessionStat(Stat):
    prefix = 'session'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, SessionStat.prefix, desc, user_facing, flags)
class PerfHistStat(Stat):
    prefix = 'perf'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, PerfHistStat.prefix, desc, user_facing, flags)
class PrefetchStat(Stat):
    prefix = 'prefetch'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, PrefetchStat.prefix, desc, user_facing, flags)
class RecStat(Stat):
    prefix = 'reconciliation'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, RecStat.prefix, desc, user_facing, flags)
class SessionOpStat(Stat):
    prefix = 'session'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, SessionOpStat.prefix, desc, user_facing, flags)
class StorageStat(Stat):
    prefix = 'tiered-storage'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, SessionOpStat.prefix, desc, user_facing, flags)
class ThreadStat(Stat):
    prefix = 'thread-state'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, ThreadStat.prefix, desc, user_facing, flags)
class TxnStat(Stat):
    prefix = 'transaction'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, TxnStat.prefix, desc, user_facing, flags)
class YieldStat(Stat):
    prefix = 'thread-yield'
    def __init__(self, name, desc, user_facing: bool, flags=''):
        Stat.__init__(self, name, YieldStat.prefix, desc, user_facing, flags)

##########################################
# Groupings of useful statistics:
# A pre-defined dictionary containing the group name as the key and the
# list of prefix tags that comprise that group.
##########################################
groups = {}
groups['cursor'] = [CursorStat.prefix, SessionOpStat.prefix]
groups['evict'] = [
    BlockCacheStat.prefix,
    BlockStat.prefix,
    CacheStat.prefix,
    CacheWalkStat.prefix,
    ChunkCacheStat.prefix,
    ConnStat.prefix,
    ThreadStat.prefix
]
groups['lsm'] = [LSMStat.prefix, TxnStat.prefix]
groups['memory'] = [
    CacheStat.prefix,
    CacheWalkStat.prefix,
    ConnStat.prefix,
    RecStat.prefix]
groups['system'] = [
    CapacityStat.prefix,
    ConnStat.prefix,
    DhandleStat.prefix,
    PerfHistStat.prefix,
    SessionOpStat.prefix,
    ThreadStat.prefix
]

##########################################
# CONNECTION statistics
##########################################
conn_stats = [
    ##########################################
    # System statistics
    ##########################################
    ConnStat('backup_blocks', 'total modified incremental blocks', False),
    ConnStat('backup_cursor_open', 'backup cursor open', False, 'no_clear,no_scale'),
    ConnStat('backup_dup_open', 'backup duplicate cursor open', False, 'no_clear,no_scale'),
    ConnStat('backup_incremental', 'incremental backup enabled', False, 'no_clear,no_scale'),
    ConnStat('backup_start', 'opening the backup cursor in progress', False, 'no_clear,no_scale'),
    ConnStat('buckets', 'hash bucket array size general', False, 'no_clear,no_scale,size'),
    ConnStat('buckets_dh', 'hash bucket array size for data handles', False, 'no_clear,no_scale,size'),
    ConnStat('cond_auto_wait', 'auto adjusting condition wait calls', False),
    ConnStat('cond_auto_wait_reset', 'auto adjusting condition resets', False),
    ConnStat('cond_auto_wait_skipped', 'auto adjusting condition wait raced to update timeout and skipped updating', False),
    ConnStat('cond_wait', 'pthread mutex condition wait calls', False),
    ConnStat('file_open', 'files currently open', False, 'no_clear,no_scale'),
    ConnStat('fsync_io', 'total fsync I/Os', False),
    ConnStat('memory_allocation', 'memory allocations', False),
    ConnStat('memory_free', 'memory frees', False),
    ConnStat('memory_grow', 'memory re-allocations', False),
    ConnStat('no_session_sweep_5min', 'number of sessions without a sweep for 5+ minutes', False),
    ConnStat('no_session_sweep_60min', 'number of sessions without a sweep for 60+ minutes', False),
    ConnStat('read_io', 'total read I/Os', False),
    ConnStat('rwlock_read', 'pthread mutex shared lock read-lock calls', False),
    ConnStat('rwlock_write', 'pthread mutex shared lock write-lock calls', False),
    ConnStat('time_travel', 'detected system time went backwards', False),
    ConnStat('write_io', 'total write I/Os', False),

    ##########################################
    # Background compaction statistics
    ##########################################
    BackgroundCompactStat('background_compact_bytes_recovered', 'background compact recovered bytes', False, 'no_scale'),
    BackgroundCompactStat('background_compact_ema', 'background compact moving average of bytes rewritten', False, 'no_scale'),
    BackgroundCompactStat('background_compact_exclude', 'background compact skipped file as it is part of the exclude list', False, 'no_scale'),
    BackgroundCompactStat('background_compact_fail', 'background compact failed calls', False, 'no_scale'),
    BackgroundCompactStat('background_compact_fail_cache_pressure', 'background compact failed calls due to cache pressure', False, 'no_scale'),
    BackgroundCompactStat('background_compact_files_tracked', 'number of files tracked by background compaction', False, 'no_scale'),
    BackgroundCompactStat('background_compact_interrupted', 'background compact interrupted', False, 'no_scale'),
    BackgroundCompactStat('background_compact_running', 'background compact running', False, 'no_scale'),
    BackgroundCompactStat('background_compact_skipped', 'background compact skipped file as not meeting requirements for compaction', False, 'no_scale'),
    BackgroundCompactStat('background_compact_success', 'background compact successful calls', False, 'no_scale'),
    BackgroundCompactStat('background_compact_timeout', 'background compact timeout', False, 'no_scale'),

    ##########################################
    # Block cache statistics
    ##########################################
    BlockCacheStat('block_cache_blocks', 'total blocks', False),
    BlockCacheStat('block_cache_blocks_evicted', 'evicted blocks', False),
    BlockCacheStat('block_cache_blocks_insert_read', 'total blocks inserted on read path', False),
    BlockCacheStat('block_cache_blocks_insert_write', 'total blocks inserted on write path', False),
    BlockCacheStat('block_cache_blocks_removed', 'removed blocks', False),
    BlockCacheStat('block_cache_blocks_removed_blocked', 'time sleeping to remove block (usecs)', False),
    BlockCacheStat('block_cache_blocks_update', 'cached blocks updated', False),
    BlockCacheStat('block_cache_bypass_chkpt', 'number of put bypasses on checkpoint I/O', False),
    BlockCacheStat('block_cache_bypass_filesize', 'file size causing bypass', False),
    BlockCacheStat('block_cache_bypass_get', 'number of bypasses on get', False),
    BlockCacheStat('block_cache_bypass_overhead_put', 'number of bypasses due to overhead on put', False),
    BlockCacheStat('block_cache_bypass_put', 'number of bypasses on put because file is too small', False),
    BlockCacheStat('block_cache_bypass_writealloc', 'number of bypasses because no-write-allocate setting was on', False),
    BlockCacheStat('block_cache_bytes', 'total bytes', False),
    BlockCacheStat('block_cache_bytes_insert_read', 'total bytes inserted on read path', False),
    BlockCacheStat('block_cache_bytes_insert_write', 'total bytes inserted on write path', False),
    BlockCacheStat('block_cache_bytes_update', 'cached bytes updated', False),
    BlockCacheStat('block_cache_eviction_passes', 'number of eviction passes', False),
    BlockCacheStat('block_cache_hits', 'number of hits', False),
    BlockCacheStat('block_cache_lookups', 'lookups', False),
    BlockCacheStat('block_cache_misses', 'number of misses', False),
    BlockCacheStat('block_cache_not_evicted_overhead', 'number of blocks not evicted due to overhead', False),

    ##########################################
    # Block manager statistics
    ##########################################
    BlockStat('block_byte_map_read', 'mapped bytes read', False, 'size'),
    BlockStat('block_byte_read', 'bytes read', False, 'size'),
    BlockStat('block_byte_read_mmap', 'bytes read via memory map API', False, 'size'),
    BlockStat('block_byte_read_syscall', 'bytes read via system call API', False, 'size'),
    BlockStat('block_byte_write', 'bytes written', False, 'size'),
    BlockStat('block_byte_write_checkpoint', 'bytes written for checkpoint', False, 'size'),
    BlockStat('block_byte_write_compact', 'bytes written by compaction', False, 'size'),
    BlockStat('block_byte_write_mmap', 'bytes written via memory map API', False, 'size'),
    BlockStat('block_byte_write_syscall', 'bytes written via system call API', False, 'size'),
    BlockStat('block_map_read', 'mapped blocks read', False),
    BlockStat('block_preload', 'blocks pre-loaded', False),
    BlockStat('block_read', 'blocks read', False),
    BlockStat('block_remap_file_resize', 'number of times the file was remapped because it changed size via fallocate or truncate', False),
    BlockStat('block_remap_file_write', 'number of times the region was remapped via write', False),
    BlockStat('block_write', 'blocks written', False),

    ##########################################
    # Cache and eviction statistics
    ##########################################
    CacheStat('cache_bytes_hs', 'bytes belonging to the history store table in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_image', 'bytes belonging to page images in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_internal', 'tracked bytes belonging to internal pages in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_leaf', 'tracked bytes belonging to leaf pages in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_max', 'maximum bytes configured', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_other', 'bytes not belonging to page images in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_updates', 'bytes allocated for updates', False, 'no_clear,no_scale,size'),
    CacheStat('cache_eviction_active_workers', 'eviction worker thread active', False, 'no_clear'),
    CacheStat('cache_eviction_aggressive_set', 'eviction currently operating in aggressive mode', False, 'no_clear,no_scale'),
    CacheStat('cache_eviction_app', 'pages evicted by application threads', False),
    CacheStat('cache_eviction_app_dirty', 'modified pages evicted by application threads', False),
    CacheStat('cache_eviction_clear_ordinary', 'pages removed from the ordinary queue to be queued for urgent eviction', False),
    CacheStat('cache_eviction_consider_prefetch', 'pages considered for eviction that were brought in by pre-fetch', False, 'no_clear,no_scale'),
    CacheStat('cache_eviction_empty_score', 'eviction empty score', False, 'no_clear,no_scale'),
    CacheStat('cache_eviction_fail', 'pages selected for eviction unable to be evicted', False),
    CacheStat('cache_eviction_fail_active_children_on_an_internal_page', 'pages selected for eviction unable to be evicted because of active children on an internal page', False),
    CacheStat('cache_eviction_fail_checkpoint_no_ts', 'pages selected for eviction unable to be evicted because of race between checkpoint and updates without timestamps', False),
    CacheStat('cache_eviction_fail_in_reconciliation', 'pages selected for eviction unable to be evicted because of failure in reconciliation', False),
    CacheStat('cache_eviction_force', 'forced eviction - pages selected count', False),
    CacheStat('cache_eviction_force_clean', 'forced eviction - pages evicted that were clean count', False),
    CacheStat('cache_eviction_force_clean_time', 'forced eviction - pages evicted that were clean time (usecs)', False),
    CacheStat('cache_eviction_force_delete', 'forced eviction - pages selected because of too many deleted items count', False),
    CacheStat('cache_eviction_force_dirty', 'forced eviction - pages evicted that were dirty count', False),
    CacheStat('cache_eviction_force_dirty_time', 'forced eviction - pages evicted that were dirty time (usecs)', False),
    CacheStat('cache_eviction_force_fail', 'forced eviction - pages selected unable to be evicted count', False),
    CacheStat('cache_eviction_force_fail_time', 'forced eviction - pages selected unable to be evicted time', False),
    CacheStat('cache_eviction_force_no_retry', 'forced eviction - do not retry count to evict pages selected to evict during reconciliation', False),
    CacheStat('cache_eviction_force_hs', 'forced eviction - history store pages selected while session has history store cursor open', False),
    CacheStat('cache_eviction_force_hs_fail', 'forced eviction - history store pages failed to evict while session has history store cursor open', False),
    CacheStat('cache_eviction_force_hs_success', 'forced eviction - history store pages successfully evicted while session has history store cursor open', False),
    CacheStat('cache_eviction_force_long_update_list', 'forced eviction - pages selected because of a large number of updates to a single item', False),
    CacheStat('cache_eviction_force_retune', 'force re-tuning of eviction workers once in a while', False),
    CacheStat('cache_eviction_get_ref', 'eviction calls to get a page', False),
    CacheStat('cache_eviction_get_ref_empty', 'eviction calls to get a page found queue empty', False),
    CacheStat('cache_eviction_get_ref_empty2', 'eviction calls to get a page found queue empty after locking', False),
    CacheStat('cache_eviction_internal_pages_already_queued', 'internal pages seen by eviction walk that are already queued', False),
    CacheStat('cache_eviction_internal_pages_queued', 'internal pages queued for eviction', False),
    CacheStat('cache_eviction_internal_pages_seen', 'internal pages seen by eviction walk', False),
    CacheStat('cache_eviction_maximum_page_size', 'maximum page size seen at eviction', False, 'no_clear,no_scale,size'),
    CacheStat('cache_eviction_maximum_milliseconds', 'maximum milliseconds spent at a single eviction', False, 'no_clear,no_scale,size'),
    CacheStat('cache_eviction_pages_already_queued', 'pages seen by eviction walk that are already queued', False),
    CacheStat('cache_eviction_pages_in_parallel_with_checkpoint', 'pages evicted in parallel with checkpoint', False),
    CacheStat('cache_eviction_pages_queued', 'pages queued for eviction', False),
    CacheStat('cache_eviction_pages_queued_oldest', 'pages queued for urgent eviction during walk', False),
    CacheStat('cache_eviction_pages_queued_post_lru', 'pages queued for eviction post lru sorting', False),
    CacheStat('cache_eviction_pages_queued_urgent', 'pages queued for urgent eviction', False),
    CacheStat('cache_eviction_pages_queued_urgent_hs_dirty', 'pages queued for urgent eviction from history store due to high dirty content', False),
    CacheStat('cache_eviction_queue_empty', 'eviction server candidate queue empty when topping up', False),
    CacheStat('cache_eviction_queue_not_empty', 'eviction server candidate queue not empty when topping up', False),
    CacheStat('cache_eviction_server_evicting', 'eviction server evicting pages', False),
    CacheStat('cache_eviction_server_skip_checkpointing_trees', 'eviction server skips trees that are being checkpointed', False),
    CacheStat('cache_eviction_server_skip_dirty_pages_during_checkpoint', 'eviction server skips dirty pages during a running checkpoint', False),
    CacheStat('cache_eviction_server_skip_pages_retry', 'eviction server skips pages that previously failed eviction and likely will again', False),
    CacheStat('cache_eviction_server_skip_pages_last_running', 'eviction server skips pages that are written with transactions greater than the last running', False),
    CacheStat('cache_eviction_server_skip_metatdata_with_history', 'eviction server skips metadata pages with history', False),
    CacheStat('cache_eviction_server_skip_trees_eviction_disabled', 'eviction server skips trees that disable eviction', False),
    CacheStat('cache_eviction_server_skip_trees_not_useful_before', 'eviction server skips trees that were not useful before', False),
    CacheStat('cache_eviction_server_skip_trees_stick_in_cache', 'eviction server skips trees that are configured to stick in cache', False),
    CacheStat('cache_eviction_server_skip_trees_too_many_active_walks', 'eviction server skips trees because there are too many active walks', False),
    CacheStat('cache_eviction_server_skip_unwanted_pages', 'eviction server skips pages that we do not want to evict', False),
    CacheStat('cache_eviction_server_slept', 'eviction server slept, because we did not make progress with eviction', False),
    CacheStat('cache_eviction_slow', 'eviction server unable to reach eviction goal', False),
    CacheStat('cache_eviction_stable_state_workers', 'eviction worker thread stable number', False, 'no_clear'),
    CacheStat('cache_eviction_state', 'eviction state', False, 'no_clear,no_scale'),
    CacheStat('cache_eviction_target_strategy_both_clean_and_dirty', 'eviction walk target strategy both clean and dirty pages', False),
    CacheStat('cache_eviction_target_strategy_clean', 'eviction walk target strategy only clean pages', False),
    CacheStat('cache_eviction_target_strategy_dirty', 'eviction walk target strategy only dirty pages', False),
    CacheStat('cache_eviction_walk', 'pages walked for eviction', False),
    CacheStat('cache_eviction_walk_leaf_notfound', 'eviction server waiting for a leaf page', False),
    CacheStat('cache_eviction_walk_passes', 'eviction passes of a file', False),
    CacheStat('cache_eviction_walk_sleeps', 'eviction walk most recent sleeps for checkpoint handle gathering', False),
    CacheStat('cache_eviction_walks_active', 'files with active eviction walks', False, 'no_clear,no_scale'),
    CacheStat('cache_eviction_walks_started', 'files with new eviction walks started', False),
    CacheStat('cache_eviction_worker_created', 'eviction worker thread created', False),
    CacheStat('cache_eviction_worker_evicting', 'eviction worker thread evicting pages', False),
    CacheStat('cache_eviction_worker_removed', 'eviction worker thread removed', False),
    CacheStat('cache_hazard_checks', 'hazard pointer check calls', False),
    CacheStat('cache_hazard_max', 'hazard pointer maximum array length', False, 'max_aggregate,no_scale'),
    CacheStat('cache_hazard_walks', 'hazard pointer check entries walked', False),
    CacheStat('cache_hs_ondisk', 'history store table on-disk size', False, 'no_clear,no_scale,size'),
    CacheStat('cache_hs_ondisk_max', 'history store table max on-disk size', False, 'no_clear,no_scale,size'),
    CacheStat('cache_reentry_hs_eviction_milliseconds', 'total milliseconds spent inside reentrant history store evictions in a reconciliation', False, 'no_clear,no_scale,size'),
    CacheStat('cache_overhead', 'percentage overhead', False, 'no_clear,no_scale'),
    CacheStat('cache_pages_dirty', 'tracked dirty pages in the cache', False, 'no_clear,no_scale'),
    CacheStat('cache_pages_inuse', 'pages currently held in the cache', False, 'no_clear,no_scale'),
    CacheStat('cache_read_app_count', 'application threads page read from disk to cache count', False),
    CacheStat('cache_read_app_time', 'application threads page read from disk to cache time (usecs)', False),
    CacheStat('cache_timed_out_ops', 'operations timed out waiting for space in cache', False),
    CacheStat('cache_write_app_count', 'application threads page write from cache to disk count', False),
    CacheStat('cache_write_app_time', 'application threads page write from cache to disk time (usecs)', False),

    ##########################################
    # Capacity statistics
    ##########################################
    CapacityStat('capacity_bytes_chunkcache', 'bytes written for chunk cache', False),
    CapacityStat('capacity_bytes_ckpt', 'bytes written for checkpoint', False),
    CapacityStat('capacity_bytes_evict', 'bytes written for eviction', False),
    CapacityStat('capacity_bytes_log', 'bytes written for log', False),
    CapacityStat('capacity_bytes_read', 'bytes read', False),
    CapacityStat('capacity_bytes_written', 'bytes written total', False),
    CapacityStat('capacity_threshold', 'threshold to call fsync', False),
    CapacityStat('capacity_time_chunkcache', 'time waiting for chunk cache IO bandwidth (usecs)', False),
    CapacityStat('capacity_time_ckpt', 'time waiting during checkpoint (usecs)', False),
    CapacityStat('capacity_time_evict', 'time waiting during eviction (usecs)', False),
    CapacityStat('capacity_time_log', 'time waiting during logging (usecs)', False),
    CapacityStat('capacity_time_read', 'time waiting during read (usecs)', False),
    CapacityStat('capacity_time_total', 'time waiting due to total capacity (usecs)', False),
    CapacityStat('fsync_all_fh', 'background fsync file handles synced', False),
    CapacityStat('fsync_all_fh_total', 'background fsync file handles considered', False),
    CapacityStat('fsync_all_time', 'background fsync time (msecs)', False, 'no_clear,no_scale'),

    ##########################################
    # Checkpoint statistics
    ##########################################
    CheckpointStat('checkpoint_fsync_post', 'fsync calls after allocating the transaction ID', False),
    CheckpointStat('checkpoint_fsync_post_duration', 'fsync duration after allocating the transaction ID (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_generation', 'generation', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_applied', 'most recent handles applied', False),
    CheckpointStat('checkpoint_handle_apply_duration', 'most recent duration for gathering applied handles (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_dropped', 'most recent handles checkpoint dropped', False),
    CheckpointStat('checkpoint_handle_drop_duration', 'most recent duration for checkpoint dropping all handles (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_duration', 'most recent duration for gathering all handles (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_locked', 'most recent handles metadata locked', False),
    CheckpointStat('checkpoint_handle_lock_duration', 'most recent duration for locking the handles (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_meta_checked', 'most recent handles metadata checked', False),
    CheckpointStat('checkpoint_handle_meta_check_duration', 'most recent duration for handles metadata checked (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_skipped', 'most recent handles skipped', False),
    CheckpointStat('checkpoint_handle_skip_duration', 'most recent duration for gathering skipped handles (usecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_handle_walked', 'most recent handles walked', False),
    CheckpointStat('checkpoint_hs_pages_reconciled', 'number of history store pages caused to be reconciled', False),
    CheckpointStat('checkpoint_pages_reconciled', 'number of pages caused to be reconciled', False),
    CheckpointStat('checkpoint_pages_visited_internal', 'number of internal pages visited', False),
    CheckpointStat('checkpoint_pages_visited_leaf', 'number of leaf pages visited', False),
    CheckpointStat('checkpoint_prep_max', 'prepare max time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_prep_min', 'prepare min time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_prep_recent', 'prepare most recent time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_prep_running', 'prepare currently running', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_prep_total', 'prepare total time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_presync', 'number of handles visited after writes complete', False),
    CheckpointStat('checkpoint_scrub_max', 'scrub max time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_scrub_min', 'scrub min time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_scrub_recent', 'scrub most recent time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_scrub_target', 'scrub dirty target', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_scrub_total', 'scrub total time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_skipped', 'checkpoints skipped because database was clean', False),
    CheckpointStat('checkpoint_state', 'progress state', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_stop_stress_active', 'stop timing stress active', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_sync', 'number of files synced', False),
    CheckpointStat('checkpoint_time_max', 'max time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_time_min', 'min time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_time_recent', 'most recent time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_time_total', 'total time (msecs)', False, 'no_clear,no_scale'),
    CheckpointStat('checkpoint_tree_duration', 'time spent on per-tree checkpoint work (usecs)', False),
    CheckpointStat('checkpoint_wait_reduce_dirty', 'wait cycles while cache dirty level is decreasing', False),
    CheckpointStat('checkpoints_api', 'number of checkpoints started by api', False),
    CheckpointStat('checkpoints_compact', 'number of checkpoints started by compaction', False),
    CheckpointStat('checkpoints_total_failed', 'total failed number of checkpoints', False),
    CheckpointStat('checkpoints_total_succeed', 'total succeed number of checkpoints', False),

    ##########################################
    # Chunk cache statistics
    ##########################################
    ChunkCacheStat('chunkcache_bytes_inuse', 'total bytes used by the cache', False),
    ChunkCacheStat('chunkcache_bytes_inuse_pinned', 'total bytes used by the cache for pinned chunks', False),
    ChunkCacheStat('chunkcache_bytes_read_persistent', 'total bytes read from persistent content', False),
    ChunkCacheStat('chunkcache_chunks_evicted', 'chunks evicted', False),
    ChunkCacheStat('chunkcache_chunks_inuse', 'total chunks held by the chunk cache', False),
    ChunkCacheStat('chunkcache_chunks_loaded_from_flushed_tables', 'number of chunks loaded from flushed tables in chunk cache', False),
    ChunkCacheStat('chunkcache_chunks_pinned', 'total pinned chunks held by the chunk cache', False),
    ChunkCacheStat('chunkcache_created_from_metadata', 'total number of chunks inserted on startup from persisted metadata.', False),
    ChunkCacheStat('chunkcache_exceeded_bitmap_capacity', 'could not allocate due to exceeding bitmap capacity', False),
    ChunkCacheStat('chunkcache_exceeded_capacity', 'could not allocate due to exceeding capacity', False),
    ChunkCacheStat('chunkcache_io_failed', 'number of times a read from storage failed', False),
    ChunkCacheStat('chunkcache_lookups', 'lookups', False),
    ChunkCacheStat('chunkcache_metadata_inserted', 'number of metadata entries inserted', False),
    ChunkCacheStat('chunkcache_metadata_removed', 'number of metadata entries removed', False),
    ChunkCacheStat('chunkcache_metadata_work_units_created', 'number of metadata inserts/deletes pushed to the worker thread', False),
    ChunkCacheStat('chunkcache_metadata_work_units_dequeued', 'number of metadata inserts/deletes read by the worker thread', False),
    ChunkCacheStat('chunkcache_metadata_work_units_dropped', 'number of metadata inserts/deletes dropped by the worker thread', False),
    ChunkCacheStat('chunkcache_misses', 'number of misses', False),
    ChunkCacheStat('chunkcache_retries', 'retried accessing a chunk while I/O was in progress', False),
    ChunkCacheStat('chunkcache_retries_checksum_mismatch', 'retries from a chunk cache checksum mismatch', False),
    ChunkCacheStat('chunkcache_spans_chunks_read', 'aggregate number of spanned chunks on read', False),
    ChunkCacheStat('chunkcache_toomany_retries', 'timed out due to too many retries', False),

    ##########################################
    # Cursor operations
    ##########################################
    CursorStat('cursor_bulk_count', 'bulk cursor count', False, 'no_clear,no_scale'),
    CursorStat('cursor_cache', 'cursor close calls that result in cache', False),
    CursorStat('cursor_cached_count', 'cached cursor count', False, 'no_clear,no_scale'),
    CursorStat('cursor_create', 'cursor create calls', False),
    CursorStat('cursor_insert', 'cursor insert calls', False),
    CursorStat('cursor_insert_bulk', 'cursor bulk loaded cursor insert calls', False),
    CursorStat('cursor_insert_bytes', 'cursor insert key and value bytes', False, 'size'),
    CursorStat('cursor_modify', 'cursor modify calls', False),
    CursorStat('cursor_modify_bytes', 'cursor modify key and value bytes affected', False, 'size'),
    CursorStat('cursor_modify_bytes_touch', 'cursor modify value bytes modified', False, 'size'),
    CursorStat('cursor_next', 'cursor next calls', False),
    CursorStat('cursor_prev', 'cursor prev calls', False),
    CursorStat('cursor_remove', 'cursor remove calls', False),
    CursorStat('cursor_remove_bytes', 'cursor remove key bytes removed', False, 'size'),
    CursorStat('cursor_reopen', 'cursors reused from cache', False),
    CursorStat('cursor_reserve', 'cursor reserve calls', False),
    CursorStat('cursor_reset', 'cursor reset calls', False),
    CursorStat('cursor_restart', 'cursor operation restarted', False),
    CursorStat('cursor_search', 'cursor search calls', False),
    CursorStat('cursor_search_hs', 'cursor search history store calls', False),
    CursorStat('cursor_search_near', 'cursor search near calls', False),
    CursorStat('cursor_truncate', 'cursor truncate calls', False),
    CursorStat('cursor_truncate_keys_deleted', 'cursor truncates performed on individual keys', False),
    CursorStat('cursor_update', 'cursor update calls', False),
    CursorStat('cursor_update_bytes', 'cursor update key and value bytes', False, 'size'),
    CursorStat('cursor_update_bytes_changed', 'cursor update value size change', False, 'size'),

    ##########################################
    # Cursor sweep
    ##########################################
    CursorStat('cursor_sweep', 'cursor sweeps', False),
    CursorStat('cursor_sweep_buckets', 'cursor sweep buckets', False),
    CursorStat('cursor_sweep_closed', 'cursor sweep cursors closed', False),
    CursorStat('cursor_sweep_examined', 'cursor sweep cursors examined', False),

    ##########################################
    # Dhandle statistics
    ##########################################
    DhandleStat('dh_conn_handle_count', 'connection data handles currently active', False, 'no_clear,no_scale'),
    DhandleStat('dh_conn_handle_size', 'connection data handle size', False, 'no_clear,no_scale,size'),
    DhandleStat('dh_session_handles', 'session dhandles swept', False),
    DhandleStat('dh_session_sweeps', 'session sweep attempts', False),
    DhandleStat('dh_sweep_close', 'connection sweep dhandles closed', False),
    DhandleStat('dh_sweep_ref', 'connection sweep candidate became referenced', False),
    DhandleStat('dh_sweep_remove', 'connection sweep dhandles removed from hash list', False),
    DhandleStat('dh_sweep_skip_ckpt', 'connection sweeps skipped due to checkpoint gathering handles', False),
    DhandleStat('dh_sweep_tod', 'connection sweep time-of-death sets', False),
    DhandleStat('dh_sweeps', 'connection sweeps', False),

    ##########################################
    # Locking statistics
    ##########################################
    LockStat('lock_checkpoint_count', 'checkpoint lock acquisitions', False),
    LockStat('lock_checkpoint_wait_application', 'checkpoint lock application thread wait time (usecs)', False),
    LockStat('lock_checkpoint_wait_internal', 'checkpoint lock internal thread wait time (usecs)', False),
    LockStat('lock_dhandle_read_count', 'dhandle read lock acquisitions', False),
    LockStat('lock_dhandle_wait_application', 'dhandle lock application thread time waiting (usecs)', False),
    LockStat('lock_dhandle_wait_internal', 'dhandle lock internal thread time waiting (usecs)', False),
    LockStat('lock_dhandle_write_count', 'dhandle write lock acquisitions', False),
    LockStat('lock_metadata_count', 'metadata lock acquisitions', False),
    LockStat('lock_metadata_wait_application', 'metadata lock application thread wait time (usecs)', False),
    LockStat('lock_metadata_wait_internal', 'metadata lock internal thread wait time (usecs)', False),
    LockStat('lock_schema_count', 'schema lock acquisitions', False),
    LockStat('lock_schema_wait_application', 'schema lock application thread wait time (usecs)', False),
    LockStat('lock_schema_wait_internal', 'schema lock internal thread wait time (usecs)', False),
    LockStat('lock_table_read_count', 'table read lock acquisitions', False),
    LockStat('lock_table_wait_application', 'table lock application thread time waiting for the table lock (usecs)', False),
    LockStat('lock_table_wait_internal', 'table lock internal thread time waiting for the table lock (usecs)', False),
    LockStat('lock_table_write_count', 'table write lock acquisitions', False),
    LockStat('lock_txn_global_read_count', 'txn global read lock acquisitions', False),
    LockStat('lock_txn_global_wait_application', 'txn global lock application thread time waiting (usecs)', False),
    LockStat('lock_txn_global_wait_internal', 'txn global lock internal thread time waiting (usecs)', False),
    LockStat('lock_txn_global_write_count', 'txn global write lock acquisitions', False),

    ##########################################
    # Logging statistics
    ##########################################
    LogStat('log_buffer_size', 'total log buffer size', False, 'no_clear,no_scale,size'),
    LogStat('log_bytes_payload', 'log bytes of payload data', False, 'size'),
    LogStat('log_bytes_written', 'log bytes written', False, 'size'),
    LogStat('log_close_yields', 'yields waiting for previous log file close', False),
    LogStat('log_compress_len', 'total size of compressed records', False, 'size'),
    LogStat('log_compress_mem', 'total in-memory size of compressed records', False, 'size'),
    LogStat('log_compress_small', 'log records too small to compress', False),
    LogStat('log_compress_write_fails', 'log records not compressed', False),
    LogStat('log_compress_writes', 'log records compressed', False),
    LogStat('log_flush', 'log flush operations', False),
    LogStat('log_force_remove_sleep', 'force log remove time sleeping (usecs)', False),
    LogStat('log_force_write', 'log force write operations', False),
    LogStat('log_force_write_skip', 'log force write operations skipped', False),
    LogStat('log_max_filesize', 'maximum log file size', False, 'no_clear,no_scale,size'),
    LogStat('log_prealloc_files', 'pre-allocated log files prepared', False),
    LogStat('log_prealloc_max', 'number of pre-allocated log files to create', False, 'no_clear,no_scale'),
    LogStat('log_prealloc_missed', 'pre-allocated log files not ready and missed', False),
    LogStat('log_prealloc_used', 'pre-allocated log files used', False),
    LogStat('log_release_write_lsn', 'log release advances write LSN', False),
    LogStat('log_scan_records', 'records processed by log scan', False),
    LogStat('log_scan_rereads', 'log scan records requiring two reads', False),
    LogStat('log_scans', 'log scan operations', False),
    LogStat('log_slot_active_closed', 'slot join found active slot closed', False),
    LogStat('log_slot_close_race', 'slot close lost race', False),
    LogStat('log_slot_close_unbuf', 'slot close unbuffered waits', False),
    LogStat('log_slot_closes', 'slot closures', False),
    LogStat('log_slot_coalesced', 'written slots coalesced', False),
    LogStat('log_slot_consolidated', 'logging bytes consolidated', False, 'size'),
    LogStat('log_slot_immediate', 'slot join calls did not yield', False),
    LogStat('log_slot_no_free_slots', 'slot transitions unable to find free slot', False),
    LogStat('log_slot_races', 'slot join atomic update races', False),
    LogStat('log_slot_switch_busy', 'busy returns attempting to switch slots', False),
    LogStat('log_slot_unbuffered', 'slot unbuffered writes', False),
    LogStat('log_slot_yield', 'slot join calls yielded', False),
    LogStat('log_slot_yield_close', 'slot join calls found active slot closed', False),
    LogStat('log_slot_yield_duration', 'slot joins yield time (usecs)', False, 'no_clear,no_scale'),
    LogStat('log_slot_yield_race', 'slot join calls atomic updates raced', False),
    LogStat('log_slot_yield_sleep', 'slot join calls slept', False),
    LogStat('log_sync', 'log sync operations', False),
    LogStat('log_sync_dir', 'log sync_dir operations', False),
    LogStat('log_sync_dir_duration', 'log sync_dir time duration (usecs)', False, 'no_clear,no_scale'),
    LogStat('log_sync_duration', 'log sync time duration (usecs)', False, 'no_clear,no_scale'),
    LogStat('log_write_lsn', 'log server thread advances write LSN', False),
    LogStat('log_write_lsn_skip', 'log server thread write LSN walk skipped', False),
    LogStat('log_writes', 'log write operations', False),
    LogStat('log_zero_fills', 'log files manually zero-filled', False),

    ##########################################
    # LSM statistics
    ##########################################
    LSMStat('lsm_rows_merged', 'rows merged in an LSM tree', False),
    LSMStat('lsm_work_queue_app', 'application work units currently queued', False, 'no_clear,no_scale'),
    LSMStat('lsm_work_queue_manager', 'merge work units currently queued', False, 'no_clear,no_scale'),
    LSMStat('lsm_work_queue_max', 'tree queue hit maximum', False),
    LSMStat('lsm_work_queue_switch', 'switch work units currently queued', False, 'no_clear,no_scale'),
    LSMStat('lsm_work_units_created', 'tree maintenance operations scheduled', False),
    LSMStat('lsm_work_units_discarded', 'tree maintenance operations discarded', False),
    LSMStat('lsm_work_units_done', 'tree maintenance operations executed', False),

    ##########################################
    # Performance Histogram Stats
    ##########################################
    PerfHistStat('perf_hist_fsread_latency_gt1000', 'file system read latency histogram (bucket 7) - 1000ms+', False),
    PerfHistStat('perf_hist_fsread_latency_lt10', 'file system read latency histogram (bucket 1) - 0-10ms', False),
    PerfHistStat('perf_hist_fsread_latency_lt50', 'file system read latency histogram (bucket 2) - 10-49ms', False),
    PerfHistStat('perf_hist_fsread_latency_lt100', 'file system read latency histogram (bucket 3) - 50-99ms', False),
    PerfHistStat('perf_hist_fsread_latency_lt250', 'file system read latency histogram (bucket 4) - 100-249ms', False),
    PerfHistStat('perf_hist_fsread_latency_lt500', 'file system read latency histogram (bucket 5) - 250-499ms', False),
    PerfHistStat('perf_hist_fsread_latency_lt1000', 'file system read latency histogram (bucket 6) - 500-999ms', False),
    PerfHistStat('perf_hist_fsread_latency_total_msecs', 'file system read latency histogram total (msecs)', False),
    PerfHistStat('perf_hist_fswrite_latency_gt1000', 'file system write latency histogram (bucket 7) - 1000ms+', False),
    PerfHistStat('perf_hist_fswrite_latency_lt10', 'file system write latency histogram (bucket 1) - 0-10ms', False),
    PerfHistStat('perf_hist_fswrite_latency_lt50', 'file system write latency histogram (bucket 2) - 10-49ms', False),
    PerfHistStat('perf_hist_fswrite_latency_lt100', 'file system write latency histogram (bucket 3) - 50-99ms', False),
    PerfHistStat('perf_hist_fswrite_latency_lt250', 'file system write latency histogram (bucket 4) - 100-249ms', False),
    PerfHistStat('perf_hist_fswrite_latency_lt500', 'file system write latency histogram (bucket 5) - 250-499ms', False),
    PerfHistStat('perf_hist_fswrite_latency_lt1000', 'file system write latency histogram (bucket 6) - 500-999ms', False),
    PerfHistStat('perf_hist_fswrite_latency_total_msecs', 'file system write latency histogram total (msecs)', False),
    PerfHistStat('perf_hist_opread_latency_gt10000', 'operation read latency histogram (bucket 6) - 10000us+', False),
    PerfHistStat('perf_hist_opread_latency_lt100', 'operation read latency histogram (bucket 1) - 0-100us', False),
    PerfHistStat('perf_hist_opread_latency_lt250', 'operation read latency histogram (bucket 2) - 100-249us', False),
    PerfHistStat('perf_hist_opread_latency_lt500', 'operation read latency histogram (bucket 3) - 250-499us', False),
    PerfHistStat('perf_hist_opread_latency_lt1000', 'operation read latency histogram (bucket 4) - 500-999us', False),
    PerfHistStat('perf_hist_opread_latency_lt10000', 'operation read latency histogram (bucket 5) - 1000-9999us', False),
    PerfHistStat('perf_hist_opread_latency_total_usecs', 'operation read latency histogram total (usecs)', False),
    PerfHistStat('perf_hist_opwrite_latency_gt10000', 'operation write latency histogram (bucket 6) - 10000us+', False),
    PerfHistStat('perf_hist_opwrite_latency_lt100', 'operation write latency histogram (bucket 1) - 0-100us', False),
    PerfHistStat('perf_hist_opwrite_latency_lt250', 'operation write latency histogram (bucket 2) - 100-249us', False),
    PerfHistStat('perf_hist_opwrite_latency_lt500', 'operation write latency histogram (bucket 3) - 250-499us', False),
    PerfHistStat('perf_hist_opwrite_latency_lt1000', 'operation write latency histogram (bucket 4) - 500-999us', False),
    PerfHistStat('perf_hist_opwrite_latency_lt10000', 'operation write latency histogram (bucket 5) - 1000-9999us', False),
    PerfHistStat('perf_hist_opwrite_latency_total_usecs', 'operation write latency histogram total (usecs)', False),

    ##########################################
    # Prefetch statistics
    ##########################################
    PrefetchStat('prefetch_attempts', 'pre-fetch triggered by page read', False),
    PrefetchStat('prefetch_disk_one', 'pre-fetch not triggered after single disk read', False),
    PrefetchStat('prefetch_pages_queued', 'pre-fetch pages queued', False),
    PrefetchStat('prefetch_failed_start', 'number of times pre-fetch failed to start', False),
    PrefetchStat('prefetch_pages_read', 'pre-fetch pages read in background', False),
    PrefetchStat('prefetch_skipped', 'pre-fetch not triggered by page read', False),
    PrefetchStat('prefetch_skipped_disk_read_count', 'pre-fetch not triggered due to disk read count', False),
    PrefetchStat('prefetch_skipped_internal_page', 'could not perform pre-fetch on internal page', False),
    PrefetchStat('prefetch_skipped_internal_session', 'pre-fetch not triggered due to internal session', False),
    PrefetchStat('prefetch_skipped_no_flag_set', 'could not perform pre-fetch on ref without the pre-fetch flag set', False),
    PrefetchStat('prefetch_skipped_no_valid_dhandle', 'pre-fetch not triggered as there is no valid dhandle', False),
    PrefetchStat('prefetch_skipped_same_ref', 'pre-fetch not repeating for recently pre-fetched ref', False),
    PrefetchStat('prefetch_skipped_special_handle', 'pre-fetch not triggered due to special btree handle', False),
    PrefetchStat('prefetch_pages_fail', 'pre-fetch page not on disk when reading', False),

    ##########################################
    # Reconciliation statistics
    ##########################################
    RecStat('rec_maximum_hs_wrapup_milliseconds', 'maximum milliseconds spent in moving updates to the history store in a reconciliation', False, 'no_clear,no_scale,size'),
    RecStat('rec_maximum_image_build_milliseconds', 'maximum milliseconds spent in building a disk image in a reconciliation', False, 'no_clear,no_scale,size'),
    RecStat('rec_maximum_milliseconds', 'maximum milliseconds spent in a reconciliation call', False, 'no_clear,no_scale,size'),
    RecStat('rec_pages_with_prepare', 'page reconciliation calls that resulted in values with prepared transaction metadata', False),
    RecStat('rec_pages_with_ts', 'page reconciliation calls that resulted in values with timestamps', False),
    RecStat('rec_pages_with_txn', 'page reconciliation calls that resulted in values with transaction ids', False),
    RecStat('rec_split_stashed_bytes', 'split bytes currently awaiting free', False, 'no_clear,no_scale,size'),
    RecStat('rec_split_stashed_objects', 'split objects currently awaiting free', False, 'no_clear,no_scale'),
    RecStat('rec_time_window_pages_prepared', 'pages written including at least one prepare state', False),
    RecStat('rec_time_window_pages_start_ts', 'pages written including at least one start timestamp', False),
    RecStat('rec_time_window_prepared', 'records written including a prepare state', False),

    ##########################################
    # Session operations
    ##########################################
    SessionOpStat('session_open', 'open session count', False, 'no_clear,no_scale'),
    SessionOpStat('session_query_ts', 'session query timestamp calls', False),
    SessionOpStat('session_table_alter_fail', 'table alter failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_alter_skip', 'table alter unchanged and skipped', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_alter_success', 'table alter successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_alter_trigger_checkpoint', 'table alter triggering checkpoint calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_conflicting_checkpoint', 'table compact conflicted with checkpoint', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_dhandle_success', 'table compact dhandle successful calls', False, 'no_scale'),
    SessionOpStat('session_table_compact_fail', 'table compact failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_fail_cache_pressure', 'table compact failed calls due to cache pressure', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_passes', 'table compact passes', False, 'no_scale'),
    SessionOpStat('session_table_compact_running', 'table compact running', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_skipped', 'table compact skipped as process would not reduce file size', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_success', 'table compact successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_compact_timeout', 'table compact timeout', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_create_fail', 'table create failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_create_success', 'table create successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_create_import_fail', 'table create with import failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_create_import_success', 'table create with import successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_drop_fail', 'table drop failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_drop_success', 'table drop successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_salvage_fail', 'table salvage failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_salvage_success', 'table salvage successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_truncate_fail', 'table truncate failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_truncate_success', 'table truncate successful calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_verify_fail', 'table verify failed calls', False, 'no_clear,no_scale'),
    SessionOpStat('session_table_verify_success', 'table verify successful calls', False, 'no_clear,no_scale'),

    ##########################################
    # Tiered storage statistics
    ##########################################
    StorageStat('flush_tier', 'flush_tier operation calls', False),
    StorageStat('flush_tier_fail', 'flush_tier failed calls', False),
    StorageStat('flush_tier_skipped', 'flush_tier tables skipped due to no checkpoint', False),
    StorageStat('flush_tier_switched', 'flush_tier tables switched', False),
    StorageStat('local_objects_inuse', 'attempts to remove a local object and the object is in use', False),
    StorageStat('local_objects_removed', 'local objects removed', False),
    StorageStat('tiered_retention', 'tiered storage local retention time (secs)', False, 'no_clear,no_scale,size'),
    StorageStat('tiered_work_units_created', 'tiered operations scheduled', False),
    StorageStat('tiered_work_units_dequeued', 'tiered operations dequeued and processed', False),
    StorageStat('tiered_work_units_removed', 'tiered operations removed without processing', False),

    ##########################################
    # Thread Count statistics
    ##########################################
    ThreadStat('thread_fsync_active', 'active filesystem fsync calls', False,'no_clear,no_scale'),
    ThreadStat('thread_read_active', 'active filesystem read calls', False,'no_clear,no_scale'),
    ThreadStat('thread_write_active', 'active filesystem write calls', False,'no_clear,no_scale'),

    ##########################################
    # Transaction statistics
    ##########################################
    TxnStat('txn_begin', 'transaction begins', False),
    TxnStat('txn_commit', 'transactions committed', False, flags='user_facing'),
    TxnStat('txn_hs_ckpt_duration', 'transaction checkpoint history store file duration (usecs)', False),
    TxnStat('txn_pinned_checkpoint_range', 'transaction range of IDs currently pinned by a checkpoint', False, 'no_clear,no_scale'),
    TxnStat('txn_pinned_range', 'transaction range of IDs currently pinned', False, 'no_clear,no_scale'),
    TxnStat('txn_pinned_timestamp', 'transaction range of timestamps currently pinned', False, 'no_clear,no_scale'),
    TxnStat('txn_pinned_timestamp_checkpoint', 'transaction range of timestamps pinned by a checkpoint', False, 'no_clear,no_scale'),
    TxnStat('txn_pinned_timestamp_oldest', 'transaction range of timestamps pinned by the oldest timestamp', False, 'no_clear,no_scale'),
    TxnStat('txn_pinned_timestamp_reader', 'transaction range of timestamps pinned by the oldest active read timestamp', False, 'no_clear,no_scale'),
    TxnStat('txn_prepare', 'prepared transactions', False),
    TxnStat('txn_prepare_active', 'prepared transactions currently active', False),
    TxnStat('txn_prepare_commit', 'prepared transactions committed', False),
    TxnStat('txn_prepare_rollback', 'prepared transactions rolled back', False),
    TxnStat('txn_prepared_updates', 'Number of prepared updates', False),
    TxnStat('txn_prepared_updates_committed', 'Number of prepared updates committed', False),
    TxnStat('txn_prepared_updates_key_repeated', 'Number of prepared updates repeated on the same key', False),
    TxnStat('txn_prepared_updates_rolledback', 'Number of prepared updates rolled back', False),
    TxnStat('txn_query_ts', 'query timestamp calls', False),
    TxnStat('txn_rollback', 'transactions rolled back', False),
    TxnStat('txn_rollback_oldest_pinned', 'oldest pinned transaction ID rolled back for eviction', False),
    TxnStat('txn_rollback_to_stable_running', 'transaction rollback to stable currently running', False, 'no_clear,no_scale'),
    TxnStat('txn_rts', 'rollback to stable calls', False),
    TxnStat('txn_rts_pages_visited', 'rollback to stable pages visited', False),
    TxnStat('txn_rts_tree_walk_skip_pages', 'rollback to stable tree walk skipping pages', False),
    TxnStat('txn_rts_upd_aborted', 'rollback to stable updates aborted', False),
    TxnStat('txn_rts_upd_aborted_dryrun', 'rollback to stable updates that would have been aborted in non-dryrun mode', False),
    TxnStat('txn_sessions_walked', 'sessions scanned in each walk of concurrent sessions', False),
    TxnStat('txn_set_ts', 'set timestamp calls', False),
    TxnStat('txn_set_ts_durable', 'set timestamp durable calls', False),
    TxnStat('txn_set_ts_durable_upd', 'set timestamp durable updates', False),
    TxnStat('txn_set_ts_force', 'set timestamp force calls', False),
    TxnStat('txn_set_ts_oldest', 'set timestamp oldest calls', False),
    TxnStat('txn_set_ts_oldest_upd', 'set timestamp oldest updates', False),
    TxnStat('txn_set_ts_out_of_order', 'set timestamp global oldest timestamp set to be more recent than the global stable timestamp', False),
    TxnStat('txn_set_ts_stable', 'set timestamp stable calls', False),
    TxnStat('txn_set_ts_stable_upd', 'set timestamp stable updates', False),
    TxnStat('txn_timestamp_oldest_active_read', 'transaction read timestamp of the oldest active reader', False, 'no_clear,no_scale'),
    TxnStat('txn_walk_sessions', 'transaction walk of concurrent sessions', False),

    ##########################################
    # Yield statistics
    ##########################################
    YieldStat('application_cache_time', 'application thread time waiting for cache (usecs)', False),
    YieldStat('application_evict_time', 'application thread time evicting (usecs)', False),
    YieldStat('application_evict_snapshot_refreshed', 'application thread snapshot refreshed for eviction', False),
    YieldStat('child_modify_blocked_page', 'page reconciliation yielded due to child modification', False),
    YieldStat('conn_close_blocked_lsm', 'connection close yielded for lsm manager shutdown', False),
    YieldStat('dhandle_lock_blocked', 'data handle lock yielded', False),
    YieldStat('page_busy_blocked', 'page acquire busy blocked', False),
    YieldStat('page_del_rollback_blocked', 'page delete rollback time sleeping for state change (usecs)', False),
    YieldStat('page_forcible_evict_blocked', 'page acquire eviction blocked', False),
    YieldStat('page_index_slot_ref_blocked', 'get reference for page index and slot time sleeping (usecs)', False),
    YieldStat('page_locked_blocked', 'page acquire locked blocked', False),
    YieldStat('page_read_blocked', 'page acquire read blocked', False),
    YieldStat('page_sleep', 'page acquire time sleeping (usecs)', False),
    YieldStat('prepared_transition_blocked_page', 'page access yielded due to prepare state change', False),
    YieldStat('txn_release_blocked', 'connection close blocked waiting for transaction state stabilization', False),
]

conn_stats = sorted(conn_stats, key=attrgetter('desc'))

##########################################
# Data source statistics
##########################################
dsrc_stats = [
    ##########################################
    # Block manager statistics
    ##########################################
    BlockStat('allocation_size', 'file allocation unit size', False, 'max_aggregate,no_scale,size'),
    BlockStat('block_alloc', 'blocks allocated', False),
    BlockStat('block_checkpoint_size', 'checkpoint size', False, 'no_scale,size'),
    BlockStat('block_extension', 'allocations requiring file extension', False),
    BlockStat('block_free', 'blocks freed', False),
    BlockStat('block_magic', 'file magic number', False, 'max_aggregate,no_scale'),
    BlockStat('block_major', 'file major version number', False, 'max_aggregate,no_scale'),
    BlockStat('block_minor', 'minor version number', False, 'max_aggregate,no_scale'),
    BlockStat('block_reuse_bytes', 'file bytes available for reuse', False, 'no_scale,size'),
    BlockStat('block_size', 'file size in bytes', False, 'no_scale,size'),

    ##########################################
    # Btree statistics
    ##########################################
    BtreeStat('btree_checkpoint_generation', 'btree checkpoint generation', False, 'no_clear,no_scale'),
    BtreeStat('btree_clean_checkpoint_timer', 'btree clean tree checkpoint expiration time', False, 'no_clear,no_scale'),
    BtreeStat('btree_column_deleted', 'column-store variable-size deleted values', False, 'no_scale,tree_walk'),
    BtreeStat('btree_column_fix', 'column-store fixed-size leaf pages', False, 'no_scale,tree_walk'),
    BtreeStat('btree_column_internal', 'column-store internal pages', False, 'no_scale,tree_walk'),
    BtreeStat('btree_column_rle', 'column-store variable-size RLE encoded values', False, 'no_scale,tree_walk'),
    BtreeStat('btree_column_tws', 'column-store fixed-size time windows', False, 'no_scale,tree_walk'),
    BtreeStat('btree_column_variable', 'column-store variable-size leaf pages', False, 'no_scale,tree_walk'),
    BtreeStat('btree_compact_bytes_rewritten_expected', 'btree expected number of compact bytes rewritten', False, 'no_clear,no_scale'),
    BtreeStat('btree_compact_pages_reviewed', 'btree compact pages reviewed', False, 'no_clear,no_scale'),
    BtreeStat('btree_compact_pages_rewritten', 'btree compact pages rewritten', False, 'no_clear,no_scale'),
    BtreeStat('btree_compact_pages_rewritten_expected', 'btree expected number of compact pages rewritten', False, 'no_clear,no_scale'),
    BtreeStat('btree_compact_pages_skipped', 'btree compact pages skipped', False, 'no_clear,no_scale'),
    BtreeStat('btree_compact_skipped', 'btree skipped by compaction as process would not reduce size', False, 'no_clear,no_scale'),
    BtreeStat('btree_entries', 'number of key/value pairs', False, 'no_scale,tree_walk'),
    BtreeStat('btree_fixed_len', 'fixed-record size', False, 'max_aggregate,no_scale,size'),
    BtreeStat('btree_maximum_depth', 'maximum tree depth', False, 'max_aggregate,no_scale'),
    BtreeStat('btree_maxintlpage', 'maximum internal page size', False, 'max_aggregate,no_scale,size'),
    BtreeStat('btree_maxleafkey', 'maximum leaf page key size', False, 'max_aggregate,no_scale,size'),
    BtreeStat('btree_maxleafpage', 'maximum leaf page size', False, 'max_aggregate,no_scale,size'),
    BtreeStat('btree_maxleafvalue', 'maximum leaf page value size', False, 'max_aggregate,no_scale,size'),
    BtreeStat('btree_overflow', 'overflow pages', False, 'no_scale,tree_walk'),
    BtreeStat('btree_row_empty_values', 'row-store empty values', False, 'no_scale,tree_walk'),
    BtreeStat('btree_row_internal', 'row-store internal pages', False, 'no_scale,tree_walk'),
    BtreeStat('btree_row_leaf', 'row-store leaf pages', False, 'no_scale,tree_walk'),

    ##########################################
    # Cache and eviction statistics
    ##########################################
    CacheStat('cache_eviction_fail', 'data source pages selected for eviction unable to be evicted', False),
    CacheStat('cache_eviction_walk_passes', 'eviction walk passes of a file', False),

    ##########################################
    # Cache content statistics
    ##########################################
    CacheWalkStat('cache_state_avg_unvisited_age', 'Average time in cache for pages that have not been visited by the eviction server', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_avg_visited_age', 'Average time in cache for pages that have been visited by the eviction server', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_avg_written_size', 'Average on-disk page image size seen', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_gen_avg_gap', 'Average difference between current eviction generation when the page was last considered', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_gen_current', 'Current eviction generation', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_gen_max_gap', 'Maximum difference between current eviction generation when the page was last considered', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_max_pagesize', 'Maximum page size seen', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_memory', 'Pages created in memory and never written', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_min_written_size', 'Minimum on-disk page image size seen', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_not_queueable', 'Pages that could not be queued for eviction', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_pages', 'Total number of pages currently in cache', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_pages_clean', 'Clean pages currently in cache', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_pages_dirty', 'Dirty pages currently in cache', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_pages_internal', 'Internal pages currently in cache', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_pages_leaf', 'Leaf pages currently in cache', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_queued', 'Pages currently queued for eviction', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_refs_skipped', 'Refs skipped during cache traversal', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_root_entries', 'Entries in the root page', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_root_size', 'Size of the root page', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_smaller_alloc_size', 'On-disk page image sizes smaller than a single allocation unit', False, 'no_clear,no_scale'),
    CacheWalkStat('cache_state_unvisited_count', 'Number of pages never visited by eviction server', False, 'no_clear,no_scale'),

    ##########################################
    # Compression statistics
    ##########################################
    CompressStat('compress_precomp_intl_max_page_size', 'compressed page maximum internal page size prior to compression', False, 'no_clear,no_scale,size'),
    CompressStat('compress_precomp_leaf_max_page_size', 'compressed page maximum leaf page size prior to compression ', False, 'no_clear,no_scale,size'),
    CompressStat('compress_read', 'pages read from disk', False),
    # dist/stat.py sorts stats by their descriptions and not their names. The following stat descriptions insert an extra
    # space before the single digit numbers (2, 4, 8) so stats will be sorted numerically (2, 4, 8, 16, 32) instead of
    # alphabetically (16, 2, 32, 4, 8).
    CompressStat('compress_read_ratio_hist_2', 'pages read from disk with compression ratio smaller than  2', False),
    CompressStat('compress_read_ratio_hist_4', 'pages read from disk with compression ratio smaller than  4', False),
    CompressStat('compress_read_ratio_hist_8', 'pages read from disk with compression ratio smaller than  8', False),
    CompressStat('compress_read_ratio_hist_16', 'pages read from disk with compression ratio smaller than 16', False),
    CompressStat('compress_read_ratio_hist_32', 'pages read from disk with compression ratio smaller than 32', False),
    CompressStat('compress_read_ratio_hist_64', 'pages read from disk with compression ratio smaller than 64', False),
    CompressStat('compress_read_ratio_hist_max', 'pages read from disk with compression ratio greater than 64', False),
    CompressStat('compress_write', 'pages written to disk', False),
    CompressStat('compress_write_ratio_hist_2', 'pages written to disk with compression ratio smaller than  2', False),
    CompressStat('compress_write_ratio_hist_4', 'pages written to disk with compression ratio smaller than  4', False),
    CompressStat('compress_write_ratio_hist_8', 'pages written to disk with compression ratio smaller than  8', False),
    CompressStat('compress_write_ratio_hist_16', 'pages written to disk with compression ratio smaller than 16', False),
    CompressStat('compress_write_ratio_hist_32', 'pages written to disk with compression ratio smaller than 32', False),
    CompressStat('compress_write_ratio_hist_64', 'pages written to disk with compression ratio smaller than 64', False),
    CompressStat('compress_write_ratio_hist_max', 'pages written to disk with compression ratio greater than 64', False),
    CompressStat('compress_write_fail', 'page written to disk failed to compress', False),
    CompressStat('compress_write_too_small', 'page written to disk was too small to compress', False),

    ##########################################
    # Cursor operations
    ##########################################
    CursorStat('cursor_cache', 'close calls that result in cache', False),
    CursorStat('cursor_create', 'create calls', False),
    CursorStat('cursor_insert', 'insert calls', False),
    CursorStat('cursor_insert_bulk', 'bulk loaded cursor insert calls', False),
    CursorStat('cursor_insert_bytes', 'insert key and value bytes', False, 'size'),
    CursorStat('cursor_modify', 'modify', False),
    CursorStat('cursor_modify_bytes', 'modify key and value bytes affected', False, 'size'),
    CursorStat('cursor_modify_bytes_touch', 'modify value bytes modified', False, 'size'),
    CursorStat('cursor_next', 'next calls', False),
    CursorStat('cursor_prev', 'prev calls', False),
    CursorStat('cursor_remove', 'remove calls', False),
    CursorStat('cursor_remove_bytes', 'remove key bytes removed', False, 'size'),
    CursorStat('cursor_reopen', 'cache cursors reuse count', False),
    CursorStat('cursor_reserve', 'reserve calls', False),
    CursorStat('cursor_reset', 'reset calls', False),
    CursorStat('cursor_restart', 'operation restarted', False),
    CursorStat('cursor_search', 'search calls', False),
    CursorStat('cursor_search_hs', 'search history store calls', False),
    CursorStat('cursor_search_near', 'search near calls', False),
    CursorStat('cursor_truncate', 'truncate calls', False),
    CursorStat('cursor_update', 'update calls', False),
    CursorStat('cursor_update_bytes', 'update key and value bytes', False, 'size'),
    CursorStat('cursor_update_bytes_changed', 'update value size change', False, 'size'),

    ##########################################
    # LSM statistics
    ##########################################
    LSMStat('bloom_count', 'bloom filters in the LSM tree', False, 'no_scale'),
    LSMStat('bloom_false_positive', 'bloom filter false positives', False),
    LSMStat('bloom_hit', 'bloom filter hits', False),
    LSMStat('bloom_miss', 'bloom filter misses', False),
    LSMStat('bloom_page_evict', 'bloom filter pages evicted from cache', False),
    LSMStat('bloom_page_read', 'bloom filter pages read into cache', False),
    LSMStat('bloom_size', 'total size of bloom filters', False, 'no_scale,size'),
    LSMStat('lsm_chunk_count', 'chunks in the LSM tree', False, 'no_scale'),
    LSMStat('lsm_generation_max', 'highest merge generation in the LSM tree', False, 'max_aggregate,no_scale'),
    LSMStat('lsm_lookup_no_bloom', 'queries that could have benefited from a Bloom filter that did not exist', False),

    ##########################################
    # Reconciliation statistics
    ##########################################
    RecStat('rec_dictionary', 'dictionary matches', False),
    RecStat('rec_multiblock_internal', 'internal page multi-block writes', False),
    RecStat('rec_multiblock_leaf', 'leaf page multi-block writes', False),
    RecStat('rec_multiblock_max', 'maximum blocks required for a page', False, 'max_aggregate,no_scale'),
    RecStat('rec_prefix_compression', 'leaf page key bytes discarded using prefix compression', False, 'size'),
    RecStat('rec_suffix_compression', 'internal page key bytes discarded using suffix compression', False, 'size'),
    RecStat('rec_time_window_pages_prepared', 'pages written including at least one prepare', False),
    RecStat('rec_time_window_pages_start_ts', 'pages written including at least one start timestamp', False),
    RecStat('rec_time_window_prepared', 'records written including a prepare', False),

    ##########################################
    # Session operations
    ##########################################
    SessionOpStat('session_compact', 'object compaction', False),
]

dsrc_stats = sorted(dsrc_stats, key=attrgetter('desc'))

##########################################
# CONNECTION AND DATA SOURCE statistics
##########################################
conn_dsrc_stats = [
    ##########################################
    # Autocommit statistics
    ##########################################
    AutoCommitStat('autocommit_readonly_retry', 'retries for readonly operations', False),
    AutoCommitStat('autocommit_update_retry', 'retries for update operations', False),

    ##########################################
    # Cache and eviction statistics
    ##########################################
    CacheStat('cache_bytes_dirty', 'tracked dirty bytes in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_dirty_total', 'bytes dirty in the cache cumulative', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_inuse', 'bytes currently in the cache', False, 'no_clear,no_scale,size'),
    CacheStat('cache_bytes_read', 'bytes read into cache', False, 'size'),
    CacheStat('cache_bytes_write', 'bytes written from cache', False, 'size'),
    CacheStat('cache_eviction_blocked_checkpoint_hs', 'checkpoint of history store file blocked non-history store page eviction', False),
    CacheStat('cache_eviction_blocked_checkpoint', 'checkpoint blocked page eviction', False),
    CacheStat('cache_eviction_blocked_hazard', 'hazard pointer blocked page eviction', False),
    CacheStat('cache_eviction_blocked_internal_page_split', 'internal page split blocked its eviction', False),
    CacheStat('cache_eviction_blocked_no_progress', 'eviction gave up due to no progress being made', False),
    CacheStat('cache_eviction_blocked_no_ts_checkpoint_race_1', 'eviction gave up due to detecting a disk value without a timestamp behind the last update on the chain', False),
    CacheStat('cache_eviction_blocked_no_ts_checkpoint_race_2', 'eviction gave up due to detecting a tombstone without a timestamp ahead of the selected on disk update', False),
    CacheStat('cache_eviction_blocked_no_ts_checkpoint_race_3', 'eviction gave up due to detecting a tombstone without a timestamp ahead of the selected on disk update after validating the update chain', False),
    CacheStat('cache_eviction_blocked_no_ts_checkpoint_race_4', 'eviction gave up due to detecting update chain entries without timestamps after the selected on disk update', False),
    CacheStat('cache_eviction_blocked_overflow_keys', 'overflow keys on a multiblock row-store page blocked its eviction', False),
    CacheStat('cache_eviction_blocked_recently_modified', 'recent modification of a page blocked its eviction', False),
    CacheStat('cache_eviction_blocked_remove_hs_race_with_checkpoint', 'eviction gave up due to needing to remove a record from the history store but checkpoint is running', False),
    CacheStat('cache_eviction_blocked_uncommitted_truncate', 'uncommitted truncate blocked page eviction', False),
    CacheStat('cache_eviction_clean', 'unmodified pages evicted', False),
    CacheStat('cache_eviction_deepen', 'page split during eviction deepened the tree', False),
    CacheStat('cache_eviction_dirty', 'modified pages evicted', False),
    CacheStat('cache_eviction_internal', 'internal pages evicted', False),
    CacheStat('cache_eviction_pages_seen', 'pages seen by eviction walk', False),
    CacheStat('cache_eviction_split_internal', 'internal pages split during eviction', False),
    CacheStat('cache_eviction_split_leaf', 'leaf pages split during eviction', False),
    CacheStat('cache_eviction_target_page_ge128', 'eviction walk target pages histogram - 128 and higher', False),
    CacheStat('cache_eviction_target_page_lt10', 'eviction walk target pages histogram - 0-9', False),
    CacheStat('cache_eviction_target_page_lt128', 'eviction walk target pages histogram - 64-128', False),
    CacheStat('cache_eviction_target_page_lt32', 'eviction walk target pages histogram - 10-31', False),
    CacheStat('cache_eviction_target_page_lt64', 'eviction walk target pages histogram - 32-63', False),
    CacheStat('cache_eviction_target_page_reduced', 'eviction walk target pages reduced due to history store cache pressure', False),
    CacheStat('cache_eviction_walk_from_root', 'eviction walks started from root of tree', False),
    CacheStat('cache_eviction_walk_restart', 'eviction walks restarted', False),
    CacheStat('cache_eviction_walk_saved_pos', 'eviction walks started from saved location in tree', False),
    CacheStat('cache_eviction_walks_abandoned', 'eviction walks abandoned', False),
    CacheStat('cache_eviction_walks_ended', 'eviction walks reached end of tree', False),
    CacheStat('cache_eviction_walks_gave_up_no_targets', 'eviction walks gave up because they saw too many pages and found no candidates', False),
    CacheStat('cache_eviction_walks_gave_up_ratio', 'eviction walks gave up because they saw too many pages and found too few candidates', False),
    CacheStat('cache_eviction_walks_stopped', 'eviction walks gave up because they restarted their walk twice', False),
    CacheStat('cache_hs_btree_truncate', 'history store table truncation to remove all the keys of a btree', False),
    CacheStat('cache_hs_btree_truncate_dryrun', 'history store table truncations that would have happened in non-dryrun mode', False),
    CacheStat('cache_hs_insert', 'history store table insert calls', False),
    CacheStat('cache_hs_insert_full_update', 'the number of times full update inserted to history store', False),
    CacheStat('cache_hs_insert_restart', 'history store table insert calls that returned restart', False),
    CacheStat('cache_hs_insert_reverse_modify', 'the number of times reverse modify inserted to history store', False),
    CacheStat('cache_hs_key_truncate', 'history store table truncation to remove an update', False),
    CacheStat('cache_hs_key_truncate_onpage_removal', 'history store table truncation to remove range of updates due to key being removed from the data page during reconciliation', False),
    CacheStat('cache_hs_key_truncate_rts', 'history store table truncation by rollback to stable to remove an update', False),
    CacheStat('cache_hs_key_truncate_rts_dryrun', 'history store table truncations to remove an update that would have happened in non-dryrun mode', False),
    CacheStat('cache_hs_key_truncate_rts_unstable', 'history store table truncation by rollback to stable to remove an unstable update', False),
    CacheStat('cache_hs_key_truncate_rts_unstable_dryrun', 'history store table truncations to remove an unstable update that would have happened in non-dryrun mode', False),
    CacheStat('cache_hs_order_lose_durable_timestamp', 'history store table resolved updates without timestamps that lose their durable timestamp', False),
    CacheStat('cache_hs_order_reinsert', 'history store table updates without timestamps fixed up by reinserting with the fixed timestamp', False),
    CacheStat('cache_hs_order_remove', 'history store table truncation to remove range of updates due to an update without a timestamp on data page', False),
    CacheStat('cache_hs_read', 'history store table reads', False),
    CacheStat('cache_hs_read_miss', 'history store table reads missed', False),
    CacheStat('cache_hs_read_squash', 'history store table reads requiring squashed modifies', False),
    CacheStat('cache_hs_write_squash', 'history store table writes requiring squashed modifies', False),
    CacheStat('cache_inmem_split', 'in-memory page splits', False),
    CacheStat('cache_inmem_splittable', 'in-memory page passed criteria to be split', False),
    CacheStat('cache_eviction_blocked_multi_block_reconcilation_during_checkpoint', 'multi-block reconciliation blocked whilst checkpoint is running', False),
    CacheStat('cache_pages_prefetch', 'pages requested from the cache due to pre-fetch', False),
    CacheStat('cache_pages_requested', 'pages requested from the cache', False),
    CacheStat('cache_read', 'pages read into cache', False),
    CacheStat('cache_read_checkpoint', 'pages read into cache by checkpoint', False),
    CacheStat('cache_read_deleted', 'pages read into cache after truncate', False),
    CacheStat('cache_read_deleted_prepared', 'pages read into cache after truncate in prepare state', False),
    CacheStat('cache_read_overflow', 'overflow pages read into cache', False),
    CacheStat('cache_reverse_splits', 'reverse splits performed', False),
    CacheStat('cache_reverse_splits_skipped_vlcs', 'reverse splits skipped because of VLCS namespace gap restrictions', False),
    CacheStat('cache_write', 'pages written from cache', False),
    CacheStat('cache_write_hs', 'page written requiring history store records', False),
    CacheStat('cache_write_restore', 'pages written requiring in-memory restoration', False),

    ##########################################
    # Checkpoint statistics
    ##########################################
    CheckpointStat('checkpoint_cleanup_pages_evict', 'pages added for eviction during checkpoint cleanup', False),
    CheckpointStat('checkpoint_cleanup_pages_removed', 'pages removed during checkpoint cleanup', False),
    CheckpointStat('checkpoint_cleanup_pages_visited', 'pages visited during checkpoint cleanup', False),
    CheckpointStat('checkpoint_cleanup_pages_walk_skipped', 'pages skipped during checkpoint cleanup tree walk', False),
    CheckpointStat('checkpoint_obsolete_applied', 'transaction checkpoints due to obsolete pages', False),
    CheckpointStat('checkpoint_snapshot_acquired', 'checkpoint has acquired a snapshot for its transaction', False),

    ##########################################
    # Cursor operations
    ##########################################
    CursorStat('cursor_bounds_comparisons', 'cursor bounds comparisons performed', False),
    CursorStat('cursor_bounds_reset', 'cursor bounds cleared from reset', False),
    CursorStat('cursor_bounds_next_early_exit', 'cursor bounds next early exit', False),
    CursorStat('cursor_bounds_prev_early_exit', 'cursor bounds prev early exit', False),
    CursorStat('cursor_bounds_search_early_exit', 'cursor bounds search early exit', False),
    CursorStat('cursor_bounds_search_near_repositioned_cursor', 'cursor bounds search near call repositioned cursor', False),
    CursorStat('cursor_bounds_next_unpositioned', 'cursor bounds next called on an unpositioned cursor', False),
    CursorStat('cursor_bounds_prev_unpositioned', 'cursor bounds prev called on an unpositioned cursor', False),
    CursorStat('cursor_next_hs_tombstone', 'cursor next calls that skip due to a globally visible history store tombstone', False),
    CursorStat('cursor_next_skip_lt_100', 'cursor next calls that skip greater than 1 and fewer than 100 entries', False),
    CursorStat('cursor_next_skip_ge_100', 'cursor next calls that skip greater than or equal to 100 entries', False),
    CursorStat('cursor_next_skip_total', 'Total number of entries skipped by cursor next calls', False),
    CursorStat('cursor_open_count', 'open cursor count', False, 'no_clear,no_scale'),
    CursorStat('cursor_prev_hs_tombstone', 'cursor prev calls that skip due to a globally visible history store tombstone', False),
    CursorStat('cursor_prev_skip_ge_100', 'cursor prev calls that skip greater than or equal to 100 entries', False),
    CursorStat('cursor_prev_skip_lt_100', 'cursor prev calls that skip less than 100 entries', False),
    CursorStat('cursor_prev_skip_total', 'Total number of entries skipped by cursor prev calls', False),
    CursorStat('cursor_reposition', 'Total number of times cursor temporarily releases pinned page to encourage eviction of hot or large page', False),
    CursorStat('cursor_reposition_failed', 'Total number of times cursor fails to temporarily release pinned page to encourage eviction of hot or large page', False),
    CursorStat('cursor_search_near_prefix_fast_paths', 'Total number of times a search near has exited due to prefix config', False),
    CursorStat('cursor_skip_hs_cur_position', 'Total number of entries skipped to position the history store cursor', False),
    CursorStat('cursor_tree_walk_del_page_skip', 'Total number of deleted pages skipped during tree walk', False),
    CursorStat('cursor_tree_walk_ondisk_del_page_skip', 'Total number of on-disk deleted pages skipped during tree walk', False),
    CursorStat('cursor_tree_walk_inmem_del_page_skip', 'Total number of in-memory deleted pages skipped during tree walk', False),

    ##########################################
    # Cursor API error statistics
    ##########################################
    CursorStat('cursor_bound_error', 'cursor bound calls that return an error', False),
    CursorStat('cursor_cache_error', 'cursor cache calls that return an error', False),
    CursorStat('cursor_close_error', 'cursor close calls that return an error', False),
    CursorStat('cursor_compare_error', 'cursor compare calls that return an error', False),
    CursorStat('cursor_equals_error', 'cursor equals calls that return an error', False),
    CursorStat('cursor_get_key_error', 'cursor get key calls that return an error', False),
    CursorStat('cursor_get_value_error', 'cursor get value calls that return an error', False),
    CursorStat('cursor_insert_check_error', 'cursor insert check calls that return an error', False),
    CursorStat('cursor_insert_error', 'cursor insert calls that return an error', False),
    CursorStat('cursor_largest_key_error', 'cursor largest key calls that return an error', False),
    CursorStat('cursor_modify_error', 'cursor modify calls that return an error', False),
    CursorStat('cursor_next_error', 'cursor next calls that return an error', False),
    CursorStat('cursor_next_random_error', 'cursor next random calls that return an error', False),
    CursorStat('cursor_prev_error', 'cursor prev calls that return an error', False),
    CursorStat('cursor_reconfigure_error', 'cursor reconfigure calls that return an error', False),
    CursorStat('cursor_reset_error', 'cursor reset calls that return an error', False),
    CursorStat('cursor_reserve_error', 'cursor reserve calls that return an error', False),
    CursorStat('cursor_reopen_error', 'cursor reopen calls that return an error', False),
    CursorStat('cursor_remove_error', 'cursor remove calls that return an error', False),
    CursorStat('cursor_search_near_error', 'cursor search near calls that return an error', False),
    CursorStat('cursor_search_error', 'cursor search calls that return an error', False),
    CursorStat('cursor_update_error', 'cursor update calls that return an error', False),

    ##########################################
    # LSM statistics
    ##########################################
    LSMStat('lsm_checkpoint_throttle', 'sleep for LSM checkpoint throttle', False),
    LSMStat('lsm_merge_throttle', 'sleep for LSM merge throttle', False),

    ##########################################
    # Reconciliation statistics
    ##########################################
    RecStat('rec_overflow_key_leaf', 'leaf-page overflow keys', False),
    RecStat('rec_overflow_value', 'overflow values written', False),
    RecStat('rec_page_delete', 'pages deleted', False),
    RecStat('rec_page_delete_fast', 'fast-path pages deleted', False),
    RecStat('rec_pages', 'page reconciliation calls', False),
    RecStat('rec_pages_eviction', 'page reconciliation calls for eviction', False),
    RecStat('rec_time_aggr_newest_start_durable_ts', 'pages written including an aggregated newest start durable timestamp ', False),
    RecStat('rec_time_aggr_newest_stop_durable_ts', 'pages written including an aggregated newest stop durable timestamp ', False),
    RecStat('rec_time_aggr_newest_stop_ts', 'pages written including an aggregated newest stop timestamp ', False),
    RecStat('rec_time_aggr_newest_stop_txn', 'pages written including an aggregated newest stop transaction ID', False),
    RecStat('rec_time_aggr_newest_txn', 'pages written including an aggregated newest transaction ID ', False),
    RecStat('rec_time_aggr_oldest_start_ts', 'pages written including an aggregated oldest start timestamp ', False),
    RecStat('rec_time_aggr_prepared', 'pages written including an aggregated prepare', False),
    RecStat('rec_time_window_bytes_ts', 'approximate byte size of timestamps in pages written', False),
    RecStat('rec_time_window_bytes_txn', 'approximate byte size of transaction IDs in pages written', False),
    RecStat('rec_time_window_durable_start_ts', 'records written including a start durable timestamp', False),
    RecStat('rec_time_window_durable_stop_ts', 'records written including a stop durable timestamp', False),
    RecStat('rec_time_window_pages_durable_start_ts', 'pages written including at least one start durable timestamp', False),
    RecStat('rec_time_window_pages_durable_stop_ts', 'pages written including at least one stop durable timestamp', False),
    RecStat('rec_time_window_pages_start_txn', 'pages written including at least one start transaction ID', False),
    RecStat('rec_time_window_pages_stop_ts', 'pages written including at least one stop timestamp', False),
    RecStat('rec_time_window_pages_stop_txn', 'pages written including at least one stop transaction ID', False),
    RecStat('rec_time_window_start_ts', 'records written including a start timestamp', False),
    RecStat('rec_time_window_start_txn', 'records written including a start transaction ID', False),
    RecStat('rec_time_window_stop_ts', 'records written including a stop timestamp', False),
    RecStat('rec_time_window_stop_txn', 'records written including a stop transaction ID', False),
    RecStat('rec_vlcs_emptied_pages', 'VLCS pages explicitly reconciled as empty', False),

    ##########################################
    # Transaction statistics
    ##########################################
    TxnStat('txn_read_overflow_remove', 'number of times overflow removed value is read', False),
    TxnStat('txn_read_race_prepare_update', 'race to read prepared update retry', False),
    TxnStat('txn_read_race_prepare_commit', 'a reader raced with a prepared transaction commit and skipped an update or updates', False),
    TxnStat('txn_rts_delete_rle_skipped', 'rollback to stable skipping delete rle', False),
    TxnStat('txn_rts_hs_removed', 'rollback to stable updates removed from history store', False),
    TxnStat('txn_rts_hs_removed_dryrun', 'rollback to stable updates that would have been removed from history store in non-dryrun mode', False),
    TxnStat('txn_rts_hs_restore_tombstones', 'rollback to stable restored tombstones from history store', False),
    TxnStat('txn_rts_hs_restore_tombstones_dryrun', 'rollback to stable tombstones from history store that would have been restored in non-dryrun mode', False),
    TxnStat('txn_rts_hs_restore_updates', 'rollback to stable restored updates from history store', False),
    TxnStat('txn_rts_hs_restore_updates_dryrun', 'rollback to stable updates from history store that would have been restored in non-dryrun mode', False),
    TxnStat('txn_rts_hs_stop_older_than_newer_start', 'rollback to stable history store records with stop timestamps older than newer records', False),
    TxnStat('txn_rts_inconsistent_ckpt', 'rollback to stable inconsistent checkpoint', False),
    TxnStat('txn_rts_keys_removed', 'rollback to stable keys removed', False),
    TxnStat('txn_rts_keys_removed_dryrun', 'rollback to stable keys that would have been removed in non-dryrun mode', False),
    TxnStat('txn_rts_keys_restored', 'rollback to stable keys restored', False),
    TxnStat('txn_rts_keys_restored_dryrun', 'rollback to stable keys that would have been restored in non-dryrun mode', False),
    TxnStat('txn_rts_stable_rle_skipped', 'rollback to stable skipping stable rle', False),
    TxnStat('txn_rts_sweep_hs_keys', 'rollback to stable sweeping history store keys', False),
    TxnStat('txn_rts_sweep_hs_keys_dryrun', 'rollback to stable history store keys that would have been swept in non-dryrun mode', False),
    TxnStat('txn_update_conflict', 'update conflicts', False),
]

conn_dsrc_stats = sorted(conn_dsrc_stats, key=attrgetter('desc'))

##########################################
# Cursor Join statistics
##########################################
join_stats = [
    JoinStat('bloom_false_positive', 'bloom filter false positives', False),
    JoinStat('bloom_insert', 'items inserted into a bloom filter', False),
    JoinStat('iterated', 'items iterated', False),
    JoinStat('main_access', 'accesses to the main table', False),
    JoinStat('membership_check', 'checks that conditions of membership are satisfied', False),
]

join_stats = sorted(join_stats, key=attrgetter('desc'))

##########################################
# Session statistics
##########################################
session_stats = [
    SessionStat('bytes_read', 'bytes read into cache', False),
    SessionStat('bytes_write', 'bytes written from cache', False),
    SessionStat('cache_time', 'time waiting for cache (usecs)', False),
    SessionStat('lock_dhandle_wait', 'dhandle lock wait time (usecs)', False),
    SessionStat('lock_schema_wait', 'schema lock wait time (usecs)', False),
    SessionStat('txn_bytes_dirty', 'dirty bytes in this txn', False),
    SessionStat('read_time', 'page read from disk to cache time (usecs)', False),
    SessionStat('write_time', 'page write from cache to disk time (usecs)', False),
]

session_stats = sorted(session_stats, key=attrgetter('desc'))
