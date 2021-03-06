// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::AnyNumber;
using testing::Return;
using testing::_;

namespace chromeos {

namespace {

const char kUser1ProfilePath[] = "/profile/user1/shill";

void AssignString(std::string* out,
                  DBusMethodCallStatus call_status,
                  const std::string& result) {
  CHECK_EQ(call_status, DBUS_METHOD_CALL_SUCCESS);
  *out = result;
}

}  // namespace

class ExtensionNetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(::switches::kWhitelistedExtensionID,
                                    "epcifkihnkjgphfkloaaleeakhpmgdmn");
    command_line->AppendSwitch(switches::kUseNewNetworkConfigurationHandlers);

    // TODO(pneubeck): Remove the following hack, once the NetworkingPrivateAPI
    // uses the ProfileHelper to obtain the userhash crbug/238623.
    std::string login_user =
        command_line->GetSwitchValueNative(switches::kLoginUser);
    // Do the same as CryptohomeClientStubImpl::GetSanitizedUsername
    std::string sanitized_user = login_user + "-profile";
    command_line->AppendSwitchASCII(switches::kLoginProfile, sanitized_user);
  }

  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest(
        "networking", "main.html?" + subtest,
        kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider_, RegisterPolicyDomain(_, _)).Times(AnyNumber());
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void InitializeSanitizedUsername() {
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    chromeos::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    DBusThreadManager::Get()->GetCryptohomeClient()->GetSanitizedUsername(
        user->email(),
        base::Bind(&AssignString, &userhash_));
    content::RunAllPendingInMessageLoop();
    CHECK(!userhash_.empty());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    InitializeSanitizedUsername();

    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ShillProfileClient::TestInterface* profile_test =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

    g_browser_process->browser_policy_connector()->
        GetNetworkConfigurationUpdater()->OnUserPolicyInitialized(
            false, userhash_);
    device_test->ClearDevices();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUser1ProfilePath, userhash_);

    device_test->AddDevice("/device/stub_wifi_device1",
                           flimflam::kTypeWifi, "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           flimflam::kTypeCellular, "stub_cellular_device1");

    const bool add_to_watchlist = true;
    service_test->AddService("stub_ethernet", "eth0",
                             flimflam::kTypeEthernet, flimflam::kStateOnline,
                             add_to_watchlist);

    service_test->AddService("stub_wifi1", "wifi1",
                             flimflam::kTypeWifi, flimflam::kStateOnline,
                             add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi1",
                                     flimflam::kSecurityProperty,
                                     base::StringValue(flimflam::kSecurityWep));

    service_test->AddService("stub_wifi2", "wifi2_PSK",
                             flimflam::kTypeWifi, flimflam::kStateIdle,
                             add_to_watchlist);
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kGuidProperty,
                                     base::StringValue("stub_wifi2"));
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kSecurityProperty,
                                     base::StringValue(flimflam::kSecurityPsk));
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kSignalStrengthProperty,
                                     base::FundamentalValue(80));
    service_test->SetServiceProperty("stub_wifi2",
                                     flimflam::kProfileProperty,
                                     base::StringValue(kUser1ProfilePath));
    profile_test->AddService("stub_wifi2");

    service_test->AddService("stub_cellular1", "cellular1",
                             flimflam::kTypeCellular, flimflam::kStateIdle,
                             add_to_watchlist);
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kNetworkTechnologyProperty,
        base::StringValue(flimflam::kNetworkTechnologyGsm));
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kActivationStateProperty,
        base::StringValue(flimflam::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        "stub_cellular1",
        flimflam::kRoamingStateProperty,
        base::StringValue(flimflam::kRoamingStateHome));

    service_test->AddService("stub_vpn1", "vpn1",
                             flimflam::kTypeVPN,
                             flimflam::kStateOnline,
                             add_to_watchlist);

    content::RunAllPendingInMessageLoop();
  }

 protected:
  policy::MockConfigurationPolicyProvider provider_;
  std::string userhash_;
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       GetManagedProperties) {
  ShillServiceClient::TestInterface* service_test =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  const std::string uidata_blob =
      "{ \"user_settings\": {"
      "      \"WiFi\": {"
      "        \"Passphrase\": \"FAKE_CREDENTIAL_VPaJDV9x\" }"
      "    }"
      "}";
  service_test->SetServiceProperty("stub_wifi2",
                                   flimflam::kUIDataProperty,
                                   base::StringValue(uidata_blob));
  service_test->SetServiceProperty("stub_wifi2",
                                   flimflam::kAutoConnectProperty,
                                   base::FundamentalValue(false));

  ShillProfileClient::TestInterface* profile_test =
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
  // Update the profile entry.
  profile_test->AddService("stub_wifi2");

  content::RunAllPendingInMessageLoop();

  const std::string user_policy_blob =
      "{ \"NetworkConfigurations\": ["
      "    { \"GUID\": \"stub_wifi2\","
      "      \"Type\": \"WiFi\","
      "      \"Name\": \"My WiFi Network\","
      "      \"WiFi\": {"
      "        \"Passphrase\": \"passphrase\","
      "        \"Recommended\": [ \"AutoConnect\", \"Passphrase\" ],"
      "        \"SSID\": \"stub_wifi2\","
      "        \"Security\": \"WPA-PSK\" }"
      "    }"
      "  ],"
      "  \"Certificates\": [],"
      "  \"Type\": \"UnencryptedConfiguration\""
      "}";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             Value::CreateStringValue(user_policy_blob));
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest,
                       VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}


}  // namespace chromeos
