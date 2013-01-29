// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_
#define CHROMEOS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_

#include <string>

namespace chromeos {
namespace onc {

struct StringTranslationEntry {
  const char* onc_value;
  const char* shill_value;
};

// These tables contain the mapping from ONC strings to Shill strings.
// These are NULL-terminated arrays.
extern const StringTranslationEntry kNetworkTypeTable[];
extern const StringTranslationEntry kVPNTypeTable[];
extern const StringTranslationEntry kWiFiSecurityTable[];
extern const StringTranslationEntry kEAPOuterTable[];
extern const StringTranslationEntry kEAP_PEAP_InnerTable[];
extern const StringTranslationEntry kEAP_TTLS_InnerTable[];

// Translate individual strings to Shill using the above tables.
bool TranslateStringToShill(const StringTranslationEntry table[],
                            const std::string& onc_value,
                            std::string* shill_value);

// Translate individual strings to ONC using the above tables.
bool TranslateStringToONC(const StringTranslationEntry table[],
                          const std::string& shill_value,
                          std::string* onc_value);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_
