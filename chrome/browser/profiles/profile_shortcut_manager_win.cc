// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_win.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace {

// Characters that are not allowed in Windows filenames. Taken from
// http://msdn.microsoft.com/en-us/library/aa365247.aspx
const char16 kReservedCharacters[] = L"<>:\"/\\|?*\x01\x02\x03\x04\x05\x06\x07"
    L"\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
    L"\x1A\x1B\x1C\x1D\x1E\x1F";

// The maximum number of characters allowed in profile shortcuts' file names.
// Warning: migration code will be needed if this is changed later, since
// existing shortcuts might no longer be found if the name is generated
// differently than it was when a shortcut was originally created.
const int kMaxProfileShortcutFileNameLength = 64;

const int kProfileAvatarShortcutBadgeWidth = 28;
const int kProfileAvatarShortcutBadgeHeight = 28;
const int kShortcutIconSize = 48;

// Creates a desktop shortcut icon file (.ico) on the disk for a given profile,
// badging the browser distribution icon with the profile avatar.
// Returns a path to the shortcut icon file on disk, which is empty if this
// fails. Use index 0 when assigning the resulting file as the icon.
FilePath CreateChromeDesktopShortcutIconForProfile(
    const FilePath& profile_path,
    const SkBitmap& avatar_bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<SkBitmap> app_icon_bitmap(GetAppIconForSize(kShortcutIconSize));
  if (!app_icon_bitmap.get())
    return FilePath();

  // TODO(rlp): Share this chunk of code with
  // avatar_menu_button::DrawTaskBarDecoration.
  const SkBitmap* source_bitmap = NULL;
  SkBitmap squarer_bitmap;
  if ((avatar_bitmap.width() == profiles::kAvatarIconWidth) &&
      (avatar_bitmap.height() == profiles::kAvatarIconHeight)) {
    // Shave a couple of columns so the bitmap is more square. So when
    // resized to a square aspect ratio it looks pretty.
    int x = 2;
    avatar_bitmap.extractSubset(&squarer_bitmap, SkIRect::MakeXYWH(x, 0,
        profiles::kAvatarIconWidth - x * 2, profiles::kAvatarIconHeight));
    source_bitmap = &squarer_bitmap;
  } else {
    source_bitmap = &avatar_bitmap;
  }
  SkBitmap sk_icon = skia::ImageOperations::Resize(
      *source_bitmap,
      skia::ImageOperations::RESIZE_LANCZOS3,
      kProfileAvatarShortcutBadgeWidth,
      kProfileAvatarShortcutBadgeHeight);

  // Overlay the avatar on the icon, anchoring it to the bottom-right of the
  // icon.
  scoped_ptr<SkCanvas> offscreen_canvas(
      skia::CreateBitmapCanvas(app_icon_bitmap->width(),
                               app_icon_bitmap->height(),
                               false));
  DCHECK(offscreen_canvas.get());
  offscreen_canvas->drawBitmap(*app_icon_bitmap, 0, 0);
  offscreen_canvas->drawBitmap(
      sk_icon,
      app_icon_bitmap->width() - kProfileAvatarShortcutBadgeWidth,
      app_icon_bitmap->height() - kProfileAvatarShortcutBadgeHeight);
  const SkBitmap& final_bitmap =
      offscreen_canvas->getDevice()->accessBitmap(false);

  // Finally, write the .ico file containing this new bitmap.
  const FilePath icon_path =
      profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
  // TODO(asvitkine): Create icon with a large 256x256 bitmap.
  if (!IconUtil::CreateIconFileFromSkBitmap(final_bitmap, SkBitmap(),
                                            icon_path))
    return FilePath();

  return icon_path;
}

// Gets the user and system directories for desktop shortcuts. Parameters may
// be NULL if a directory type is not needed. Returns true on success.
bool GetDesktopShortcutsDirectories(FilePath* user_shortcuts_directory,
                                    FilePath* system_shortcuts_directory) {
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (user_shortcuts_directory &&
      !ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                  distribution, ShellUtil::CURRENT_USER,
                                  user_shortcuts_directory)) {
    NOTREACHED();
    return false;
  }
  if (system_shortcuts_directory &&
      !ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                  distribution, ShellUtil::SYSTEM_LEVEL,
                                  system_shortcuts_directory)) {
    NOTREACHED();
    return false;
  }
  return true;
}

