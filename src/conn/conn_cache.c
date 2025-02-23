/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#define WT_CONFIG_DEBUG(session, fmt, ...)                                 \
    if (FLD_ISSET(S2C(session)->debug_flags, WT_CONN_DEBUG_CONFIGURATION)) \
        __wt_verbose_warning(session, WT_VERB_CONFIGURATION, fmt, __VA_ARGS__);

/*
 * __cache_config_abs_to_pct --
 *     Cache configuration values can be either a percentage or an absolute size, this function
 *     converts an absolute size to a percentage.
 */
static WT_INLINE int
__cache_config_abs_to_pct(
  WT_SESSION_IMPL *session, double *param, const char *param_name, bool shared)
{
    WT_CONNECTION_IMPL *conn;
    double input;

    conn = S2C(session);

    WT_ASSERT(session, param != NULL);
    input = *param;

    /*
     * Anything above 100 is an absolute value; convert it to percentage.
     */
    if (input > 100.0) {
        /*
         * In a shared cache configuration the cache size changes regularly. Therefore, we require a
         * percentage setting and do not allow an absolute size setting.
         */
        if (shared)
            WT_RET_MSG(session, EINVAL,
              "Shared cache configuration requires a percentage value for %s", param_name);
        /* An absolute value can't exceed the cache size. */
        if (input > conn->cache_size)
            WT_RET_MSG(session, EINVAL, "%s should not exceed cache size", param_name);

        *param = (input * 100.0) / (conn->cache_size);
    }

    return (0);
}

/*
 * __validate_cache_config --
 *     Validate trigger and target values of given configs.
 */
