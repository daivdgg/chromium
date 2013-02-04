// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_

#include "base/file_path.h"

class PrefServiceSimple;
class Profile;

namespace chrome {

// TODO(koz/benwells): These functions need to be put somewhere more
// specific than the chrome namespace.

// Do any once off initialization needed for the app list.
void InitAppList(Profile* profile);

// Show the app list.
void ShowAppList(Profile* profile);

// Register local state preferences for the app list.
void RegisterAppListPrefs(PrefServiceSimple* prefs);

// Change the profile that the app list is showing.
void SetAppListProfile(const FilePath& profile_file_path);

// Get the path of the profile to be used with the app list.
FilePath GetAppListProfilePath(const FilePath& user_data_dir);

// Dismiss the app list.
void DismissAppList();

// Get the profile the app list is currently showing.
Profile* GetCurrentAppListProfile();

// Returns true if the app list is visible.
bool IsAppListVisible();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_