// Returns the long form of |path|, which will expand any shortened components
// like "foo~2" to their full names.
FilePath ConvertToLongPath(const FilePath& path) {
  const size_t length = GetLongPathName(path.value().c_str(), NULL, 0);
  if (length != 0 && length != path.value().length()) {
    std::vector<wchar_t> long_path(length);
    if (GetLongPathName(path.value().c_str(), &long_path[0], length) != 0)
      return FilePath(&long_path[0]);
  }
  return path;
}

// Returns true if the file at |path| is a Chrome shortcut and returns its
// command line in output parameter |command_line|.
bool IsChromeShortcut(const FilePath& path,
                      const FilePath& chrome_exe,
                      string16* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (path.Extension() != installer::kLnkExt)
    return false;

  FilePath target_path;
  if (!base::win::ResolveShortcut(path, &target_path, command_line))
    return false;
  // One of the paths may be in short (elided) form. Compare long paths to
  // ensure these are still properly matched.
  return ConvertToLongPath(target_path) == ConvertToLongPath(chrome_exe);
}

// Populates |paths| with the file paths of Chrome desktop shortcuts that have
// the specified |command_line|. If |include_empty_command_lines| is true,
// Chrome desktop shortcuts with empty command lines will also be included.
void ListDesktopShortcutsWithCommandLine(const FilePath& chrome_exe,
                                         const string16& command_line,
                                         bool include_empty_command_lines,
                                         std::vector<FilePath>* paths) {
  FilePath user_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory, NULL))
    return;

  file_util::FileEnumerator enumerator(user_shortcuts_directory, false,
      file_util::FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    string16 shortcut_command_line;
    if (!IsChromeShortcut(path, chrome_exe, &shortcut_command_line))
      continue;

    // TODO(asvitkine): Change this to build a CommandLine object and ensure all
    // args from |command_line| are present in the shortcut's CommandLine. This
    // will be more robust when |command_line| contains multiple args.
    if ((shortcut_command_line.empty() && include_empty_command_lines) ||
        (shortcut_command_line.find(command_line) != string16::npos)) {
      paths->push_back(path);
    }
  }
}

// Renames an existing Chrome desktop profile shortcut. Must be called on the
// FILE thread.
void RenameChromeDesktopShortcutForProfile(
    const string16& old_shortcut_filename,
    const string16& new_shortcut_filename) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath user_shortcuts_directory;
  FilePath system_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory,
                                      &system_shortcuts_directory)) {
    return;
  }

  const FilePath old_shortcut_path =
      user_shortcuts_directory.Append(old_shortcut_filename);
  const FilePath new_shortcut_path =
      user_shortcuts_directory.Append(new_shortcut_filename);

  if (file_util::PathExists(old_shortcut_path)) {
    // Rename the old shortcut unless a system-level shortcut exists at the
    // destination, in which case the old shortcut is simply deleted.
    const FilePath possible_new_system_shortcut =
        system_shortcuts_directory.Append(new_shortcut_filename);
    if (file_util::PathExists(possible_new_system_shortcut))
      file_util::Delete(old_shortcut_path, false);
    else if (!file_util::Move(old_shortcut_path, new_shortcut_path))
      DLOG(ERROR) << "Could not rename Windows profile desktop shortcut.";
  } else {
    // If the shortcut does not exist, it may have been renamed by the user. In
    // that case, its name should not be changed.
    // It's also possible that a system-level shortcut exists instead - this
    // should only be the case for the original Chrome shortcut from an
    // installation. If that's the case, copy that one over - it will get its
    // properties updated by |CreateOrUpdateDesktopShortcutsForProfile()|.
    const FilePath possible_old_system_shortcut =
        system_shortcuts_directory.Append(old_shortcut_filename);
    if (file_util::PathExists(possible_old_system_shortcut))
      file_util::CopyFile(possible_old_system_shortcut, new_shortcut_path);
  }
}