static int
__validate_cache_config(WT_SESSION_IMPL *session, const char *cfg[], bool shared)
{
    WT_CACHE *cache;
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);
    cache = conn->cache;

    /* Debug flags are not yet set when this function runs during connection open. Set it now. */
    WT_RET(__wt_config_gets(session, cfg, "debug_mode.configuration", &cval));
    if (cval.val)
        FLD_SET(conn->debug_flags, WT_CONN_DEBUG_CONFIGURATION);
    else
        FLD_CLR(conn->debug_flags, WT_CONN_DEBUG_CONFIGURATION);

    /*
     * If not using a shared cache configure the cache size, otherwise check for a reserved size.
     * All other settings are independent of whether we are using a shared cache or not.
     */
    if (!shared) {
        WT_RET(__wt_config_gets(session, cfg, "cache_size", &cval));
        conn->cache_size = (uint64_t)cval.val;
    }

    WT_RET(__wt_config_gets(session, cfg, "eviction_target", &cval));
    cache->eviction_target = (double)cval.val;
    WT_RET(
      __cache_config_abs_to_pct(session, &(cache->eviction_target), "eviction target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_trigger", &cval));
    cache->eviction_trigger = (double)cval.val;
    WT_RET(
      __cache_config_abs_to_pct(session, &(cache->eviction_trigger), "eviction trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_dirty_target", &cval));
    cache->eviction_dirty_target = (double)cval.val;
    WT_RET(__cache_config_abs_to_pct(
      session, &(cache->eviction_dirty_target), "eviction dirty target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_dirty_trigger", &cval));
    cache->eviction_dirty_trigger = (double)cval.val;
    WT_RET(__cache_config_abs_to_pct(
      session, &(cache->eviction_dirty_trigger), "eviction dirty trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_updates_target", &cval));
    cache->eviction_updates_target = (double)cval.val;
    WT_RET(__cache_config_abs_to_pct(
      session, &(cache->eviction_updates_target), "eviction updates target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_updates_trigger", &cval));
    cache->eviction_updates_trigger = (double)cval.val;
    WT_RET(__cache_config_abs_to_pct(
      session, &(cache->eviction_updates_trigger), "eviction updates trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_checkpoint_target", &cval));
    cache->eviction_checkpoint_target = (double)cval.val;
    WT_RET(__cache_config_abs_to_pct(
      session, &(cache->eviction_checkpoint_target), "eviction checkpoint target", shared));

    /* Check for invalid configurations and automatically fix them to suitable values. */
    if (cache->eviction_dirty_target > cache->eviction_target) {
        WT_CONFIG_DEBUG(session,
          "config eviction_dirty_target=%f cannot exceed eviction_target=%f. Setting "
          "eviction_dirty_target to %f.",
          cache->eviction_dirty_target, cache->eviction_target, cache->eviction_target);
        cache->eviction_dirty_target = cache->eviction_target;
    }

    if (cache->eviction_checkpoint_target > 0 &&
      cache->eviction_checkpoint_target < cache->eviction_dirty_target) {
        WT_CONFIG_DEBUG(session,
          "config eviction_checkpoint_target=%f cannot be less than eviction_dirty_target=%f. "
          "Setting "
          "eviction_checkpoint_target to %f.",
          cache->eviction_checkpoint_target, cache->eviction_dirty_target,
          cache->eviction_dirty_target);
        cache->eviction_checkpoint_target = cache->eviction_dirty_target;
    }

    if (cache->eviction_dirty_trigger > cache->eviction_trigger) {
        WT_CONFIG_DEBUG(session,
          "config eviction_dirty_trigger=%f cannot exceed eviction_trigger=%f. Setting "
          "eviction_dirty_trigger to %f.",
          cache->eviction_dirty_trigger, cache->eviction_trigger, cache->eviction_trigger);
        cache->eviction_dirty_trigger = cache->eviction_trigger;
    }

    if (cache->eviction_updates_target < DBL_EPSILON) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_target (%f) cannot be zero. Setting "
          "to 50%% of eviction_updates_target (%f).",
          cache->eviction_updates_target, cache->eviction_dirty_target / 2);
        cache->eviction_updates_target = cache->eviction_dirty_target / 2;
    }

    if (cache->eviction_updates_trigger < DBL_EPSILON) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_trigger (%f) cannot be zero. Setting "
          "to 50%% of eviction_updates_trigger (%f).",
          cache->eviction_updates_trigger, cache->eviction_dirty_trigger / 2);
        cache->eviction_updates_trigger = cache->eviction_dirty_trigger / 2;
    }

    /* Don't allow the trigger to be larger than the overall trigger. */
    if (cache->eviction_updates_trigger > cache->eviction_trigger) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_trigger=%f cannot exceed eviction_trigger=%f. Setting "
          "eviction_updates_trigger to %f.",
          cache->eviction_updates_trigger, cache->eviction_trigger, cache->eviction_trigger);
        cache->eviction_updates_trigger = cache->eviction_trigger;
    }

    /* The target size must be lower than the trigger size or we will never get any work done. */
    if (cache->eviction_target >= cache->eviction_trigger)
        WT_RET_MSG(session, EINVAL, "eviction target must be lower than the eviction trigger");
    if (cache->eviction_dirty_target >= cache->eviction_dirty_trigger)
        WT_RET_MSG(
          session, EINVAL, "eviction dirty target must be lower than the eviction dirty trigger");
    if (cache->eviction_updates_target >= cache->eviction_updates_trigger)
        WT_RET_MSG(session, EINVAL,
          "eviction updates target must be lower than the eviction updates trigger");

    return (0);
}

/*
 * __cache_config_local --
 *     Configure the underlying cache.
 */
