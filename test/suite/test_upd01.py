#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

import threading
import wiredtiger, wttest, wtthread
from wtscenario import make_scenarios

# test_upd01.py
# Test an update restore eviction
class test_upd01(wttest.WiredTigerTestCase):
    conn_config = 'cache_size=2MB,eviction=(threads_max=1),statistics=(all),timing_stress_for_test=[checkpoint_slow]'
    key_format_values = [
        ('column', dict(key_format='r')),
    ]
    scenarios = make_scenarios(key_format_values)
    
    def evict(self, k, do_assert, do_transaction):
        # Configure debug behavior on a cursor to evict the positioned page on cursor reset
        # and evict the page.
        evict_cursor = self.session.open_cursor(self.uri, None, "debug=(release_evict)")
        if do_transaction: # Used by BF-32981, but not WT-12096.
            self.session.begin_transaction()
        evict_cursor.set_key(k)
        if do_assert: # Used by WT-12096, but not BF-32981
            self.assertEquals(evict_cursor.search(), 0)
        else:
            evict_cursor.search()

        self.session.breakpoint()

        evict_cursor.reset()
        evict_cursor.close() # Used by BF-32981, but not WT-12096
        if do_transaction: # Used by BF-32981, but not WT-12096
            self.session.rollback_transaction()

    def test_update_restore_eviction(self):
        uri = "table:test_upd01-update_restore_eviction"
        create_params = 'value_format=S,key_format={}'.format(self.key_format)
        value1 = 'abcedfghijklmnopqrstuvwxyz' * 5
        self.session.create(uri, create_params)
        cursor = self.session.open_cursor(uri)

        session2 = self.setUpSessionOpen(self.conn)
        session2.create(uri, create_params)
        cursor2 = session2.open_cursor(uri)

        # Insert a visible update
        self.session.begin_transaction()
        cursor[1] = value1
        self.session.commit_transaction()

        # Insert a not visible, i.e. uncommitted, update
        session2.begin_transaction()
        cursor2[1] = value1
        cursor2.close()         # To delete hazard pointers

        # Create a checkpoint in parallel with the evcition below.
        done = threading.Event()
        ckpt = wtthread.checkpoint_thread(self.conn, done)
        try:
            self.pr("start checkpoint")
            ckpt.start()
            self.evict(1, True, False)
        finally:
            done.set()          # Signal chkpt to exit.
            ckpt.join()
