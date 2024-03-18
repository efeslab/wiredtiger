/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once

#ifdef HAVE_X86INTRIN_H
#if !defined(_MSC_VER) && !defined(_lint)
#include <x86intrin.h>
#endif
#endif

#define WT_VECTOR_SIZE 16 /* chunk size */

/*
 * __lex_compare_ge_16 --
 *     Lexicographic comparison routine for data greater than or equal to 16 bytes. Returns: < 0 if
 *     user_item is lexicographically < tree_item, = 0 if user_item is lexicographically =
 *     tree_item, > 0 if user_item is lexicographically > tree_item. We use the names "user" and
 *     "tree" so it's clear in the btree code which the application is looking at when we call its
 *     comparison function. Some platforms like ARM offer dedicated instructions for reading 16
 *     bytes at a time, allowing for faster comparisons.
 */
static WT_INLINE int
__lex_compare_ge_16(const uint8_t *ustartp, const uint8_t *tstartp, size_t len, int lencmp)
{
#ifdef HAVE_X86INTRIN_H
    __m128i res_eq, t, u;
    uint64_t tfirst, ufirst;
#else
    struct {
        uint64_t a, b;
    } udata, tdata;
#endif
    uint64_t u64, t64;
    const uint8_t *uendp, *userp, *tendp, *treep;
    bool firsteq;

    uendp = ustartp + len;
    tendp = tstartp + len;

    /* skip 16 matching bytes at a time, starting at first possible difference. */
    for (userp = ustartp, treep = tstartp; uendp - userp > WT_VECTOR_SIZE;
         userp += WT_VECTOR_SIZE, treep += WT_VECTOR_SIZE) {
#ifdef HAVE_X86INTRIN_H
        u = _mm_loadu_si128((const __m128i *)userp);
        t = _mm_loadu_si128((const __m128i *)treep);
        res_eq = _mm_cmpeq_epi8(u, t);
        if (_mm_movemask_epi8(res_eq) != 65535)
            goto final128;
#else
        memcpy(&udata, userp, WT_VECTOR_SIZE);
        memcpy(&tdata, treep, WT_VECTOR_SIZE);
        if (udata.a != tdata.a || udata.b != tdata.b)
            goto final128;
#endif
    }

    /*
     * Rewind until there is exactly 16 bytes left. We know we started with at least 16, so we are
     * still in bound.
     */
#ifdef HAVE_X86INTRIN_H
    u = _mm_loadu_si128((const __m128i *)(uendp - WT_VECTOR_SIZE));
    t = _mm_loadu_si128((const __m128i *)(tendp - WT_VECTOR_SIZE));
#else
    memcpy(&udata, uendp - WT_VECTOR_SIZE, WT_VECTOR_SIZE);
    memcpy(&tdata, tendp - WT_VECTOR_SIZE, WT_VECTOR_SIZE);
#endif

final128:
#ifdef HAVE_X86INTRIN_H
    ufirst = (uint64_t)_mm_extract_epi64(u, 0);
    tfirst = (uint64_t)_mm_extract_epi64(t, 0);
    firsteq = ufirst == tfirst;
    u64 = firsteq ? (uint64_t)_mm_extract_epi64(u, 1) : ufirst;
    t64 = firsteq ? (uint64_t)_mm_extract_epi64(t, 1) : tfirst;
#else
    firsteq = udata.a == tdata.a;
    u64 = firsteq ? udata.b : udata.a;
    t64 = firsteq ? tdata.b : tdata.a;
#endif

#ifndef WORDS_BIGENDIAN
    u64 = __wt_bswap64(u64);
    t64 = __wt_bswap64(t64);
#endif

    return (u64 < t64 ? -1 : u64 > t64 ? 1 : lencmp);
}

