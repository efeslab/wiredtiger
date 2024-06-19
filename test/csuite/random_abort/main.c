/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "test_util.h"

#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

static char home[PATH_MAX]; /* Program working dir */

/*
 * These two names for the URI and file system must be maintained in tandem.
 */
static const char *const col_uri = "table:col_main";
static const char *const uri = "table:main";
static bool compaction;
static bool compat;
static bool inmem;
static bool use_lazyfs;

static WT_LAZY_FS lazyfs;

#define MAX_TH 12
#define MIN_TH 5
#define MAX_TIME 40
#define MIN_TIME 10

#define OP_TYPE_DELETE 0
#define OP_TYPE_INSERT 1
#define OP_TYPE_MODIFY 2
#define MAX_NUM_OPS 3

// #define DELETE_RECORDS_FILE RECORDS_DIR DIR_DELIM_STR "delete-records-%" PRIu32
// #define INSERT_RECORDS_FILE RECORDS_DIR DIR_DELIM_STR "insert-records-%" PRIu32
// #define MODIFY_RECORDS_FILE RECORDS_DIR DIR_DELIM_STR "modify-records-%" PRIu32

// #define DELETE_RECORD_FILE_ID 0
// #define INSERT_RECORD_FILE_ID 1
// #define MODIFY_RECORD_FILE_ID 2
// #define MAX_RECORD_FILES 3

#define ENV_CONFIG_DEF \
    "create,log=(file_max=10M,enabled),statistics=(all),statistics_log=(json,on_close,wait=1)"
#define ENV_CONFIG_TXNSYNC                                                                        \
    "create,log=(file_max=10M,enabled),"                                                          \
    "transaction_sync=(enabled,method=none),statistics=(all),statistics_log=(json,on_close,wait=" \
    "1)"
#define ENV_CONFIG_TXNSYNC_FSYNC                                                                   \
    "create,log=(file_max=10M,enabled),"                                                           \
    "transaction_sync=(enabled,method=fsync),statistics=(all),statistics_log=(json,on_close,wait=" \
    "1)"

/*
 * A minimum width of 10, along with zero filling, means that all the keys sort according to their
 * integer value, making each thread's key space distinct.
 */
#define KEY_FORMAT ("%010" PRIu64)

/*
 * Maximum number of modifications that are allowed to perform cursor modify operation.
 */
#define MAX_MODIFY_ENTRIES 10

#define MAX_VAL 4096
/*
 * STR_MAX_VAL is set to MAX_VAL - 1 to account for the extra null character.
 */
#define STR_MAX_VAL "4095"

static void handler(int) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));
static void usage(void) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));

/*
 * usage --
 *     TODO: Add a comment describing this function.
 */
static void
usage(void)
{
    fprintf(stderr, "usage: %s [-h dir] [-T threads] [-Cclmpv]\n", progname);
    exit(EXIT_FAILURE);
}

typedef struct {
    WT_CONNECTION *conn;
    uint64_t start;
    uint32_t id;
    uint64_t num_ops;
} WT_THREAD_DATA;

typedef struct WT_OPS{
    uint64_t th_id; // thread id 
    uint64_t th_op_id; // thread local operation id
    struct WT_OPS* ops;
} WT_OPS;

// log file to save logs from different threads
FILE *global_log_file;
uint64_t global_op_id = 0;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t global_op_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 * thread_run --
 *     TODO: Add a comment describing this function.
 */