// Updates all desktop shortcuts for the given profile to have the specified
// parameters. If |create_mode| is CREATE_WHEN_NONE_FOUND, a new shortcut is
// created if no existing ones were found. Whether non-profile shortcuts should
// be updated is specified by |action|. Must be called on the FILE thread.
void CreateOrUpdateDesktopShortcutsForProfile(
    const FilePath& profile_path,
    const string16& old_profile_name,
    const string16& profile_name,
    const SkBitmap& avatar_image,
    ProfileShortcutManagerWin::CreateOrUpdateMode create_mode,
    ProfileShortcutManagerWin::NonProfileShortcutAction action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  // Ensure that the distribution supports creating shortcuts. If it doesn't,
  // the following code may result in NOTREACHED() being hit.
  DCHECK(distribution->CanCreateDesktopShortcuts());

  if (old_profile_name != profile_name) {
    const string16 old_shortcut_filename =
        profiles::internal::GetShortcutFilenameForProfile(old_profile_name,
                                                          distribution);
    const string16 new_shortcut_filename =
        profiles::internal::GetShortcutFilenameForProfile(profile_name,
                                                          distribution);
    RenameChromeDesktopShortcutForProfile(old_shortcut_filename,
                                          new_shortcut_filename);
  }

  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  installer::Product product(distribution);
  product.AddDefaultShortcutProperties(chrome_exe, &properties);

  const string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);

  // Only set the profile-specific properties when |profile_name| is non empty.
  // If it is empty, it means the shortcut being created should be a regular,
  // non-profile Chrome shortcut.
  if (!profile_name.empty()) {
    const FilePath shortcut_icon =
        CreateChromeDesktopShortcutIconForProfile(profile_path, avatar_image);
    if (!shortcut_icon.empty())
      properties.set_icon(shortcut_icon, 0);
    properties.set_arguments(command_line);
  } else {
    // Set the arguments explicitly to the empty string to ensure that
    // |ShellUtil::CreateOrUpdateShortcut| updates that part of the shortcut.
    properties.set_arguments(string16());
  }

  ShellUtil::ShortcutOperation operation =
      ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING;

  std::vector<FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line,
      action == ProfileShortcutManagerWin::UPDATE_NON_PROFILE_SHORTCUTS,
      &shortcuts);
  if (create_mode == ProfileShortcutManagerWin::CREATE_WHEN_NONE_FOUND &&
      shortcuts.empty()) {
    const string16 shortcut_name =
        profiles::internal::GetShortcutFilenameForProfile(profile_name,
                                                          distribution);
    shortcuts.push_back(FilePath(shortcut_name));
    operation = ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL;
  }

  for (size_t i = 0; i < shortcuts.size(); ++i) {
    const FilePath shortcut_name = shortcuts[i].BaseName().RemoveExtension();
    properties.set_shortcut_name(shortcut_name.value());
    ShellUtil::CreateOrUpdateShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
        distribution, properties, operation);
  }
}

// Returns true if any desktop shortcuts exist with target |chrome_exe|,
// regardless of their command line arguments.
bool ChromeDesktopShortcutsExist(const FilePath& chrome_exe) {
  FilePath user_shortcuts_directory;
  if (!GetDesktopShortcutsDirectories(&user_shortcuts_directory, NULL))
    return false;

  file_util::FileEnumerator enumerator(user_shortcuts_directory, false,
      file_util::FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (IsChromeShortcut(path, chrome_exe, NULL))
      return true;
  }

  return false;
}

// Deletes all desktop shortcuts for the specified profile and also removes the
// corresponding icon file. If |ensure_shortcuts_remain| is true, then a regular
// non-profile shortcut will be created if this function would otherwise delete
// the last Chrome desktop shortcut(s). Must be called on the FILE thread.
void DeleteDesktopShortcutsAndIconFile(const FilePath& profile_path,
                                       bool ensure_shortcuts_remain) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  const string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  std::vector<FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line, false,
                                      &shortcuts);

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  for (size_t i = 0; i < shortcuts.size(); ++i) {
    const string16 shortcut_name =
        shortcuts[i].BaseName().RemoveExtension().value();
    ShellUtil::RemoveShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                              distribution, chrome_exe, ShellUtil::CURRENT_USER,
                              &shortcut_name);
  }

  const FilePath icon_path =
      profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
  file_util::Delete(icon_path, false);

  // If |ensure_shortcuts_remain| is true and deleting this profile caused the
  // last shortcuts to be removed, re-create a regular non-profile shortcut.
  const bool had_shortcuts = !shortcuts.empty();
  if (ensure_shortcuts_remain && had_shortcuts &&
      !ChromeDesktopShortcutsExist(chrome_exe)) {
    BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
    // Ensure that the distribution supports creating shortcuts. If it doesn't,
    // the following code may result in NOTREACHED() being hit.
    DCHECK(distribution->CanCreateDesktopShortcuts());
    installer::Product product(distribution);

    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    product.AddDefaultShortcutProperties(chrome_exe, &properties);
    properties.set_shortcut_name(
        profiles::internal::GetShortcutFilenameForProfile(string16(),
                                                          distribution));
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, distribution, properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);
  }
}