/*
 * __lex_compare_lt_16 --
 *     Lexicographic comparison routine for data less than 16 bytes. Returns: < 0 if user_item is
 *     lexicographically < tree_item, = 0 if user_item is lexicographically = tree_item, > 0 if
 *     user_item is lexicographically > tree_item. We use the names "user" and "tree" so it's clear
 *     in the btree code which the application is looking at when we call its comparison function.
 *     Some platforms like ARM offer dedicated instructions for reading 16 bytes at a time, allowing
 *     for faster comparisons.
 */
static WT_INLINE int
__lex_compare_lt_16(const uint8_t *ustartp, const uint8_t *tstartp, size_t len, int lencmp)
{
    uint64_t ta, tb, ua, ub, u64, t64;
    const uint8_t *tendp, *uendp;

    uendp = ustartp + len;
    tendp = tstartp + len;
    if (len & sizeof(uint64_t)) {
        /*
         * len >= 64 bits. len is implicitly less than 128bits since the function accepts 16 bytes
         * or less.
         */
        memcpy(&ua, ustartp, sizeof(uint64_t));
        memcpy(&ta, tstartp, sizeof(uint64_t));
        memcpy(&ub, uendp - sizeof(uint64_t), sizeof(uint64_t));
        memcpy(&tb, tendp - sizeof(uint64_t), sizeof(uint64_t));
    } else if (len & sizeof(uint32_t)) {
        /* len >= 32 bits */
        uint32_t ta32, tb32, ua32, ub32;
        memcpy(&ua32, ustartp, sizeof(uint32_t));
        memcpy(&ta32, tstartp, sizeof(uint32_t));
        memcpy(&ub32, uendp - sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&tb32, tendp - sizeof(uint32_t), sizeof(uint32_t));
        ua = ua32;
        ta = ta32;
        ub = ub32;
        tb = tb32;
    } else if (len & sizeof(uint16_t)) {
        /* len >= 16 bits */
        uint16_t ta16, tb16, ua16, ub16;
        memcpy(&ua16, ustartp, sizeof(uint16_t));
        memcpy(&ta16, tstartp, sizeof(uint16_t));
        memcpy(&ub16, uendp - sizeof(uint16_t), sizeof(uint16_t));
        memcpy(&tb16, tendp - sizeof(uint16_t), sizeof(uint16_t));
        ua = ua16;
        ta = ta16;
        ub = ub16;
        tb = tb16;
    } else if (len & sizeof(uint8_t)) {
        uint8_t ta8, ua8;
        memcpy(&ua8, ustartp, sizeof(uint8_t));
        memcpy(&ta8, tstartp, sizeof(uint8_t));
        return (ua8 < ta8 ? -1 : ua8 > ta8 ? 1 : lencmp);
    } else
        return (lencmp);

    u64 = ua == ta ? ub : ua;
    t64 = ua == ta ? tb : ta;

#ifndef WORDS_BIGENDIAN
    u64 = __wt_bswap64(u64);
    t64 = __wt_bswap64(t64);
#endif

    return (u64 < t64 ? -1 : u64 > t64 ? 1 : lencmp);
}

/*
 * __wt_lex_compare --
 *     Lexicographic comparison routine. Returns: < 0 if user_item is lexicographically < tree_item,
 *     = 0 if user_item is lexicographically = tree_item, > 0 if user_item is lexicographically >
 *     tree_item. We use the names "user" and "tree" so it's clear in the btree code which the
 *     application is looking at when we call its comparison function.
 */
static WT_INLINE int
__wt_lex_compare(const WT_ITEM *user_item, const WT_ITEM *tree_item)
{
    size_t len, usz, tsz;
    int lencmp, ret_val;

    usz = user_item->size;
    tsz = tree_item->size;
    if (usz < tsz) {
        len = usz;
        lencmp = -1;
    } else if (usz > tsz) {
        len = tsz;
        lencmp = 1;
    } else {
        len = usz;
        lencmp = 0;
    }

    if (len >= WT_VECTOR_SIZE)
        ret_val = __lex_compare_ge_16(
          (const uint8_t *)user_item->data, (const uint8_t *)tree_item->data, len, lencmp);
    else
        ret_val = __lex_compare_lt_16(
          (const uint8_t *)user_item->data, (const uint8_t *)tree_item->data, len, lencmp);

    return (ret_val);
}

