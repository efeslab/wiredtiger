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

// /*
//  * Maximum number of modifications that are allowed to perform cursor modify operation.
//  */
// #define MAX_MODIFY_ENTRIES 10

#define MAX_VAL 4096
/*
 * STR_MAX_VAL is set to MAX_VAL - 1 to account for the extra null character.
 */
// #define STR_MAX_VAL "4095"

// static void handler(int) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));
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
const char *working_dir;
char copy_dir[512];
char home_dir[512];

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t backup_mutex = PTHREAD_MUTEX_INITIALIZER;


// pthread_mutex_t global_op_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 * thread_run --
 *     TODO: Add a comment describing this function.
 */
static WT_THREAD_RET
thread_run(void *arg)
{
    WT_CURSOR *cursor;
    // WT_CURSOR *backup_cursor;
    WT_DECL_RET;
    WT_ITEM data;
    WT_RAND_STATE rnd;
    WT_SESSION *session;
    WT_THREAD_DATA *td;
    size_t lsize;
    uint64_t i, thread_op_id = 0;
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
        if (i == (td->start + td->num_ops / 2)) {
            testutil_snprintf(buf, sizeof(buf), "%s", "backup");
        }

        if (columnar_table)
            cursor->set_key(cursor, i);
        else {
            testutil_snprintf(kname, sizeof(kname), KEY_FORMAT, i);
            cursor->set_key(cursor, kname);
        }

        data.size = __wt_random(&rnd) % MAX_VAL;
        data.data = buf;

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

    }
    
    if (cursor != NULL) {
        ret = cursor->close(cursor);
        testutil_assert(ret == 0);
    }
    ret = session->close(session, NULL);
    testutil_assert(ret == 0);

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
        // num_ops != 0 ? td[i].start = num_ops * (uint64_t)i : WT_BILLION * (uint64_t)i;
        td[i].start = num_ops != 0 ? num_ops * (uint64_t)i : WT_BILLION * (uint64_t)i;
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
    testutil_check(conn->close(conn, NULL));
    free(thr);
    free(td);
    if (num_ops == 0) {
        _exit(EXIT_SUCCESS);
    }
}

extern int __wt_optind;
extern char *__wt_optarg;

static int checker(const char* home_path) {
    WT_CONNECTION *conn = NULL;
    WT_SESSION *session = NULL;
    WT_CURSOR *cursor = NULL;
    int ret;

    // Initialize the WiredTiger database connection
    if ((ret = wiredtiger_open(home_path, NULL, "create", &conn)) != 0) {
        fprintf(stderr, "Error connecting to WiredTiger: %s\n", wiredtiger_strerror(ret));
        return EXIT_FAILURE;
    }

    // Open a session
    if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0) {
        fprintf(stderr, "Error opening session: %s\n", wiredtiger_strerror(ret));
        goto cleanup_connection;
    }

    // Open the cursor for the table 'table:main'
    if ((ret = session->open_cursor(session, "table:main", NULL, NULL, &cursor)) != 0) {
        fprintf(stderr, "Error opening cursor: %s\n", wiredtiger_strerror(ret));
        goto cleanup_session;
    }

    // Iterate over the table
    while ((ret = cursor->next(cursor)) == 0) {
        WT_ITEM key_item, value_item;

        // Get the key
        if ((ret = cursor->get_key(cursor, &key_item)) != 0) {
            fprintf(stderr, "Error getting key: %s\n", wiredtiger_strerror(ret));
            goto cleanup_cursor;
        }

        // Get the value
        if ((ret = cursor->get_value(cursor, &value_item)) != 0) {
            fprintf(stderr, "Error getting value: %s\n", wiredtiger_strerror(ret));
            goto cleanup_cursor;
        }

        // Print the key and value
        printf("key: %s, value: %s\n",
       (const char *)key_item.data,
       (const char *)value_item.data);
    }

    if (ret != WT_NOTFOUND) {
        fprintf(stderr, "Error iterating over cursor: %s\n", wiredtiger_strerror(ret));
        goto cleanup_cursor;
    }

    // Successfully iterated over the table
    ret = EXIT_SUCCESS;

cleanup_cursor:
    if (cursor) {
        if ((ret = cursor->close(cursor)) != 0) {
            fprintf(stderr, "Error closing cursor: %s\n", wiredtiger_strerror(ret));
        }
    }

cleanup_session:
    if (session) {
        if ((ret = session->close(session, NULL)) != 0) {
            fprintf(stderr, "Error closing session: %s\n", wiredtiger_strerror(ret));
        }
    }

cleanup_connection:
    if (conn) {
        if ((ret = conn->close(conn, NULL)) != 0) {
            fprintf(stderr, "Error closing connection: %s\n", wiredtiger_strerror(ret));
        }
    }

    return ret == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * main --
 *     TODO: Add a comment describing this function.
 */
int
main(int argc, char *argv[])
{
    // struct sigaction sa;
    // struct stat sb;
    WT_RAND_STATE rnd;
    // pid_t pid;
    uint32_t nth, timeout;
    uint64_t num_ops;
    int ch;
    int ret = 0; 
    // int status;
    char buf[PATH_MAX];
    // char log_file_path[1024];
    // char fname[MAX_RECORD_FILES][64];
    char cwd_start[PATH_MAX]; /* The working directory when we started */
    const char *squint_mode;
    // const char *global_log_file_path;
    // const char *backup_path;
    bool preserve, rand_th, rand_time, verify_only, squint;
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
    // backup = false;
    num_ops = 0;
    // working_dir = use_lazyfs ? "WT_TEST.random-abort-lazyfs" : "WT_TEST.random-abort";
    if (argc == 2) {
        rand_th = false;
        nth = 1;

        working_dir = argv[1];
        while (isspace(*working_dir)) working_dir++;

        squint = true;
        preserve = true;
        verify_only = true;
    } else {
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
                // if (strcmp(squint_mode, "backup-workload") == 0) {
                //     squint = true;
                //     preserve = true;
                //     backup = true;
                // }
                // if (strcmp(squint_mode, "backup-checker") == 0) {
                //     squint = true;
                //     preserve = true;
                //     verify_only = true;
                //     backup = true;
                // }
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
    }

    // argc -= __wt_optind;
    // if (argc != 0)
    //     usage();


    // snprintf(log_file_path, sizeof(log_file_path), "%s/global_log_file.txt", working_dir);
    // printf("Log file path: %s\n", log_file_path);
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
        // global_log_file_path = strcat(working_dir, "/global_log_file.txt");
        global_log_file = fopen("/home/jiexiao/squint/alice/alice/bugs/bug4/global_log_file.txt", "w");
        if (global_log_file == NULL) {
            testutil_die(errno, "fopen: global_log_file.txt");
        }

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
        // memset(&sa, 0, sizeof(sa));
        // sa.sa_handler = handler;
        // testutil_assert_errno(sigaction(SIGCHLD, &sa, NULL) == 0);
        // testutil_assert_errno((pid = fork()) >= 0);
        fill_db(nth, num_ops);

        fclose(global_log_file);
        printf("Filled database!\n");
    }
   

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
        // testutil_snprintf(buf, sizeof(buf), "%s/%s", home, WT_HOME_DIR);
        snprintf(home_dir, sizeof(home_dir), "%s/WT_HOME", working_dir);
        if (checker(home_dir)!= 0) {
            printf("Mismatch or missing key-value pairs in the database before backup\n");
        } else {
            printf("All key-value pairs are present in the database\n");
        }
    } else {
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
    pthread_mutex_destroy(&backup_mutex);
    return (ret);
}