// Returns true if profile at |profile_path| has any shortcuts. Does not
// consider non-profile shortcuts. Must be called on the FILE thread.
bool HasAnyProfileShortcuts(const FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  const string16 command_line =
      profiles::internal::CreateProfileShortcutFlags(profile_path);
  std::vector<FilePath> shortcuts;
  ListDesktopShortcutsWithCommandLine(chrome_exe, command_line, false,
                                      &shortcuts);
  return !shortcuts.empty();
}

// Replaces any reserved characters with spaces, and trims the resulting string
// to prevent any leading and trailing spaces. Also makes sure that the
// resulting filename doesn't exceed |kMaxProfileShortcutFileNameLength|.
// TODO(macourteau): find a way to limit the total path's length to MAX_PATH
// instead of limiting the profile's name to |kMaxProfileShortcutFileNameLength|
// characters.
string16 SanitizeShortcutProfileNameString(const string16& profile_name) {
  string16 sanitized = profile_name;
  size_t pos = sanitized.find_first_of(kReservedCharacters);
  while (pos != string16::npos) {
    sanitized[pos] = L' ';
    pos = sanitized.find_first_of(kReservedCharacters, pos + 1);
  }

  TrimWhitespace(sanitized, TRIM_LEADING, &sanitized);
  if (sanitized.size() > kMaxProfileShortcutFileNameLength)
    sanitized.erase(kMaxProfileShortcutFileNameLength);
  TrimWhitespace(sanitized, TRIM_TRAILING, &sanitized);

  return sanitized;
}

}  // namespace

namespace profiles {
namespace internal {

const char kProfileIconFileName[] = "Google Profile.ico";

string16 GetShortcutFilenameForProfile(const string16& profile_name,
                                       BrowserDistribution* distribution) {
  string16 shortcut_name;
  if (!profile_name.empty()) {
    shortcut_name.append(SanitizeShortcutProfileNameString(profile_name));
    shortcut_name.append(L" - ");
  }
  shortcut_name.append(distribution->GetAppShortCutName());
  return shortcut_name + installer::kLnkExt;
}

string16 CreateProfileShortcutFlags(const FilePath& profile_path) {
  return base::StringPrintf(L"--%ls=\"%ls\"",
                            ASCIIToUTF16(switches::kProfileDirectory).c_str(),
                            profile_path.BaseName().value().c_str());
}

}  // namespace internal
}  // namespace profiles

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  return BrowserDistribution::GetDistribution()->CanCreateDesktopShortcuts() &&
         !CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir);
}

// static
ProfileShortcutManager* ProfileShortcutManager::Create(
    ProfileManager* manager) {
  return new ProfileShortcutManagerWin(manager);
}

ProfileShortcutManagerWin::ProfileShortcutManagerWin(ProfileManager* manager)
    : profile_manager_(manager) {
  profile_manager_->GetProfileInfoCache().AddObserver(this);
}

ProfileShortcutManagerWin::~ProfileShortcutManagerWin() {
  profile_manager_->GetProfileInfoCache().RemoveObserver(this);
}

void ProfileShortcutManagerWin::CreateProfileShortcut(
    const FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, CREATE_WHEN_NONE_FOUND,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::RemoveProfileShortcuts(
    const FilePath& profile_path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeleteDesktopShortcutsAndIconFile, profile_path, false));
}

void ProfileShortcutManagerWin::HasProfileShortcuts(
    const FilePath& profile_path,
    const base::Callback<void(bool)>& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&HasAnyProfileShortcuts, profile_path), callback);
}