/*
 * __wt_compare --
 *     The same as __wt_lex_compare, but using the application's collator function when configured.
 */
static WT_INLINE int
__wt_compare(WT_SESSION_IMPL *session, WT_COLLATOR *collator, const WT_ITEM *user_item,
  const WT_ITEM *tree_item, int *cmpp)
{
    if (collator == NULL) {
        *cmpp = __wt_lex_compare(user_item, tree_item);
        return (0);
    }
    return (collator->compare(collator, &session->iface, user_item, tree_item, cmpp));
}

/*
 * __wt_compare_bounds --
 *     Return if the cursor key is within the bounded range. If upper is True, this indicates a next
 *     call and the key is checked against the upper bound. If upper is False, this indicates a prev
 *     call and the key is then checked against the lower bound.
 */
static WT_INLINE int
__wt_compare_bounds(WT_SESSION_IMPL *session, WT_CURSOR *cursor, WT_ITEM *key, uint64_t recno,
  bool upper, bool *key_out_of_bounds)
{
    uint64_t recno_bound;
    int cmpp;

    cmpp = 0;
    recno_bound = 0;

    WT_STAT_CONN_DATA_INCR(session, cursor_bounds_comparisons);

    if (upper) {
        WT_ASSERT(session, WT_DATA_IN_ITEM(&cursor->upper_bound));
        if (CUR2BT(cursor)->type == BTREE_ROW)
            WT_RET(
              __wt_compare(session, CUR2BT(cursor)->collator, key, &cursor->upper_bound, &cmpp));
        else
            /* Unpack the raw recno buffer into integer variable. */
            WT_RET(__wt_struct_unpack(
              session, cursor->upper_bound.data, cursor->upper_bound.size, "q", &recno_bound));

        if (F_ISSET(cursor, WT_CURSTD_BOUND_UPPER_INCLUSIVE))
            *key_out_of_bounds =
              CUR2BT(cursor)->type == BTREE_ROW ? (cmpp > 0) : (recno > recno_bound);
        else
            *key_out_of_bounds =
              CUR2BT(cursor)->type == BTREE_ROW ? (cmpp >= 0) : (recno >= recno_bound);
    } else {
        WT_ASSERT(session, WT_DATA_IN_ITEM(&cursor->lower_bound));
        if (CUR2BT(cursor)->type == BTREE_ROW)
            WT_RET(
              __wt_compare(session, CUR2BT(cursor)->collator, key, &cursor->lower_bound, &cmpp));
        else
            /* Unpack the raw recno buffer into integer variable. */
            WT_RET(__wt_struct_unpack(
              session, cursor->lower_bound.data, cursor->lower_bound.size, "q", &recno_bound));

        if (F_ISSET(cursor, WT_CURSTD_BOUND_LOWER_INCLUSIVE))
            *key_out_of_bounds =
              CUR2BT(cursor)->type == BTREE_ROW ? (cmpp < 0) : (recno < recno_bound);
        else
            *key_out_of_bounds =
              CUR2BT(cursor)->type == BTREE_ROW ? (cmpp <= 0) : (recno <= recno_bound);
    }
    return (0);
}

/*
 * __lex_compare_skip_ge_16 --
 *     Lexicographic comparison routine for data greater than or equal to 16 bytes, skipping leading
 *     bytes. Returns: < 0 if user_item is lexicographically < tree_item = 0 if user_item is
 *     lexicographically = tree_item > 0 if user_item is lexicographically > tree_item We use the
 *     names "user" and "tree" so it's clear in the btree code which the application is looking at
 *     when we call its comparison function. Some platforms like ARM offer dedicated instructions
 *     for reading 16 bytes at a time, allowing for faster comparisons. Some platforms like ARM
 *     offer dedicated instructions for reading 16 bytes at a time, allowing for faster comparisons.
 */