static WT_THREAD_RET
thread_run(void *arg)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_ITEM data, newv;
    WT_MODIFY entries[MAX_MODIFY_ENTRIES];
    WT_RAND_STATE rnd;
    WT_SESSION *session;
    WT_THREAD_DATA *td;
    size_t lsize;
    size_t maxdiff, new_buf_size;
    uint64_t i, thread_op_id = 0;
    int nentries;
    char buf[MAX_VAL], new_buf[MAX_VAL];
    char kname[64], lgbuf[8];
    char large[128 * 1024];
    bool columnar_table;
    uint64_t local_global_op_id;

    // initialize variables and clear the buffers
    __wt_random_init(&rnd);
    memset(buf, 0, sizeof(buf));
    memset(new_buf, 0, sizeof(new_buf));
    memset(kname, 0, sizeof(kname));
    lsize = sizeof(large);
    memset(large, 0, lsize);
    nentries = MAX_MODIFY_ENTRIES;
    columnar_table = false;

    td = (WT_THREAD_DATA *)arg;

    /*
     * Set up a large value putting our id in it. Write it in there a bunch of times, but the rest
     * of the buffer can just be zero.
     */
    testutil_snprintf(lgbuf, sizeof(lgbuf), "th-%" PRIu32, td->id);
    for (i = 0; i < 128; i += strlen(lgbuf))
        testutil_snprintf(&large[i], lsize - i, "%s", lgbuf);

    testutil_check(td->conn->open_session(td->conn, NULL, NULL, &session));

    /* Make alternate threads operate on the column-store table. */
    if (td->id % 2 != 0)
        columnar_table = true;

    if (columnar_table)
        testutil_check(session->open_cursor(session, col_uri, NULL, NULL, &cursor));
    else
        testutil_check(session->open_cursor(session, uri, NULL, NULL, &cursor));

    /*
     * Write our portion of the key space until we're killed.
     */
    printf("Thread %" PRIu32 " starts at %" PRIu64 "\n", td->id, td->start);
    for (i = td->start; td->num_ops != 0 ? i < td->start + td->num_ops : 1 ; ++i) {
        if (i % 1000 == 0) {
            printf("checkpoint %lu\n", i);
        }
        /* Record number 0 is invalid for columnar store, check it. */
        if (i == 0)
            i++;

        /*
         * The value is the insert- with key appended.
         */
        testutil_snprintf(buf, sizeof(buf), "insert-%" PRIu64, i);

        if (columnar_table)
            cursor->set_key(cursor, i);
        else {
            testutil_snprintf(kname, sizeof(kname), KEY_FORMAT, i);
            cursor->set_key(cursor, kname);
        }
        /*
         * Every 30th record write a very large record that exceeds the log buffer size. This forces
         * us to use the unbuffered path.
         */
        if (i % 30 == 0) {
            data.size = 128 * 1024;
            data.data = large;
        } else {
            data.size = __wt_random(&rnd) % MAX_VAL;
            data.data = buf;
        }
        cursor->set_value(cursor, &data);
        while ((ret = cursor->insert(cursor)) == WT_ROLLBACK)
            ;
        testutil_assert(ret == 0);

        /*
         * Save the key separately for checking later.
         */
        pthread_mutex_lock(&log_mutex);
        thread_op_id ++;
        global_op_id++;
        local_global_op_id = global_op_id;
        if (fprintf(global_log_file, "(%" PRIu64 ", %" PRIu32 ", %" PRIu64 ", INSERT, %" PRIu64 ", %s)\n",
                    local_global_op_id, td->id, thread_op_id, i, buf) < 0)
            testutil_die(errno, "fprintf");
        pthread_mutex_unlock(&log_mutex);

        /*
         * If configured, run compaction on database after each epoch of 100000 operations.
         */
        if (compaction && i >= (100 * WT_THOUSAND) && i % (100 * WT_THOUSAND) == 0) {
            printf("Running compaction in Thread %" PRIu32 "\n", td->id);
            if (columnar_table)
                ret = session->compact(session, col_uri, NULL);
            else
                ret = session->compact(session, uri, NULL);
            /*
             * We may have several sessions trying to compact the same URI, in this case, EBUSY is
             * returned.
             */
            testutil_assert(ret == 0 || ret == EBUSY);
        }

        /*
         * Decide what kind of operation can be performed on the already inserted data.
         */
        if (i % MAX_NUM_OPS == OP_TYPE_DELETE) {
            if (columnar_table)
                cursor->set_key(cursor, i);
            else
                cursor->set_key(cursor, kname);

            while ((ret = cursor->remove(cursor)) == WT_ROLLBACK);
            // printf("Thread %u: ret %d \n", td->id, ret);
            if (ret != 0) {
                printf("Thread %u: ret %d with key %" PRIu64 "\n", td->id, ret, i);
            }
            testutil_assert(ret == 0);

            /* Save the key separately for checking later.*/
            pthread_mutex_lock(&log_mutex);
            thread_op_id ++;
            global_op_id++;
            local_global_op_id = global_op_id;
            if (fprintf(global_log_file, "(%" PRIu64 ", %" PRIu32 ", %" PRIu64 ", DELETE, %" PRIu64 ", deleted)\n",
                        local_global_op_id, td->id, thread_op_id, i) < 0)
                testutil_die(errno, "fprintf");
            pthread_mutex_unlock(&log_mutex);

        } else if (i % MAX_NUM_OPS == OP_TYPE_MODIFY) {
            testutil_snprintf(new_buf, sizeof(new_buf), "modify-%" PRIu64, i);
            new_buf_size = (data.size < MAX_VAL - 1 ? data.size : MAX_VAL - 1);

            newv.data = new_buf;
            newv.size = new_buf_size;
            maxdiff = MAX_VAL;

            /*
             * Make sure the modify operation is carried out in an snapshot isolation level with
             * explicit transaction.
             */
            do {
                testutil_check(session->begin_transaction(session, NULL));

                if (columnar_table)
                    cursor->set_key(cursor, i);
                else
                    cursor->set_key(cursor, kname);

                ret = wiredtiger_calc_modify(session, &data, &newv, maxdiff, entries, &nentries);
                if (ret == 0)
                    ret = cursor->modify(cursor, entries, nentries);
                else {
                    /*
                     * In case if we couldn't able to generate modify vectors, treat this change as
                     * a normal update operation.
                     */
                    cursor->set_value(cursor, &newv);
                    ret = cursor->update(cursor);
                }
                testutil_check(ret == 0 ? session->commit_transaction(session, NULL) :
                                          session->rollback_transaction(session, NULL));
            } while (ret == WT_ROLLBACK);
            testutil_assert(ret == 0);

            /*
             * Save the key and new value separately for checking later.
             */
            pthread_mutex_lock(&log_mutex);
            thread_op_id ++;
            global_op_id++;
            local_global_op_id = global_op_id;
            if (fprintf(global_log_file, "(%" PRIu64 ", %" PRIu32 ", %" PRIu64 ", MODIFY, %" PRIu64 ", %s)\n",
                        local_global_op_id, td->id, thread_op_id, i, new_buf) < 0)
                testutil_die(errno, "fprintf");
            pthread_mutex_unlock(&log_mutex);
        } else if (i % MAX_NUM_OPS != OP_TYPE_INSERT)
            /* Dead code. To catch any op type misses */
            testutil_die(0, "Unsupported operation type.");
    }
    // KILL IF USING NUM_OPS
    if (td->num_ops != 0) {
        printf("Thread %u killed\n", td->id);
        pthread_exit(NULL);
    }
    /* NOTREACHED */
    return NULL;
}


// TROUBLE. NEED TO FIX.
// static void fill_db(uint32_t, uint64_t) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));

/*
 * fill_db --
 *     Child process creates the database and table, and then creates worker threads to add data
 *     until it is killed by the parent.
 */