void ProfileShortcutManagerWin::OnProfileAdded(const FilePath& profile_path) {
  const size_t profile_count =
      profile_manager_->GetProfileInfoCache().GetNumberOfProfiles();
  if (profile_count == 1) {
    CreateOrUpdateShortcutsForProfileAtPath(profile_path,
                                            CREATE_WHEN_NONE_FOUND,
                                            UPDATE_NON_PROFILE_SHORTCUTS);
  } else if (profile_count == 2) {
    CreateOrUpdateShortcutsForProfileAtPath(GetOtherProfilePath(profile_path),
                                            UPDATE_EXISTING_ONLY,
                                            UPDATE_NON_PROFILE_SHORTCUTS);
  }
}

void ProfileShortcutManagerWin::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
}

void ProfileShortcutManagerWin::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  // If there is only one profile remaining, remove the badging information
  // from an existing shortcut.
  const bool deleting_down_to_last_profile = (cache.GetNumberOfProfiles() == 1);
  if (deleting_down_to_last_profile) {
    CreateOrUpdateShortcutsForProfileAtPath(cache.GetPathOfProfileAtIndex(0),
                                            UPDATE_EXISTING_ONLY,
                                            IGNORE_NON_PROFILE_SHORTCUTS);
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DeleteDesktopShortcutsAndIconFile,
                                     profile_path,
                                     deleting_down_to_last_profile));
}

void ProfileShortcutManagerWin::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, UPDATE_EXISTING_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

void ProfileShortcutManagerWin::OnProfileAvatarChanged(
    const FilePath& profile_path) {
  CreateOrUpdateShortcutsForProfileAtPath(profile_path, UPDATE_EXISTING_ONLY,
                                          IGNORE_NON_PROFILE_SHORTCUTS);
}

FilePath ProfileShortcutManagerWin::GetOtherProfilePath(
    const FilePath& profile_path) {
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  DCHECK_EQ(2U, cache.GetNumberOfProfiles());
  // Get the index of the current profile, in order to find the index of the
  // other profile.
  size_t current_profile_index = cache.GetIndexOfProfileWithPath(profile_path);
  size_t other_profile_index = (current_profile_index == 0) ? 1 : 0;
  return cache.GetPathOfProfileAtIndex(other_profile_index);
}

void ProfileShortcutManagerWin::CreateOrUpdateShortcutsForProfileAtPath(
    const FilePath& profile_path,
    CreateOrUpdateMode create_mode,
    NonProfileShortcutAction action) {
  ProfileInfoCache* cache = &profile_manager_->GetProfileInfoCache();
  size_t profile_index = cache->GetIndexOfProfileWithPath(profile_path);
  if (profile_index == std::string::npos)
    return;
  bool remove_badging = cache->GetNumberOfProfiles() == 1;

  string16 old_shortcut_appended_name =
      cache->GetShortcutNameOfProfileAtIndex(profile_index);

  string16 new_shortcut_appended_name;
  if (!remove_badging)
    new_shortcut_appended_name = cache->GetNameOfProfileAtIndex(profile_index);

  SkBitmap profile_avatar_bitmap_copy;
  if (!remove_badging) {
    size_t profile_icon_index =
        cache->GetAvatarIconIndexOfProfileAtIndex(profile_index);
    gfx::Image profile_avatar_image = ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(
            cache->GetDefaultAvatarIconResourceIDAtIndex(profile_icon_index));

    DCHECK(!profile_avatar_image.IsEmpty());
    const SkBitmap* profile_avatar_bitmap = profile_avatar_image.ToSkBitmap();
    // Make a copy of the SkBitmap to ensure that we can safely use the image
    // data on the FILE thread.
    profile_avatar_bitmap->deepCopyTo(&profile_avatar_bitmap_copy,
                                      profile_avatar_bitmap->getConfig());
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateOrUpdateDesktopShortcutsForProfile,
                 profile_path, old_shortcut_appended_name,
                 new_shortcut_appended_name, profile_avatar_bitmap_copy,
                 create_mode, action));

  cache->SetShortcutNameOfProfileAtIndex(profile_index,
                                         new_shortcut_appended_name);
}