static WT_INLINE int
__lex_compare_skip_ge_16(
  const uint8_t *ustartp, const uint8_t *tstartp, size_t len, int lencmp, size_t *matchp)
{
#ifdef HAVE_X86INTRIN_H
    __m128i res_eq, t, u;
    uint64_t tfirst, ufirst;
#else
    struct {
        uint64_t a, b;
    } udata, tdata;
#endif
    size_t match;
    uint64_t u64, t64;
    const uint8_t *uendp, *userp, *tendp, *treep;
    bool firsteq;

    match = *matchp;
    uendp = ustartp + len;
    tendp = tstartp + len;

    /* skip 16 matching bytes at a time, starting at first possible difference. */
    for (userp = ustartp + match, treep = tstartp + match; uendp - userp > WT_VECTOR_SIZE;
         userp += WT_VECTOR_SIZE, treep += WT_VECTOR_SIZE) {
#ifdef HAVE_X86INTRIN_H
        u = _mm_loadu_si128((const __m128i *)userp);
        t = _mm_loadu_si128((const __m128i *)treep);
        res_eq = _mm_cmpeq_epi8(u, t);
        if (_mm_movemask_epi8(res_eq) != 65535) {
            match = (size_t)(userp - ustartp);
            goto final128;
        }
#else
        memcpy(&udata, userp, WT_VECTOR_SIZE);
        memcpy(&tdata, treep, WT_VECTOR_SIZE);
        if (udata.a != tdata.a || udata.b != tdata.b) {
            match = (size_t)(userp - ustartp);
            goto final128;
        }
#endif
    }

    /*
     * Rewind until there is exactly 16 bytes left. We know we started with at least 16, so we are
     * still in bound.
     */
#ifdef HAVE_X86INTRIN_H
    u = _mm_loadu_si128((const __m128i *)(uendp - WT_VECTOR_SIZE));
    t = _mm_loadu_si128((const __m128i *)(tendp - WT_VECTOR_SIZE));
#else
    memcpy(&udata, uendp - WT_VECTOR_SIZE, WT_VECTOR_SIZE);
    memcpy(&tdata, tendp - WT_VECTOR_SIZE, WT_VECTOR_SIZE);
#endif
    match = (size_t)(uendp - ustartp) - WT_VECTOR_SIZE;

final128:
#ifdef HAVE_X86INTRIN_H
    ufirst = (uint64_t)_mm_extract_epi64(u, 0);
    tfirst = (uint64_t)_mm_extract_epi64(t, 0);
    firsteq = ufirst == tfirst;
    u64 = firsteq ? (uint64_t)_mm_extract_epi64(u, 1) : ufirst;
    t64 = firsteq ? (uint64_t)_mm_extract_epi64(t, 1) : tfirst;
#else
    firsteq = udata.a == tdata.a;
    u64 = firsteq ? udata.b : udata.a;
    t64 = firsteq ? tdata.b : tdata.a;
#endif
    match += firsteq ? sizeof(uint64_t) : 0;

#ifndef WORDS_BIGENDIAN
    u64 = __wt_bswap64(u64);
    t64 = __wt_bswap64(t64);
#endif

    match += (size_t)__builtin_clzll(u64 ^ t64) / 8;
    *matchp = match;

    return (u64 < t64 ? -1 : u64 > t64 ? 1 : lencmp);
}

/*
 * __wt_lex_compare_skip --
 *     Lexicographic comparison routine, skipping leading bytes. Returns: < 0 if user_item is
 *     lexicographically < tree_item = 0 if user_item is lexicographically = tree_item > 0 if
 *     user_item is lexicographically > tree_item We use the names "user" and "tree" so it's clear
 *     in the btree code which the application is looking at when we call its comparison function.
 */