static void
fill_db(uint32_t nth, uint64_t num_ops)
{
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_THREAD_DATA *td;
    wt_thread_t *thr;
    uint32_t i;
    char envconf[512];

    thr = dcalloc(nth, sizeof(*thr));
    td = dcalloc(nth, sizeof(WT_THREAD_DATA));
    if (chdir(home) != 0)
        testutil_die(errno, "Child chdir: %s", home);
    if (inmem)
        strcpy(envconf, ENV_CONFIG_DEF);
    else if (use_lazyfs)
        strcpy(envconf, ENV_CONFIG_TXNSYNC_FSYNC);
    else
        strcpy(envconf, ENV_CONFIG_TXNSYNC);
    if (compat)
        strcat(envconf, TESTUTIL_ENV_CONFIG_COMPAT);

    testutil_check(wiredtiger_open(WT_HOME_DIR, NULL, envconf, &conn));
    testutil_check(conn->open_session(conn, NULL, NULL, &session));
    testutil_check(session->create(session, col_uri, "key_format=r,value_format=u"));
    testutil_check(session->create(session, uri, "key_format=S,value_format=u"));
    testutil_check(session->close(session, NULL));

    printf("Create %" PRIu32 " writer threads\n", nth);
    for (i = 0; i < nth; ++i) {
        td[i].conn = conn;
        num_ops != 0 ? td[i].start = num_ops * (uint64_t)i : WT_BILLION * (uint64_t)i;
        // td[i].start = WT_BILLION * (uint64_t)i; // change to num_ops
        td[i].id = i;
        td[i].num_ops = num_ops;
        testutil_check(__wt_thread_create(NULL, &thr[i], thread_run, &td[i]));
    }
    printf("Spawned %" PRIu32 " writer threads\n", nth);
    fflush(stdout);

    /*
    * The threads never exit, so the child will just wait here until it is killed.
    */
    for (i = 0; i < nth; ++i) {
        printf("Waiting for thread %u\n", i);
        testutil_check(__wt_thread_join(NULL, &thr[i]));
        printf("Joined thread %u\n", i);
    }
    printf("Ops: All threads complete!");
    free(thr);
    free(td);
    if (num_ops == 0) {
        _exit(EXIT_SUCCESS);
    }
}

extern int __wt_optind;
extern char *__wt_optarg;

/*
 * handler --
 *     TODO: Add a comment describing this function.
 */
static void
handler(int sig)
{
    pid_t pid;

    WT_UNUSED(sig);
    pid = wait(NULL);

    /* Clean up LazyFS. */
    if (use_lazyfs)
        testutil_lazyfs_cleanup(&lazyfs);

    /* The core file will indicate why the child exited. Choose EINVAL here. */
    testutil_die(EINVAL, "Child process %" PRIu64 " abnormally exited", (uint64_t)pid);
}

/*
 * recover_and_verify --
 *     TODO: Add a comment describing this function.
 */
