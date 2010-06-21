// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_

#include "chrome/test/in_process_browser_test.h"

#include "base/scoped_vector.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "net/base/mock_host_resolver.h"
#include "net/socket/ssl_test_util.h"

class Profile;
class CommandLine;

namespace net {
class ScopedDefaultHostResolverProc;
}

namespace switches {
extern const wchar_t kSyncUserForTest[];
extern const wchar_t kSyncPasswordForTest[];
}

// This is the base class for integration tests for all sync data types. Derived
// classes must be defined for each sync data type. Individual tests are defined
// using the IN_PROC_BROWSER_TEST_F macro.
class LiveSyncTest : public InProcessBrowserTest {
 public:
  // The different types of live sync tests that can be implemented.
  enum TestType {
    // Tests where only one client profile is synced with the server. Typically
    // sanity level tests.
    SINGLE_CLIENT,

    // Tests where two client profiles are synced with the server. Typically
    // functionality level tests.
    TWO_CLIENT,

    // Tests where three client profiles are synced with the server. Typically,
    // these tests create client side races and verify that sync works.
    MULTIPLE_CLIENT,

    // Tests where several client profiles are synced with the server. Only used
    // by stress tests.
    MANY_CLIENT
  };

  // A LiveSyncTest must be associated with a particular test type.
  explicit LiveSyncTest(TestType test_type)
      : test_type_(test_type),
        num_clients_(-1),
        started_local_test_server_(false) {
    InProcessBrowserTest::set_show_window(true);
    switch (test_type_) {
      case SINGLE_CLIENT: {
        num_clients_ = 1;
        break;
      }
      case TWO_CLIENT: {
        num_clients_ = 2;
        break;
      }
      case MULTIPLE_CLIENT: {
        num_clients_ = 3;
        break;
      }
      case MANY_CLIENT: {
        num_clients_ = 10;
        break;
      }
    }
  }

  virtual ~LiveSyncTest() {}

  // Validates command line parameters and creates a local python test server if
  // specified.
  virtual void SetUp();

  // Brings down local python test server if one was created.
  virtual void TearDown();

  // Append command line flag to enable sync.
  virtual void SetUpCommandLine(CommandLine* command_line) {}

  // Helper to ProfileManager::CreateProfile that handles path creation.
  static Profile* MakeProfile(const FilePath::StringType name);

  // Used to access a particular sync profile.
  Profile* GetProfile(int index);

  // Used to access a particular sync client.
  ProfileSyncServiceTestHarness* GetClient(int index);

  // Used to verify changes to individual sync profiles.
  Profile* verifier();

  // Initializes sync clients and profiles but does not sync any of them.
  virtual bool SetupClients();

  // Initializes sync clients and profiles if required and syncs each of them.
  virtual bool SetupSync();

 protected:
  // InProcessBrowserTest override. Destroys all the sync clients and sync
  // profiles created by a test.
  virtual void CleanUpOnMainThread();

  // InProcessBrowserTest override. Changes behavior of the default host
  // resolver to avoid DNS lookup errors.
  virtual void SetUpInProcessBrowserTestFixture();

  // InProcessBrowserTest override. Resets the host resolver its default
  // behavior.
  virtual void TearDownInProcessBrowserTestFixture();

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

private:
  // Helper method used to create a local python test server.
  virtual void SetUpLocalTestServer();

  // Helper method used to destroy the local python test server if one was
  // created.
  virtual void TearDownLocalTestServer();

  // Used to differentiate between single-client, two-client, multi-client and
  // many-client tests.
  TestType test_type_;

  // Number of sync clients that will be created by a test.
  int num_clients_;

  // Collection of sync profiles used by a test. A sync profile maintains sync
  // data contained within its own subdirectory under the chrome user data
  // directory.
  ScopedVector<Profile> profiles_;

  // Collection of sync clients used by a test. A sync client is associated with
  // a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  ScopedVector<ProfileSyncServiceTestHarness> clients_;

  // Sync profile against which changes to individual profiles are verified. We
  // don't need a corresponding verifier sync client because the contents of the
  // verifier profile are strictly local, and are not meant to be synced.
  scoped_ptr<Profile> verifier_;

  // Local instance of python sync server.
  net::TestServerLauncher server_;

  // Keeps track of whether a local python sync server was used for a test.
  bool started_local_test_server_;

  // Sync integration tests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  DISALLOW_COPY_AND_ASSIGN(LiveSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