static int
__cache_config_local(WT_SESSION_IMPL *session, bool shared, const char *cfg[])
{
    WT_CACHE *cache;
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    uint32_t evict_threads_max, evict_threads_min;

    conn = S2C(session);
    cache = conn->cache;

    WT_RET(__validate_cache_config(session, cfg, shared));

    /* Set config values as percentages. */
    WT_RET(__wt_config_gets(session, cfg, "cache_overhead", &cval));
    cache->overhead_pct = (u_int)cval.val;

    WT_RET(__wt_config_gets(session, cfg, "eviction.threads_max", &cval));
    WT_ASSERT(session, cval.val > 0);
    evict_threads_max = (uint32_t)cval.val;

    WT_RET(__wt_config_gets(session, cfg, "eviction.threads_min", &cval));
    WT_ASSERT(session, cval.val > 0);
    evict_threads_min = (uint32_t)cval.val;

    if (evict_threads_min > evict_threads_max)
        WT_RET_MSG(
          session, EINVAL, "eviction=(threads_min) cannot be greater than eviction=(threads_max)");
    conn->evict_threads_max = evict_threads_max;
    conn->evict_threads_min = evict_threads_min;

    WT_RET(__wt_config_gets(session, cfg, "eviction.evict_sample_inmem", &cval));
    conn->evict_sample_inmem = cval.val != 0;

    /* Retrieve the wait time and convert from milliseconds */
    WT_RET(__wt_config_gets(session, cfg, "cache_max_wait_ms", &cval));
    cache->cache_max_wait_us = (uint64_t)(cval.val * WT_THOUSAND);

    /* Retrieve the timeout value and convert from seconds */
    WT_RET(__wt_config_gets(session, cfg, "cache_stuck_timeout_ms", &cval));
    cache->cache_stuck_timeout_ms = (uint64_t)cval.val;

    return (0);
}

/*
 * __wti_cache_config --
 *     Configure or reconfigure the current cache and shared cache.
 */
int
__wti_cache_config(WT_SESSION_IMPL *session, const char *cfg[], bool reconfig)
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    bool now_shared, was_shared;

    conn = S2C(session);

    WT_ASSERT(session, conn->cache != NULL);

    WT_RET(__wt_config_gets_none(session, cfg, "shared_cache.name", &cval));
    now_shared = cval.len != 0;
    was_shared = F_ISSET(conn, WT_CONN_CACHE_POOL);

    /* Cleanup if reconfiguring */
    if (reconfig && was_shared && !now_shared)
        /* Remove ourselves from the pool if necessary */
        WT_RET(__wti_conn_cache_pool_destroy(session));
    else if (reconfig && !was_shared && now_shared)
        /*
         * Cache size will now be managed by the cache pool - the start size always needs to be zero
         * to allow the pool to manage how much memory is in-use.
         */
        conn->cache_size = 0;

    /*
     * Always setup the local cache - it's used even if we are participating in a shared cache.
     */
    WT_RET(__cache_config_local(session, now_shared, cfg));
    if (now_shared) {
        WT_RET(__wti_cache_pool_config(session, cfg));
        WT_ASSERT(session, F_ISSET(conn, WT_CONN_CACHE_POOL));
        if (!was_shared)
            WT_RET(__wti_conn_cache_pool_open(session));
    }

    /*
     * Resize the thread group if reconfiguring, otherwise the thread group will be initialized as
     * part of creating the cache.
     */
    if (reconfig)
        WT_RET(__wt_thread_group_resize(session, &conn->evict_threads, conn->evict_threads_min,
          conn->evict_threads_max, WT_THREAD_CAN_WAIT | WT_THREAD_PANIC_FAIL));

    return (0);
}

/*
 * __wti_cache_create --
 *     Create the underlying cache.
 */