static int
recover_and_verify(uint32_t nthreads, char* home_dir)
{
    // FILE *fp[MAX_RECORD_FILES];
    WT_CONNECTION *conn;
    WT_CURSOR *col_cursor, *cursor, *row_cursor;
    WT_DECL_RET;
    WT_ITEM search_value;
    WT_SESSION *session;
    uint64_t absent, count, key, last_key, middle;
    uint32_t i;
    char file_value[MAX_VAL];
    // char fname[MAX_RECORD_FILES][64];
    char kname[64];
    bool columnar_table, fatal;

    printf("Open database, run recovery and verify content\n");
    printf("What is WT_HOME? %s\n", home_dir);
    // testutil_check(wiredtiger_open(WT_HOME_DIR, NULL, TESTUTIL_ENV_CONFIG_REC, &conn));
    testutil_check(wiredtiger_open(home_dir, NULL, TESTUTIL_ENV_CONFIG_REC, &conn));
    testutil_check(conn->open_session(conn, NULL, NULL, &session));
    testutil_check(session->open_cursor(session, col_uri, NULL, NULL, &col_cursor));
    testutil_check(session->open_cursor(session, uri, NULL, NULL, &row_cursor));

    absent = count = 0;
    fatal = false;
    for (i = 0; i < nthreads; ++i) {

        /*
         * Every alternative thread is operated on column-store table. Make sure that proper cursor
         * is used for verification of recovered records.
         */
        if (i % 2 != 0) {
            columnar_table = true;
            cursor = col_cursor;
        } else {
            columnar_table = false;
            cursor = row_cursor;
        }

        middle = 0;
        // testutil_snprintf(fname[DELETE_RECORD_FILE_ID], sizeof(fname[DELETE_RECORD_FILE_ID]),
        //   DELETE_RECORDS_FILE, i);
        // if ((fp[DELETE_RECORD_FILE_ID] = fopen(fname[DELETE_RECORD_FILE_ID], "r")) == NULL)
        //     testutil_die(errno, "fopen: %s", fname[DELETE_RECORD_FILE_ID]);
        // testutil_snprintf(fname[INSERT_RECORD_FILE_ID], sizeof(fname[INSERT_RECORD_FILE_ID]),
        //   INSERT_RECORDS_FILE, i);
        // if ((fp[INSERT_RECORD_FILE_ID] = fopen(fname[INSERT_RECORD_FILE_ID], "r")) == NULL)
        //     testutil_die(errno, "fopen: %s", fname[INSERT_RECORD_FILE_ID]);
        // testutil_snprintf(fname[MODIFY_RECORD_FILE_ID], sizeof(fname[MODIFY_RECORD_FILE_ID]),
        //   MODIFY_RECORDS_FILE, i);
        // if ((fp[MODIFY_RECORD_FILE_ID] = fopen(fname[MODIFY_RECORD_FILE_ID], "r")) == NULL)
        //     testutil_die(errno, "fopen: %s", fname[MODIFY_RECORD_FILE_ID]);

        if ((global_log_file = fopen("global_log_file.txt", "r")) == NULL) {
            testutil_die(errno, "fopen: global_log_file.txt");
        }

        /*
         * For every key in the saved file, verify that the key exists in the table after recovery.
         * If we're doing in-memory log buffering we never expect a record missing in the middle,
         * but records may be missing at the end. If we did write-no-sync, we expect every key to
         * have been recovered.
         */
        for (last_key = UINT64_MAX;; ++count, last_key = key) {
            ret = fscanf(global_log_file, "%" SCNu64 "\n", &key);
            /*
             * Consider anything other than clear success in getting the key to be EOF. We've seen
             * file system issues where the file ends with zeroes on a 4K boundary and does not
             * return EOF but a ret of zero.
             */
            if (ret != 1)
                break;
            /*
             * If we're unlucky, the last line may be a partially written key at the end that can
             * result in a false negative error for a missing record. Detect it.
             */
            if (last_key != UINT64_MAX && key != last_key + 1) {
                printf("Global log file: Ignore partial record %" PRIu64 " last valid key %" PRIu64 "\n",
                  key, last_key);
                break;
            }

            if (key % MAX_NUM_OPS == OP_TYPE_DELETE) {
                /*
                 * If it is delete operation, make sure the record doesn't exist.
                 */
                ret = fscanf(global_log_file, "%" SCNu64 "\n", &key);

                /*
                 * Consider anything other than clear success in getting the key to be EOF. We've
                 * seen file system issues where the file ends with zeroes on a 4K boundary and does
                 * not return EOF but a ret of zero.
                 */
                if (ret != 1)
                    break;

                /*
                 * If we're unlucky, the last line may be a partially written key at the end that
                 * can result in a false negative error for a missing record. Detect it.
                 */
                if (last_key != UINT64_MAX && key <= last_key) {
                    printf("Global log file: Ignore partial record %" PRIu64 " last valid key %" PRIu64 "\n",
                    key, last_key);
                    break;
                }

                if (columnar_table)
                    cursor->set_key(cursor, key);
                else {
                    testutil_snprintf(kname, sizeof(kname), KEY_FORMAT, key);
                    cursor->set_key(cursor, kname);
                }

                while ((ret = cursor->search(cursor)) == WT_ROLLBACK)
                    ;
                if (ret != 0)
                    testutil_assert(ret == WT_NOTFOUND);
                else if (middle != 0) {
                    /*
                     * We should never find an existing key after we have detected one missing for
                     * the thread.
                     */
                    printf("Global log file: after missing record at %" PRIu64 " key %" PRIu64 " exists\n",
                        middle, key);
                    fatal = true;
                } else {
                    if (!inmem)
                        printf("Global log file: deleted record found with key %" PRIu64 "\n",
                          key);
                    absent++;
                    middle = key;
                }
            } else if (key % MAX_NUM_OPS == OP_TYPE_INSERT) {
                /*
                 * If it is insert only operation, make sure the record exists
                 */
                if (columnar_table)
                    cursor->set_key(cursor, key);
                else {
                    testutil_snprintf(kname, sizeof(kname), KEY_FORMAT, key);
                    cursor->set_key(cursor, kname);
                }

                while ((ret = cursor->search(cursor)) == WT_ROLLBACK)
                    ;
                if (ret != 0) {
                    testutil_assert(ret == WT_NOTFOUND);
                    if (!inmem)
                        printf("Global log file: no insert record with key %" PRIu64 "\n",
                          key);
                    absent++;
                    middle = key;
                } else if (middle != 0) {
                    /*
                     * We should never find an existing key after we have detected one missing for
                     * the thread.
                     */
                    printf("Global log file: after missing record at %" PRIu64 " key %" PRIu64 " exists\n",
                      middle, key);
                    fatal = true;
                }
            } else if (key % MAX_NUM_OPS == OP_TYPE_MODIFY) {
                /*
                 * If it is modify operation, make sure value of the fetched record matches with
                 * saved.
                 */
                ret = fscanf(
                  global_log_file, "%" STR_MAX_VAL "s %" SCNu64 "\n", file_value, &key);

                /*
                 * Consider anything other than clear success in getting the key to be EOF. We've
                 * seen file system issues where the file ends with zeroes on a 4K boundary and does
                 * not return EOF but a ret of zero.
                 */
                if (ret != 2)
                    break;

                /*
                 * If we're unlucky, the last line may be a partially written key and value at the
                 * end that can result in a false negative error for a missing record. Detect the
                 * key.
                 */
                if (last_key != UINT64_MAX && key <= last_key) {
                    printf("Global log file: Ignore partial record %" PRIu64 " last valid key %" PRIu64 "\n",
                    key, last_key);
                    break;
                }

                if (columnar_table)
                    cursor->set_key(cursor, key);
                else {
                    testutil_snprintf(kname, sizeof(kname), KEY_FORMAT, key);
                    cursor->set_key(cursor, kname);
                }

                while ((ret = cursor->search(cursor)) == WT_ROLLBACK)
                    ;
                if (ret != 0) {
                    testutil_assert(ret == WT_NOTFOUND);
                    if (!inmem)
                        printf("Global log file: no modified record with key %" PRIu64 "\n",
                            key);
                    absent++;
                    middle = key;
                } else if (middle != 0) {
                    /*
                     * We should never find an existing key after we have detected one missing for
                     * the thread.
                     */
                    printf("Global log file: after missing record at %" PRIu64 " key %" PRIu64 " exists\n",
                        middle, key);
                    fatal = true;
                } else {
                    testutil_check(cursor->get_value(cursor, &search_value));
                    if (strncmp(file_value, search_value.data, search_value.size) == 0)
                        continue;

                    if (!inmem)
                        /*
                         * Once the key exist in the database, there is no way that fetched data can
                         * mismatch with saved.
                         */
                        printf("Global log file: modified record with data mismatch key %" PRIu64 "\n",
                         key);

                    absent++;
                    middle = key;
                }
            } else
                /* Dead code. To catch any op type misses */
                testutil_die(0, "Unsupported operation type.");
        }
        // for (j = 0; j < MAX_RECORD_FILES; j++) {
        //     if (fclose(global_log_file) != 0)
        //         testutil_die(errno, "fclose");
        // }
        if (fclose(global_log_file) != 0) {testutil_die(errno, "fclose");}
                
    }
    testutil_check(conn->close(conn, NULL));
    if (fatal)
        return (EXIT_FAILURE);
    if (!inmem && absent) {
        printf("%" PRIu64 " record(s) are missed from %" PRIu64 "\n", absent, count);
        return (EXIT_FAILURE);
    }
    printf("%" PRIu64 " records verified\n", count);
    return (EXIT_SUCCESS);
}

