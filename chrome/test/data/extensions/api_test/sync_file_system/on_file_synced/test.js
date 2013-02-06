// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onFileSynced.addListener(fileSyncEventReceived);
  chrome.syncFileSystem.requestFileSystem(function() {});
}

function fileSyncEventReceived(file_entry, sync_operation_result) {
  chrome.test.assertEq("foo.txt", file_entry.name);
  chrome.test.assertEq("/foo.txt", file_entry.fullPath);
  chrome.test.assertTrue(file_entry.isFile);
  chrome.test.assertFalse(file_entry.isDirectory);
  chrome.test.assertEq("added", sync_operation_result);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