static WT_INLINE int
__wt_lex_compare_skip(
  WT_SESSION_IMPL *session, const WT_ITEM *user_item, const WT_ITEM *tree_item, size_t *matchp)
{
    size_t len, usz, tsz;
    int lencmp, ret_val;

    usz = user_item->size;
    tsz = tree_item->size;
    if (usz < tsz) {
        len = usz;
        lencmp = -1;
    } else if (usz > tsz) {
        len = tsz;
        lencmp = 1;
    } else {
        len = usz;
        lencmp = 0;
    }

    if (len >= WT_VECTOR_SIZE) {
        ret_val = __lex_compare_skip_ge_16(
          (const uint8_t *)user_item->data, (const uint8_t *)tree_item->data, len, lencmp, matchp);

#ifdef HAVE_DIAGNOSTIC
        /*
         * There are various optimizations in the code to skip comparing prefixes that are known to
         * be the same. If configured, check that the prefixes actually match.
         */
        if (FLD_ISSET(S2C(session)->timing_stress_flags, WT_TIMING_STRESS_PREFIX_COMPARE)) {
            int full_cmp_ret;
            full_cmp_ret = __wt_lex_compare(user_item, tree_item);
            WT_ASSERT_ALWAYS(session, full_cmp_ret == ret_val,
              "Comparison that skipped prefix returned different result than a full comparison");
        }
#else
        WT_UNUSED(session);
#endif
    } else
        /*
         * We completely ignore match when len < 16 because it wouldn't reduce the amount of work
         * done, and would add overhead.
         */
        ret_val = __lex_compare_lt_16(
          (const uint8_t *)user_item->data, (const uint8_t *)tree_item->data, len, lencmp);

    return (ret_val);
}

/*
 * __wt_compare_skip --
 *     The same as __wt_lex_compare_skip, but using the application's collator function when
 *     configured.
 */
static WT_INLINE int
__wt_compare_skip(WT_SESSION_IMPL *session, WT_COLLATOR *collator, const WT_ITEM *user_item,
  const WT_ITEM *tree_item, int *cmpp, size_t *matchp)
{
    if (collator == NULL) {
        *cmpp = __wt_lex_compare_skip(session, user_item, tree_item, matchp);
        return (0);
    }
    return (collator->compare(collator, &session->iface, user_item, tree_item, cmpp));
}

/*
 * __wt_lex_compare_short --
 *     Lexicographic comparison routine for short keys. Returns: < 0 if user_item is
 *     lexicographically < tree_item = 0 if user_item is lexicographically = tree_item > 0 if
 *     user_item is lexicographically > tree_item We use the names "user" and "tree" so it's clear
 *     in the btree code which the application is looking at when we call its comparison function.
 */
static WT_INLINE int
__wt_lex_compare_short(const WT_ITEM *user_item, const WT_ITEM *tree_item)
{
    size_t len, usz, tsz;
    const uint8_t *userp, *treep;

    usz = user_item->size;
    tsz = tree_item->size;
    len = WT_MIN(usz, tsz);

    userp = (const uint8_t *)user_item->data;
    treep = (const uint8_t *)tree_item->data;

/*
 * The maximum packed uint64_t is 9B, catch row-store objects using packed record numbers as keys.
 *
 * Don't use a #define to compress this case statement: gcc7 complains about implicit fallthrough
 * and doesn't support explicit fallthrough comments in macros.
 */
#define WT_COMPARE_SHORT_MAXLEN 9
    switch (len) {
    case 9:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 8:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 7:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 6:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 5:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 4:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 3:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 2:
        if (*userp != *treep)
            break;
        ++userp;
        ++treep;
    /* FALLTHROUGH */
    case 1:
        if (*userp != *treep)
            break;

        /* Contents are equal up to the smallest length. */
        return ((usz == tsz) ? 0 : (usz < tsz) ? -1 : 1);
    }
    return (*userp < *treep ? -1 : 1);
}