static WT_OPS*
read_completed_log(const char* fname) {
    FILE *fp;
    uint64_t thread_id, done;
    uint32_t thread_op_id;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    WT_OPS * ops_head, *curr_ops;

    fp = fopen(fname, "r");
    if (fp == NULL) {
        testutil_die(errno, "fopen: %s", fname);
    }

    ops_head = (WT_OPS *)malloc(sizeof(WT_OPS));
    curr_ops = ops_head;
    ops_head->ops = NULL;

    // Read the file line by line in format (thread_id, thread_op_id, done)
    while ((read = getline(&line, &len, fp)) != -1) {
        sscanf(line, "%" SCNu64 ", %" SCNu32 ", %" SCNu64 "\n",
            &thread_id, &thread_op_id, &done);
        if (done) {
            // construct WT_OPS struct
            curr_ops->th_id = thread_id;
            curr_ops->th_op_id = thread_op_id;
            curr_ops->ops = (WT_OPS *)malloc(sizeof(WT_OPS));
            curr_ops = curr_ops->ops;
            curr_ops->ops = NULL;
        }
    }

    fclose(fp);
    if (line) {
        free(line);
    }

    // Ensure the last node's ops is NULL
    if (curr_ops != ops_head) {
        free(curr_ops);
        curr_ops = NULL;
    }

    return ops_head;
}

// Read the global log file and update the hashmap
// Input: log_file_path - path to the global log file
// Output: hashmap_out - hashmap to be updated
static void
read_global_log(const char* log_file_path, GHashTable **hashmap) { 
    FILE *fp;
    char *buf = (char *)malloc(64 * sizeof(char));
    char *op = (char *)malloc(64 * sizeof(char));
    uint64_t global_id, thread_op_id;
    uint32_t thread_id;
    uint32_t key;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    WT_OPS *head, *curr_ops, *temp;
    // char* element;


    fp = fopen(log_file_path, "r");
    if (fp == NULL) {
        testutil_die(errno, "fopen: %s", log_file_path);
    }

    head = read_completed_log("/home/jiexiao/wiredtiger/build/test/csuite/random_abort/ops_completed_log.txt");
    curr_ops = head;

    *hashmap = g_hash_table_new(g_direct_hash, g_direct_equal);

    // Iterate the completed operations and update the hashmap    
    while (curr_ops->ops != NULL) {
        // Get line until the thread_id and thread_op_id match
        while ((read = getline(&line, &len, fp)) != -1) {
            // Read the file line by line in format (global_id, thread_id, thread_op_id, op, key, buf)
            sscanf(line, "(%" PRIu64 ", %" PRIu32 ", %" PRIu64 ", %63[^,], %u, %63[^)\n])\n",
                &global_id, &thread_id, &thread_op_id, op, &key, buf);

            // let key and buf null terminated
            // key[strlen(key)] = '\0';
            buf[strlen(buf)] = '\0';

            if (curr_ops->th_id == thread_id && curr_ops->th_op_id == thread_op_id) {
                printf("Thread %u, op %lu: %s %u %s\n", thread_id, thread_op_id, op, key, buf);
                if (strcmp(op, "INSERT") == 0) {
                    // insert the key_val pair into the hashmap
                    g_hash_table_insert(*hashmap, GINT_TO_POINTER(key), strdup(buf));
                } else if (strcmp(op, "DELETE") == 0) {
                    // delete the key_val pair from the hashmap
                    g_hash_table_remove(*hashmap, GINT_TO_POINTER(key));
                } else if (strcmp(op, "MODIFY") == 0) {
                    // modify the key_val pair in the hashmap
                    g_hash_table_insert(*hashmap, GINT_TO_POINTER(key), strdup(buf));
                }
                break;
            }
        }
        curr_ops = curr_ops->ops;
    }

    fclose(fp);
    if (line) {
        free(line);
    }

    free(buf);
    free(op);
    // free(key);

    // Free the WT_OPS list
    while (head->ops != NULL) {
        temp = head;
        head = head->ops;
        free(temp);
    }
}