int
__wti_cache_create(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    int i;

    conn = S2C(session);

    WT_ASSERT(session, conn->cache == NULL);

    WT_RET(__wt_calloc_one(session, &conn->cache));

    cache = conn->cache;

    /* Use a common routine for run-time configuration options. */
    WT_RET(__wti_cache_config(session, cfg, false));

    /*
     * The lowest possible page read-generation has a special meaning, it marks a page for forcible
     * eviction; don't let it happen by accident.
     */
    cache->read_gen_oldest = WT_READGEN_START_VALUE;
    __wt_atomic_store64(&cache->read_gen, WT_READGEN_START_VALUE);

    WT_RET(__wt_cond_auto_alloc(
      session, "cache eviction server", 10 * WT_THOUSAND, WT_MILLION, &cache->evict_cond));
    WT_RET(__wt_spin_init(session, &cache->evict_pass_lock, "evict pass"));
    WT_RET(__wt_spin_init(session, &cache->evict_queue_lock, "cache eviction queue"));
    WT_RET(__wt_spin_init(session, &cache->evict_walk_lock, "cache walk"));
    if ((ret = __wt_open_internal_session(
           conn, "evict pass", false, WT_SESSION_NO_DATA_HANDLES, 0, &cache->walk_session)) != 0)
        WT_RET_MSG(NULL, ret, "Failed to create session for eviction walks");

    /* Allocate the LRU eviction queue. */
    cache->evict_slots = WT_EVICT_WALK_BASE + WT_EVICT_WALK_INCR;
    for (i = 0; i < WT_EVICT_QUEUE_MAX; ++i) {
        WT_RET(__wt_calloc_def(session, cache->evict_slots, &cache->evict_queues[i].evict_queue));
        WT_RET(__wt_spin_init(session, &cache->evict_queues[i].evict_lock, "cache eviction"));
    }

    /* Ensure there are always non-NULL queues. */
    cache->evict_current_queue = cache->evict_fill_queue = &cache->evict_queues[0];
    cache->evict_other_queue = &cache->evict_queues[1];
    cache->evict_urgent_queue = &cache->evict_queues[WT_EVICT_URGENT_QUEUE];

    /*
     * We get/set some values in the cache statistics (rather than have two copies), configure them.
     */
    __wti_cache_stats_update(session);
    return (0);
}

/*
 * __wti_cache_stats_update --
 *     Update the cache statistics for return to the application.
 */
void
__wti_cache_stats_update(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_CONNECTION_STATS **stats;
    uint64_t intl, inuse, leaf;

    conn = S2C(session);
    cache = conn->cache;
    stats = conn->stats;

    inuse = __wt_cache_bytes_inuse(cache);
    intl = __wt_cache_bytes_plus_overhead(cache, __wt_atomic_load64(&cache->bytes_internal));
    /*
     * There are races updating the different cache tracking values so be paranoid calculating the
     * leaf byte usage.
     */
    leaf = inuse > intl ? inuse - intl : 0;

    WT_STATP_CONN_SET(session, stats, cache_bytes_max, conn->cache_size);
    WT_STATP_CONN_SET(session, stats, cache_bytes_inuse, inuse);
    WT_STATP_CONN_SET(session, stats, cache_overhead, cache->overhead_pct);

    WT_STATP_CONN_SET(session, stats, cache_bytes_dirty, __wt_cache_dirty_inuse(cache));
    WT_STATP_CONN_SET(session, stats, cache_bytes_dirty_total,
      __wt_cache_bytes_plus_overhead(cache, __wt_atomic_load64(&cache->bytes_dirty_total)));
    WT_STATP_CONN_SET(session, stats, cache_bytes_hs,
      __wt_cache_bytes_plus_overhead(cache, __wt_atomic_load64(&cache->bytes_hs)));
    WT_STATP_CONN_SET(session, stats, cache_bytes_image, __wt_cache_bytes_image(cache));
    WT_STATP_CONN_SET(session, stats, cache_pages_inuse, __wt_cache_pages_inuse(cache));
    WT_STATP_CONN_SET(session, stats, cache_bytes_internal, intl);
    WT_STATP_CONN_SET(session, stats, cache_bytes_leaf, leaf);
    WT_STATP_CONN_SET(session, stats, cache_bytes_other, __wt_cache_bytes_other(cache));
    WT_STATP_CONN_SET(session, stats, cache_bytes_updates, __wt_cache_bytes_updates(cache));

    WT_STATP_CONN_SET(session, stats, cache_eviction_maximum_page_size,
      __wt_atomic_load64(&cache->evict_max_page_size));
    WT_STATP_CONN_SET(session, stats, cache_eviction_maximum_milliseconds,
      __wt_atomic_load64(&cache->evict_max_ms));
    WT_STATP_CONN_SET(
      session, stats, cache_reentry_hs_eviction_milliseconds, cache->reentry_hs_eviction_ms);
    WT_STATP_CONN_SET(
      session, stats, cache_pages_dirty, cache->pages_dirty_intl + cache->pages_dirty_leaf);

    WT_STATP_CONN_SET(session, stats, cache_eviction_state, __wt_atomic_load32(&cache->flags));
    WT_STATP_CONN_SET(session, stats, cache_eviction_aggressive_set, cache->evict_aggressive_score);
    WT_STATP_CONN_SET(session, stats, cache_eviction_empty_score, cache->evict_empty_score);

    WT_STATP_CONN_SET(session, stats, cache_eviction_active_workers,
      __wt_atomic_load32(&conn->evict_threads.current_threads));
    WT_STATP_CONN_SET(
      session, stats, cache_eviction_stable_state_workers, cache->evict_tune_workers_best);

    /*
     * The number of files with active walks ~= number of hazard pointers in the walk session. Note:
     * reading without locking.
     */
    if (__wt_atomic_loadbool(&conn->evict_server_running))
        WT_STATP_CONN_SET(
          session, stats, cache_eviction_walks_active, cache->walk_session->hazards.num_active);

    WT_STATP_CONN_SET(
      session, stats, rec_maximum_hs_wrapup_milliseconds, conn->rec_maximum_hs_wrapup_milliseconds);
    WT_STATP_CONN_SET(session, stats, rec_maximum_image_build_milliseconds,
      conn->rec_maximum_image_build_milliseconds);
    WT_STATP_CONN_SET(session, stats, rec_maximum_milliseconds, conn->rec_maximum_milliseconds);
}

