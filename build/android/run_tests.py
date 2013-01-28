#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all the native unit tests.

1. Copy over test binary to /data/local on device.
2. Resources: chrome/unit_tests requires resources (chrome.pak and en-US.pak)
   to be deployed to the device. We use the device's $EXTERNAL_STORAGE as the
   base dir (which maps to Context.getExternalFilesDir()).
3. Environment:
3.1. chrome/unit_tests requires (via chrome_paths.cc) a directory named:
     $EXTERNAL_STORAGE + /chrome/test/data
4. Run the binary in the device and stream the log to the host.
4.1. Optionally, filter specific tests.
4.2. If we're running a single test suite and we have multiple devices
     connected, we'll shard the tests.
5. Clean up the device.

Suppressions:

Individual tests in a test binary can be suppressed by listing it in
the gtest_filter directory in a file of the same name as the test binary,
one test per line. Here is an example:

  $ cat gtest_filter/base_unittests_disabled
  DataPackTest.Load
  ReadOnlyFileUtilTest.ContentsEqual

This file is generated by the tests running on devices. If running on emulator,
additonal filter file which lists the tests only failed in emulator will be
loaded. We don't care about the rare testcases which succeeded on emuatlor, but
failed on device.
"""

import copy
import fnmatch
import logging
import optparse
import os
import signal
import subprocess
import sys
import time

from pylib import android_commands
from pylib import cmd_helper
from pylib import ports
from pylib.base.base_test_sharder import BaseTestSharder
from pylib.gtest import debug_info
from pylib.gtest import gtest_config
from pylib.gtest.single_test_runner import SingleTestRunner
from pylib.utils import emulator
from pylib.utils import run_tests_helper
from pylib.utils import test_options_parser
from pylib.utils import time_profile
from pylib.utils import xvfb


def FullyQualifiedTestSuites(exe, option_test_suite, build_type):
  """Get a list of absolute paths to test suite targets.

  Args:
    exe: if True, use the executable-based test runner.
    option_test_suite: the test_suite specified as an option.
    build_type: 'Release' or 'Debug'.
  """
  test_suite_dir = os.path.join(cmd_helper.OutDirectory.get(), build_type)
  if option_test_suite:
    all_test_suites = [option_test_suite]
  else:
    all_test_suites = gtest_config.STABLE_TEST_SUITES

  if exe:
    qualified_test_suites = [os.path.join(test_suite_dir, t)
                             for t in all_test_suites]
  else:
    # out/(Debug|Release)/$SUITE_apk/$SUITE-debug.apk
    qualified_test_suites = [os.path.join(test_suite_dir,
                                          t + '_apk',
                                          t + '-debug.apk')
                             for t in all_test_suites]
  for t, q in zip(all_test_suites, qualified_test_suites):
    if not os.path.exists(q):
      raise Exception('Test suite %s not found in %s.\n'
                      'Supported test suites:\n %s\n'
                      'Ensure it has been built.\n' %
                      (t, q, gtest_config.STABLE_TEST_SUITES))
  return qualified_test_suites


class TestSharder(BaseTestSharder):
  """Responsible for sharding the tests on the connected devices."""

  def __init__(self, attached_devices, test_suite, gtest_filter,
               test_arguments, timeout, cleanup_test_files, tool,
               log_dump_name, build_type, in_webkit_checkout,
               flakiness_server=None):
    BaseTestSharder.__init__(self, attached_devices, build_type)
    self.test_suite = test_suite
    self.gtest_filter = gtest_filter or ''
    self.test_arguments = test_arguments
    self.timeout = timeout
    self.cleanup_test_files = cleanup_test_files
    self.tool = tool
    self.log_dump_name = log_dump_name
    self.in_webkit_checkout = in_webkit_checkout
    self.flakiness_server = flakiness_server
    self.all_tests = []
    if not self.gtest_filter:
      # No filter has been specified, let's add all tests then.
      self.all_tests, self.attached_devices = self._GetAllEnabledTests()
    self.tests = self.all_tests

  def _GetAllEnabledTests(self):
    """Get all enabled tests and available devices.

    Obtains a list of enabled tests from the test package on the device,
    then filters it again using the diabled list on the host.

    Returns:
      Tuple of (all enabled tests, available devices).

    Raises Exception if all devices failed.
    """
    # TODO(frankf): This method is doing too much in a non-systematic way.
    # If the intention is to drop flaky devices, why not go through all devices
    # instead of breaking on the first succesfull run?
    available_devices = list(self.attached_devices)
    while available_devices:
      try:
        return (self._GetTestsFromDevice(available_devices[-1]),
                available_devices)
      except Exception as e:
        logging.warning('Failed obtaining tests from %s %s',
                        available_devices[-1], e)
        available_devices.pop()

    raise Exception('No device available to get the list of tests.')

  def _GetTestsFromDevice(self, device):
    logging.info('Obtaining tests from %s', device)
    test_runner = SingleTestRunner(
        device,
        self.test_suite,
        self.gtest_filter,
        self.test_arguments,
        self.timeout,
        self.cleanup_test_files,
        self.tool,
        0,
        not not self.log_dump_name,
        self.build_type,
        self.in_webkit_checkout)
    # The executable/apk needs to be copied before we can call GetAllTests.
    test_runner.test_package.StripAndCopyExecutable()
    all_tests = test_runner.test_package.GetAllTests()
    disabled_list = test_runner.GetDisabledTests()
    # Only includes tests that do not have any match in the disabled list.
    all_tests = filter(lambda t:
                       not any([fnmatch.fnmatch(t, disabled_pattern)
                                for disabled_pattern in disabled_list]),
                       all_tests)
    return all_tests

  def CreateShardedTestRunner(self, device, index):
    """Creates a suite-specific test runner.

    Args:
      device: Device serial where this shard will run.
      index: Index of this device in the pool.

    Returns:
      A SingleTestRunner object.
    """
    device_num = len(self.attached_devices)
    shard_test_list = self.tests[index::device_num]
    test_filter = ':'.join(shard_test_list) + self.gtest_filter
    return SingleTestRunner(
        device,
        self.test_suite,
        test_filter,
        self.test_arguments,
        self.timeout,
        self.cleanup_test_files, self.tool, index,
        not not self.log_dump_name,
        self.build_type,
        self.in_webkit_checkout)

  def OnTestsCompleted(self, test_runners, test_results):
    """Notifies that we completed the tests."""
    test_results.LogFull(
        test_type='Unit test',
        test_package=test_runners[0].test_package.test_suite_basename,
        build_type=self.build_type,
        flakiness_server=self.flakiness_server)
    test_results.PrintAnnotation()

    if self.log_dump_name:
      # Zip all debug info outputs into a file named by log_dump_name.
      debug_info.GTestDebugInfo.ZipAndCleanResults(
          os.path.join(
              cmd_helper.OutDirectory.get(), self.build_type,
              'debug_info_dumps'),
          self.log_dump_name)


def _RunATestSuite(options):
  """Run a single test suite.

  Helper for Dispatch() to allow stop/restart of the emulator across
  test bundles.  If using the emulator, we start it on entry and stop
  it on exit.

  Args:
    options: options for running the tests.

  Returns:
    0 if successful, number of failing tests otherwise.
  """
  step_name = os.path.basename(options.test_suite).replace('-debug.apk', '')
  attached_devices = []
  buildbot_emulators = []

  if options.use_emulator:
    buildbot_emulators = emulator.LaunchEmulators(options.emulator_count,
                                                  wait_for_boot=True)
    attached_devices = [e.device for e in buildbot_emulators]
  elif options.test_device:
    attached_devices = [options.test_device]
  else:
    attached_devices = android_commands.GetAttachedDevices()

  if not attached_devices:
    logging.critical('A device must be attached and online.')
    return 1

  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any tests.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  if options.gtest_filter:
    logging.warning('Sharding is not possible with these configurations.')
    attached_devices = [attached_devices[0]]

  sharder = TestSharder(
      attached_devices,
      options.test_suite,
      options.gtest_filter,
      options.test_arguments,
      options.timeout,
      options.cleanup_test_files,
      options.tool,
      options.log_dump,
      options.build_type,
      options.webkit,
      options.flakiness_dashboard_server)
  test_results = sharder.RunShardedTests()

  for buildbot_emulator in buildbot_emulators:
    buildbot_emulator.Shutdown()

  return len(test_results.GetAllBroken())


def Dispatch(options):
  """Dispatches the tests, sharding if possible.

  If options.use_emulator is True, all tests will be run in new emulator
  instance.

  Args:
    options: options for running the tests.

  Returns:
    0 if successful, number of failing tests otherwise.
  """
  if options.test_suite == 'help':
    ListTestSuites()
    return 0

  if options.use_xvfb:
    framebuffer = xvfb.Xvfb()
    framebuffer.Start()

  all_test_suites = FullyQualifiedTestSuites(options.exe, options.test_suite,
                                             options.build_type)
  failures = 0
  for suite in all_test_suites:
    # Give each test suite its own copy of options.
    test_options = copy.deepcopy(options)
    test_options.test_suite = suite
    failures += _RunATestSuite(test_options)

  if options.use_xvfb:
    framebuffer.Stop()
  return failures


def ListTestSuites():
  """Display a list of available test suites."""
  print 'Available test suites are:'
  for test_suite in gtest_config.STABLE_TEST_SUITES:
    print test_suite


def main(argv):
  option_parser = optparse.OptionParser()
  test_options_parser.AddGTestOptions(option_parser)
  options, args = option_parser.parse_args(argv)

  if len(args) > 1:
    option_parser.error('Unknown argument: %s' % args[1:])

  run_tests_helper.SetLogLevel(options.verbose_count)

  if options.out_directory:
    cmd_helper.OutDirectory.set(options.out_directory)

  if options.use_emulator:
    emulator.DeleteAllTempAVDs()

  failed_tests_count = Dispatch(options)

  # Failures of individual test suites are communicated by printing a
  # STEP_FAILURE message.
  # Returning a success exit status also prevents the buildbot from incorrectly
  # marking the last suite as failed if there were failures in other suites in
  # the batch (this happens because the exit status is a sum of all failures
  # from all suites, but the buildbot associates the exit status only with the
  # most recent step).
  if options.exit_code:
    return failed_tests_count
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
