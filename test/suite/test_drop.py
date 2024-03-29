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

import wiredtiger, wttest
from helper import confirm_does_not_exist
from wtdataset import SimpleDataSet, ComplexDataSet
from wtdataset import SimpleIndexDataSet
from wtscenario import make_scenarios

# test_drop.py
#    session level drop operation
@wttest.skip_for_hook("tiered", "FIXME-WT-9809 - Fails for tiered")
class test_drop(wttest.WiredTigerTestCase):
    name = 'test_drop'
    extra_config = ''

    scenarios = make_scenarios([
        # XXX TEMPORARY
        #('file', dict(uri='file:')),
        #('table', dict(uri='table:')),
        ('table-lsm', dict(uri='table:', extra_config=',type=lsm')),
    ])

    # Populate an object, remove it and confirm it no longer exists.
    def drop(self, dataset, cnt, with_cursor, reopen, with_transaction, drop_index):
        uri = self.uri + self.name + "." + str(cnt)
        self.prout(f'>> {cnt}. Start: dataset(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
        ds = dataset(self, uri, 10, config=self.extra_config)
        # Set first values to variant 1.
        self.session.begin_transaction()
        self.prout(f'>> ds.popuate(,variant=1): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
        ds.populate()
        variant = 1
        self.session.commit_transaction(),

        # Open cursors should cause failure.
        if with_cursor and not with_transaction:
            self.prout(f'>> open_cursor(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            cursor = self.session.open_cursor(uri, None, None)
            self.prout(f'>> A. drop() with open cursor should fail: uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.assertRaises(wiredtiger.WiredTigerError,
                lambda: self.session.drop(uri, None))
            self.prout(f'>> cursor.close(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            cursor.close()
            # Check that the table works and has not changed.
            self.prout(f'>> ds.check(,variant=1): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            ds.check()

        #XXX Check this
        # Active transactions should cause EBUSY.
        if with_transaction:
            self.session.begin_transaction()
            if with_cursor:
                self.prout(f'>> open_cursor(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
                cursor = self.session.open_cursor(uri, None, None)
            # Change from variant 1 to variant 2 within transaction A.
            self.prout(f'>> ds.popuate(,variant=2): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            ds.populate(False, 2)
            variant = 2
            # Fail to drop the table with EBUSY.
            self.prout(f'>> B. drop() with active transaction should fail with EBUSY: uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.assertTrue(self.raisesBusy(lambda: self.session.drop(uri, None)),
                            "was expecting drop call to fail with EBUSY")
            # Check that transaction A still contains variant 2.
            self.prout(f'>> ds.check(,variant=2): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            ds.check(2)
            if with_cursor:
                self.prout(f'>> cursor.close(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
                cursor.close()
            # Check that transaction A needs rollback by failing to commit it.
            self.prout(f'>> commit_transaction() fails: transaction requires rollback: uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.assertRaisesWithMessage(wiredtiger.WiredTigerError,
                lambda: self.session.commit_transaction(),
                "/transaction requires rollback: Invalid argument/")
            variant = 1
            # Test that we are back to variant 1.
            self.prout(f'>> ds.check(,variant=1): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            ds.check(1)

        if reopen:
            self.prout(f'>> reopen_conn(): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.reopen_conn()
            # Check that the table still contains the proper variant.
            self.prout(f'>> ds.check(,variant={variant}): uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            ds.check(variant)

        if drop_index:
            if dataset == SimpleIndexDataSet:
                drop_uri = ds.indexname
            else:
                drop_uri = ds.index_name(0)
        else:
            drop_uri = uri
        self.prout(f'>> C. dropUntilSuccess({drop_uri}): with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
        self.dropUntilSuccess(self.session, drop_uri)

        self.prout(f'>> confirm_does_not_exist({drop_uri}): with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
        confirm_does_not_exist(self, drop_uri)

        # Skip if tiered because test_drop_dne contains: self.skipTest("negative tests for drop do not work in tiered storage").
        if self.extra_config.find('type=lsm') == -1:
            # Test dropping a non-existent table
            # Fail without force or force=false
            self.prout(f'>> D. drop({drop_uri}) without force a non-existent table should fail: with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.assertRaises(wiredtiger.WiredTigerError,
                lambda: self.session.drop(drop_uri, None))
            self.prout(f'>> E. drop({drop_uri}, force=false) a non-existent table should fail: with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.assertRaises(wiredtiger.WiredTigerError,
                lambda: self.session.drop(drop_uri, "force=false"))
            # Succeed with force=true.
            self.prout(f'>> F. drop({drop_uri}, force=true) a non-existent table: with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY
            self.session.drop(drop_uri, "force=true")

        self.prout(f'>> {cnt}. Done: uri={uri}, with_cursor={with_cursor}, reopen={reopen}, with_transaction={with_transaction}, drop_index={drop_index}') # XXX TEMPORARY

    # Test drop of an object.
    def test_drop(self):
        self.prout(f'>> test_drop: wttest.WiredTigerTestCase._concurrent={wttest.WiredTigerTestCase._concurrent}') # XXX TEMPORARY
        cnt = 0
        # SimpleDataSet: Simple file or table object.
        # Try all combinations except dropping the index, the simple
        # case has no indices.
        if False:
            self.prout('dataset is SimpleDataSet') # XXX TEMPORARY
            for with_cursor in [False, True]:
                for reopen in [False, True]:
                    for with_transaction in [False, True]:
                        cnt = cnt + 1
                        self.drop(SimpleDataSet, cnt, with_cursor, reopen, with_transaction, False)
        elif False:
            self.prout('dataset is SimpleDataSet') # XXX TEMPORARY
            with_cursor = False
            reopen = False
            with_transaction = True
            drop_index = False
            cnt = cnt + 1
            self.drop(SimpleDataSet, cnt, with_cursor, reopen, with_transaction, False)

        # SimpleIndexDataSet: A table with an index
        # Try almost all test combinations.
        # Skip if tiered since indices don't work for tiered.
        if False and self.uri == "table:" and self.extra_config.find('type=lsm') == -1:
            self.prout('dataset is SimpleIndexDataSet') # XXX TEMPORARY
            for with_cursor in [False, True]:
                for reopen in [False, True]:
                    for with_transaction in [False, True]:
                        cnt = cnt + 1
                        # drop_index == False since it does not work.
                        self.drop(SimpleIndexDataSet, cnt, with_cursor,
                                  reopen, with_transaction, False)

        # ComplexDataSet: A complex, multi-file table object.
        # Try all test combinations.
        if False and self.uri == "table:":
            self.prout('dataset is ComplexDataSet') # XXX TEMPORARY
            for with_cursor in [False, True]:
                for reopen in [False, True]:
                    for with_transaction in [False, True]:
                        for drop_index in [False, True]:
                            cnt = cnt + 1
                            self.drop(ComplexDataSet, cnt, with_cursor,
                                      reopen, with_transaction, drop_index)
        elif True and self.uri == "table:":
            self.prout('dataset is ComplexDataSet') # XXX TEMPORARY
            with_cursor = False
            reopen = False
            with_transaction = False
            drop_index = False
            cnt = cnt + 1
            self.drop(ComplexDataSet, cnt, with_cursor, reopen, with_transaction, drop_index)

    # Test drop of a non-existent object: force succeeds, without force fails.
    def hide_test_drop_dne(self): # XXX TEMPORARY
        if 'tiered' in self.hook_names:
            self.skipTest("negative tests for drop do not work in tiered storage")
        uri = self.uri + self.name
        cguri = 'colgroup:' + self.name
        idxuri = 'index:' + self.name + ':indexname'
        lsmuri = 'lsm:' + self.name
        confirm_does_not_exist(self, uri)
        self.session.drop(uri, 'force')
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.drop(uri, None))
        self.session.drop(cguri, 'force')
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.drop(cguri, None))
        self.session.drop(idxuri, 'force')
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.drop(idxuri, None))
        self.session.drop(lsmuri, 'force')
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.drop(lsmuri, None))