/*
 * __wti_cache_destroy --
 *     Discard the underlying cache.
 */
int
__wti_cache_destroy(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    int i;

    conn = S2C(session);
    cache = conn->cache;

    if (cache == NULL)
        return (0);

    /* The cache should be empty at this point.  Complain if not. */
    if (cache->pages_inmem != cache->pages_evicted)
        __wt_errx(session,
          "cache server: exiting with %" PRIu64 " pages in memory and %" PRIu64 " pages evicted",
          cache->pages_inmem, cache->pages_evicted);
    if ((__wt_atomic_load64(&cache->bytes_image_intl) +
          __wt_atomic_load64(&cache->bytes_image_leaf)) != 0)
        __wt_errx(session, "cache server: exiting with %" PRIu64 " image bytes in memory",
          __wt_atomic_load64(&cache->bytes_image_intl) +
            __wt_atomic_load64(&cache->bytes_image_leaf));
    if (__wt_atomic_load64(&cache->bytes_inmem) != 0)
        __wt_errx(session, "cache server: exiting with %" PRIu64 " bytes in memory",
          __wt_atomic_load64(&cache->bytes_inmem));
    if ((__wt_atomic_load64(&cache->bytes_dirty_intl) +
          __wt_atomic_load64(&cache->bytes_dirty_leaf)) != 0 ||
      cache->pages_dirty_intl + cache->pages_dirty_leaf != 0)
        __wt_errx(session,
          "cache server: exiting with %" PRIu64 " bytes dirty and %" PRIu64 " pages dirty",
          __wt_atomic_load64(&cache->bytes_dirty_intl) +
            __wt_atomic_load64(&cache->bytes_dirty_leaf),
          cache->pages_dirty_intl + cache->pages_dirty_leaf);

    __wt_cond_destroy(session, &cache->evict_cond);
    __wt_spin_destroy(session, &cache->evict_pass_lock);
    __wt_spin_destroy(session, &cache->evict_queue_lock);
    __wt_spin_destroy(session, &cache->evict_walk_lock);
    if (cache->walk_session != NULL)
        WT_TRET(__wt_session_close_internal(cache->walk_session));

    for (i = 0; i < WT_EVICT_QUEUE_MAX; ++i) {
        __wt_spin_destroy(session, &cache->evict_queues[i].evict_lock);
        __wt_free(session, cache->evict_queues[i].evict_queue);
    }

    __wt_free(session, conn->cache);
    return (ret);
}