// Check the database for the key_val pairs present in the hashmap
// return 0 if the key_val pairs are present in the database
// return 1 if the key_val pairs are not present in the database
static int
check_db(const char* home_dir) {
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    uint32_t key_val;
    char *key;  
    WT_ITEM data;
    char* hashmap_value;
    int ret;
    GHashTable* hashmap = NULL;

    unsigned num_entries;
    // GHashTableIter iter;
    // gpointer key_ptr, value_ptr;

    int notFound = 0;

    // Get the hashmap from the global log file
    read_global_log("/home/jiexiao/wiredtiger/build/test/csuite/random_abort/global_log_file.txt", &hashmap);
    num_entries = g_hash_table_size(hashmap);
    printf("Number of entries in hashmap: %u\n", num_entries);
    
    
    // // print all key value pairs in hashmap
    /////////////////////////////////////////////////////////////////////////////////////
    // g_hash_table_iter_init(&iter, hashmap);
    // while (g_hash_table_iter_next(&iter, &key_ptr, &value_ptr)) {
    //     printf("Key: %d, Value: %s\n", GPOINTER_TO_INT(key_ptr), (char*)value_ptr);
    // }
    // if (!g_hash_table_contains(hashmap, key_ptr)) {
    //     // Key not found in hashmap, print the key-value pair
    //     fprintf(stderr, "Key not found in hashmap: Key: %d, Value: %s\n", GPOINTER_TO_INT(key_ptr), value);
    // }
    // hashmap_value = (char *)g_hash_table_lookup(hashmap, key_ptr);
    // // print hashmap_value
    // printf("Hashmap value: %s\n", hashmap_value);


    // if (!g_hash_table_contains(hashmap, GINT_TO_POINTER(1))) {
    //     // Key not found in hashmap, print the key-value pair
    //     fprintf(stderr, "Key not found in hashmap: Key: %d, Value: %s\n", 1, value);
    // }
    // hashmap_value = (char *)g_hash_table_lookup(hashmap, GINT_TO_POINTER(1));
    // // print hashmap_value
    // printf("Hashmap value: %s\n", hashmap_value);
    /////////////////////////////////////////////////////////////////////////////


    // Initialize the WiredTiger database connection
    if ((ret = wiredtiger_open(home_dir, NULL, "create", &conn)) != 0) {
        fprintf(stderr, "Error connecting to WiredTiger: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Open a session
    if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0) {
        fprintf(stderr, "Error opening session: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Open the table
    // Assume the table is named 'table:mytable'
    if ((ret = session->open_cursor(session, "table:main", NULL, NULL, &cursor)) != 0) {
        fprintf(stderr, "Error opening cursor: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Iterate through the table and print the key-value pairs
    while ((ret = cursor->next(cursor)) == 0) {
        if ((ret = cursor->get_key(cursor, &key)) != 0) {
            fprintf(stderr, "Error getting key: %s\n", wiredtiger_strerror(ret));
            cursor->close(cursor);
            session->close(session, NULL);
            conn->close(conn, NULL);
            g_hash_table_destroy(hashmap);
            return ret;
        }

        key_val = (uint32_t)atoi(key);

        // Check if the key is present in the hashmap
        hashmap_value = (char *)g_hash_table_lookup(hashmap, GINT_TO_POINTER(key_val));
        
        // print hashmap_value
        // printf("Key: %u\n", key_val);
        // printf("Hashmap value: %s\n", hashmap_value);

        if ((ret = cursor->get_value(cursor, &data)) != 0) {
            fprintf(stderr, "Error getting value: %s\n", wiredtiger_strerror(ret));
            cursor->close(cursor);
            session->close(session, NULL);
            conn->close(conn, NULL);
            g_hash_table_destroy(hashmap);
            return ret;
        }

         if (hashmap_value == NULL || strcmp(hashmap_value, (char *)data.data) != 0) {
            // Key not found or value mismatch, print the key-value pair
            fprintf(stderr, "Mismatch or missing key: Key: %u, actual value: %s, expected value: %s\n", key_val, hashmap_value, (char *)data.data);
            notFound = 1;
        } else {
            // Remove the key from the hashmap
            g_hash_table_remove(hashmap, GINT_TO_POINTER(key_val));
        }
    }

    if (ret != WT_NOTFOUND) {
        fprintf(stderr, "Error iterating cursor: %s\n", wiredtiger_strerror(ret));
        cursor->close(cursor);
        session->close(session, NULL);
        conn->close(conn, NULL);
        g_hash_table_destroy(hashmap);
        return EXIT_FAILURE;
    }

    // check the size of hashmap
    if (g_hash_table_size(hashmap) != 0) {
        fprintf(stderr, "Error: Hashmap not empty\n");
        cursor->close(cursor);
        session->close(session, NULL);
        conn->close(conn, NULL);
        g_hash_table_destroy(hashmap);
        return EXIT_FAILURE;
    }


    // Close the cursor, session, and connection
    cursor->close(cursor);
    session->close(session, NULL);
    conn->close(conn, NULL);
    g_hash_table_destroy(hashmap);

    return notFound;
}

static void 
copy_directory(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    struct dirent *entry;
    char src_path[1024];
    char dst_path[1024];
    struct stat statbuf;

    mkdir(dst, 0755);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        stat(src_path, &statbuf);

        if (S_ISDIR(statbuf.st_mode)) {
            copy_directory(src_path, dst_path);
        } else {
            FILE *src_file = fopen(src_path, "rb");
            FILE *dst_file = fopen(dst_path, "wb");

            char buffer[4096];
            size_t bytes;

            while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                fwrite(buffer, 1, bytes, dst_file);
            }

            fclose(src_file);
            fclose(dst_file);
        }
    }
    closedir(dir);
}

static int
test_backup(const char* home_dir, const char* copy_dir) {
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    WT_CURSOR *backup_cursor;
    int ret = 0;
    // char* backup_dir = "/home/jiexiao/wiredtiger/build/test/csuite/random_abort/backup";

    // Initialize the WiredTiger database connection
    if ((ret = wiredtiger_open(home_dir, NULL, "create", &conn)) != 0) {
        fprintf(stderr, "Error connecting to WiredTiger: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Open a session
    if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0) {
        fprintf(stderr, "Error opening session: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Open the table
    // Assume the table is named 'table:mytable'
    if ((ret = session->open_cursor(session, "table:main", NULL, NULL, &cursor)) != 0) {
        fprintf(stderr, "Error opening cursor: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Backup the database
    if ((ret = session->open_cursor(session, "backup:", NULL, NULL, &backup_cursor)) != 0) {
        fprintf(stderr, "Error backing up database: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    copy_directory(home_dir, copy_dir);

    // Close the cursor, session, and connection
    cursor->close(cursor);
    session->close(session, NULL);
    conn->close(conn, NULL);

    // Open connection to the copied directory and verify the data
    if ((ret = wiredtiger_open(copy_dir, NULL, "create", &conn)) != 0) {
        fprintf(stderr, "Error connecting to WiredTiger: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // Open a session
    if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0) {
        fprintf(stderr, "Error opening session: %s\n", wiredtiger_strerror(ret));
        return ret;
    }

    // close the session and connection
    cursor->close(cursor);
    backup_cursor->close(backup_cursor);
    session->close(session, NULL);
    conn->close(conn, NULL);

    return (EXIT_SUCCESS);
}

static int 
check_backup(const char* home_dir) {
    printf("home dir: %s\n", home_dir);
    return EXIT_SUCCESS;
}

/*
 * main --
 *     TODO: Add a comment describing this function.
 */
int
main(int argc, char *argv[])
{
    struct sigaction sa;
    // struct stat sb;
    WT_RAND_STATE rnd;
    pid_t pid;
    uint32_t i, nth, timeout;
    uint64_t num_ops;
    int ch;
    int ret = 0; 
    int status;
    char buf[PATH_MAX];
    // char log_file_path[1024];
    // char fname[MAX_RECORD_FILES][64];
    char cwd_start[PATH_MAX]; /* The working directory when we started */
    const char *working_dir;
    const char *squint_mode;
    bool preserve, rand_th, rand_time, verify_only, squint, backup;
    char cwd[1024]; // DELETE

    (void)testutil_set_progname(argv);

    compaction = compat = inmem = false;
    use_lazyfs = lazyfs_is_implicitly_enabled();
    nth = MIN_TH;
    preserve = false;
    rand_th = rand_time = true;
    timeout = MIN_TIME;
    verify_only = false;
    squint = false;
    backup = false;
    num_ops = 0;
    working_dir = use_lazyfs ? "WT_TEST.random-abort-lazyfs" : "WT_TEST.random-abort";

    while ((ch = __wt_getopt(progname, argc, argv, "Cch:lmpT:t:vs:o:")) != EOF)
        switch (ch) {
        case 'C':
            compat = true;
            break;
        case 'c':
            compaction = true;
            break;
        case 'h':
            working_dir = __wt_optarg;
            while (isspace(*working_dir)) working_dir++;
            break;
        case 'l':
            use_lazyfs = true;
            break;
        case 'm':
            inmem = true;
            break;
        case 'p':
            preserve = true;
            break;
        case 'T':
            rand_th = false;
            nth = (uint32_t)atoi(__wt_optarg);
            break;
        case 't':
            rand_time = false;
            timeout = (uint32_t)atoi(__wt_optarg);
            break;
        case 'v':
            verify_only = true;
            break;
        case 's':
            squint_mode = __wt_optarg;
            // remove any numbers of trailing space in the beginning
            while (isspace(*squint_mode)) squint_mode++;
            printf("Squint Mode: %s\n", squint_mode);
            if (strcmp(squint_mode, "workload") == 0) {
                squint = true;
                preserve = true;
            }
            if (strcmp(squint_mode, "checker") == 0) {
                squint = true;
                preserve = true;
                verify_only = true;
            }
            if (strcmp(squint_mode, "backup-workload") == 0) {
                squint = true;
                preserve = true;
                backup = true;
            }
            if (strcmp(squint_mode, "backup-checker") == 0) {
                squint = true;
                preserve = true;
                verify_only = true;
                backup = true;
            }
            break;
        case 'o':
            /* The number of operations per thread. Takes precedence over -t. */
            if (__wt_optarg != 0) {
                rand_time = false;
                num_ops = (uint64_t)atoi(__wt_optarg);
                printf("Timeout disabled, executing a finite number of operations.\n");
            }
            break;
        default:
            usage();
        }
    argc -= __wt_optind;
    if (argc != 0)
        usage();
    
    
    // snprintf(log_file_path, sizeof(log_file_path), "%s/global_log_file.txt", working_dir);
    // printf("Log file path: %s\n", log_file_path);
    global_log_file = fopen("global_log_file.txt", "w");
    if (global_log_file == NULL) {
        testutil_die(errno, "fopen: global_log_file.txt");
    }
    testutil_work_dir_from_path(home, sizeof(home), working_dir);

    /*
     * If the user wants to verify they need to tell us how many threads there were so we can find
     * the old record files.
     */
    if (verify_only && rand_th) {
        fprintf(stderr, "Verify option requires specifying number of threads\n");
        exit(EXIT_FAILURE);
    }

    /* Remember the current working directory. */
    testutil_assert_errno(getcwd(cwd_start, sizeof(cwd_start)) != NULL);

    /* Create the database, run the test, and fail. */
    if (!verify_only) {
        /* Create the test's home directory. */
        testutil_recreate_dir(home);

        /* Set up the test subdirectories. */
        testutil_snprintf(buf, sizeof(buf), "%s/%s", home, RECORDS_DIR);
        testutil_mkdir(buf);
        testutil_snprintf(buf, sizeof(buf), "%s/%s", home, WT_HOME_DIR);
        testutil_mkdir(buf);
        printf("Set up test home directory and subdirectories!\n");

        /* Set up LazyFS. */
        if (use_lazyfs)
            testutil_lazyfs_setup(&lazyfs, home);

        /* Set up the rest of the test. */
        __wt_random_init_seed(NULL, &rnd);
        if (rand_time) {
            timeout = __wt_random(&rnd) % MAX_TIME;
            if (timeout < MIN_TIME)
                timeout = MIN_TIME;
        }
        if (rand_th) {
            nth = __wt_random(&rnd) % MAX_TH;
            if (nth < MIN_TH)
                nth = MIN_TH;
        }
        printf("Parent: Compatibility %s in-mem log %s\n", compat ? "true" : "false",
          inmem ? "true" : "false");
        printf("Parent: Create %" PRIu32 " threads; sleep %" PRIu32 " seconds\n", nth, timeout);
        printf("CONFIG: %s%s%s%s%s -h %s -T %" PRIu32 " -t %" PRIu32 "\n", progname,
          compat ? " -C" : "", compaction ? " -c" : "", use_lazyfs ? " -l" : "", inmem ? " -m" : "",
          working_dir, nth, timeout);

        /*
         * Fork a child to insert as many items. We will then randomly kill the child, run recovery
         * and make sure all items we wrote exist after recovery runs.
         */
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handler;
        testutil_assert_errno(sigaction(SIGCHLD, &sa, NULL) == 0);
        testutil_assert_errno((pid = fork()) >= 0);
        
        if (pid == 0) { /* child */
            fill_db(nth, num_ops);
            /* NOTREACHED */
            printf("NOT REACHED?\n");
        } else if (pid > 0 && num_ops != 0) {
            for (i = 0; i < nth; ++i) {
                waitpid(pid, &status, 0);
            }
            printf("Passed\n");
        } else {
            /* parent */
            /*
            * Sleep for the configured amount of time before killing the child. Start the timeout from
            * the time we notice that the child workers have created their record files. That allows
            * the test to run correctly on really slow machines.
            */
            i = 0;
            // while (i < nth) {
            //     for (j = 0; j < MAX_RECORD_FILES; j++) {
            //         /*
            //         * Wait for each record file to exist.
            //         */
            //         if (j == DELETE_RECORD_FILE_ID)
            //             testutil_snprintf(fname[j], sizeof(fname[j]), DELETE_RECORDS_FILE, i);
            //         else if (j == INSERT_RECORD_FILE_ID)
            //             testutil_snprintf(fname[j], sizeof(fname[j]), INSERT_RECORDS_FILE, i);
            //         else
            //             testutil_snprintf(fname[j], sizeof(fname[j]), MODIFY_RECORDS_FILE, i);
            //         testutil_snprintf(buf, sizeof(buf), "%s/%s", home, fname[j]);
            //         while (stat(buf, &sb) != 0)
            //             testutil_sleep_wait(1, pid);
            //     }
            //     ++i;
            // }

            sleep(timeout);
            sa.sa_handler = SIG_DFL;
            testutil_assert_errno(sigaction(SIGCHLD, &sa, NULL) == 0);
            testutil_assert_errno(kill(pid, SIGKILL) == 0);
            testutil_assert_errno(waitpid(pid, &status, 0) != -1);
        }
       
    }
    printf("Filled database!\n");

    /*
     * !!! If we wanted to take a copy of the directory before recovery,
     * this is the place to do it.
     */
    if (chdir(home) != 0)
        testutil_die(errno, "parent chdir: %s", home);

    /* Copy the data to a separate folder for debugging purpose. */
    if (!squint) {
        testutil_copy_data(home);
    }

    /*
     * Clear the cache, if we are using LazyFS. Do this after we save the data for debugging
     * purposes, so that we can see what we might have lost. If we are using LazyFS, the underlying
     * directory shows the state that we'd get after we clear the cache.
     */
    if (!verify_only && use_lazyfs)
        testutil_lazyfs_clear_cache(&lazyfs);

    /*
     * Recover the database and verify whether all the records from all threads are present or not?
     */
    if (squint && verify_only) {
        // testutil_snprintf(buf, sizeof(buf), "%s/%s", home, RECORDS_DIR);
        // wait thread finish
        testutil_snprintf(buf, sizeof(buf), "%s/%s", home, WT_HOME_DIR);
        if (backup) {
            ret = check_backup(buf);
            if (ret != EXIT_SUCCESS) {
                printf("Backup failed\n");
            } else {
                printf("Backup succeeded\n");
            }
        } else {
            if (check_db(buf)!= 0) {
                printf("Mismatch or missing key-value pairs in the database\n");
            } else {
                printf("All key-value pairs are present in the database\n");
            }
        }
    } else if (verify_only) {
        // testutil_snprintf(buf, sizeof(buf), "%s/%s", home, RECORDS_DIR);
        testutil_snprintf(buf, sizeof(buf), "%s/%s", home, WT_HOME_DIR);
        ret = recover_and_verify(nth, buf);
    } else {
        if (backup) {
            ret = test_backup(buf, "/home/jiexiao/wiredtiger/logs/backup");
            if (ret != EXIT_SUCCESS) {
                printf("Backup failed\n");
            } else {
                printf("Backup succeeded\n");
            }
        } else {
            ret = recover_and_verify(nth, buf);
        }
        ret = EXIT_SUCCESS;
    } 

    /*
     * Clean up.
     */

    /* Clean up the test directory. */
    if (ret == EXIT_SUCCESS && !preserve)
        testutil_clean_test_artifacts(home);

    /* At this point, we are inside `home`, which we intend to delete. cd to the parent dir. */
    if (chdir(cwd_start) != 0)
        testutil_die(errno, "root chdir: %s", home);

    /* Clean up LazyFS. */
    if (!verify_only && use_lazyfs)
        testutil_lazyfs_cleanup(&lazyfs);

    /* Delete the work directory. */
    if (ret == EXIT_SUCCESS && !preserve)
        testutil_remove(home);
    

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }

    printf("Done!\n");

    // close the file and clean the mutex
    pthread_mutex_destroy(&log_mutex);
    return (ret);
}
