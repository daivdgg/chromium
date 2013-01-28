// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include <set>

#include "base/file_path.h"
#include "googleurl/src/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/quota/mock_special_storage_policy.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using fileapi::ExternalMountPoints;
using fileapi::FileSystemURL;

namespace {

FileSystemURL CreateFileSystemURL(const std::string& extension,
                                  const char* path,
                                  ExternalMountPoints* mount_points) {
  return mount_points->CreateCrackedFileSystemURL(
      GURL("chrome-extension://" + extension + "/"),
      fileapi::kFileSystemTypeExternal,
      FilePath::FromUTF8Unsafe(path));
}

TEST(CrosMountPointProviderTest, DefaultMountPoints) {
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      fileapi::ExternalMountPoints::GetSystemInstance());
  std::vector<FilePath> root_dirs = provider.GetRootDirectories();
  std::set<FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());

  // By default there should be 3 mount points (in system mount points):
  EXPECT_EQ(3u, root_dirs.size());
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/media/removable"))));
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/media/archive"))));
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/usr/share/oem"))));
}

TEST(CrosMountPointProviderTest, GetRootDirectories) {
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      system_mount_points.get());

  // Register 'local' test mount points.
  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(FPL("/a/b/c")));
  mount_points->RegisterFileSystem("d",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(FPL("/b/c/d")));

  // Register system test mount points.
  system_mount_points->RegisterFileSystem("d",
                                          fileapi::kFileSystemTypeNativeLocal,
                                          FilePath(FPL("/g/c/d")));
  system_mount_points->RegisterFileSystem("e",
                                          fileapi::kFileSystemTypeNativeLocal,
                                          FilePath(FPL("/g/d/e")));

  std::vector<FilePath> root_dirs = provider.GetRootDirectories();
  std::set<FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());
  EXPECT_EQ(4u, root_dirs.size());
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/a/b/c"))));
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/b/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/g/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(FilePath(FPL("/g/d/e"))));
}

TEST(CrosMountPointProviderTest, MountPointsVisibility) {
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<fileapi::ExternalMountPoints> sibling_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      system_mount_points.get());

  // A provider that shares system_mount_points with |provider|.
  chromeos::CrosMountPointProvider sibling_provider(
      storage_policy,
      sibling_mount_points.get(),
      system_mount_points.get());

  FilePath ignored;

  // Adding empty mount point should fail.
  EXPECT_FALSE(provider.AddLocalMountPoint(FilePath()));

  // Add mount point to the provider.
  EXPECT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/a/b/c"))));

  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/a/b/c"))));
  // The mount point with the same name exists, but path is different.
  EXPECT_FALSE(provider.HasMountPoint(FilePath(FPL("/x/a/b/c"))));
  EXPECT_FALSE(sibling_provider.HasMountPoint(FilePath(FPL("/a/b/c"))));
  EXPECT_TRUE(mount_points->GetRegisteredPath("c", &ignored));
  EXPECT_FALSE(system_mount_points->GetRegisteredPath("c", &ignored));

  // Add mount point directly to |mount_points|. It should be seen by
  // |provider|.
  EXPECT_TRUE(mount_points->RegisterFileSystem(
      "d", fileapi::kFileSystemTypeNativeLocal, FilePath(FPL("/b/c/d"))));

  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/b/c/d"))));
  EXPECT_FALSE(sibling_provider.HasMountPoint(FilePath(FPL("/b/c/d"))));

  // Add mount point to system mount points.
  EXPECT_TRUE(system_mount_points->RegisterFileSystem(
      "e", fileapi::kFileSystemTypeNativeLocal, FilePath(FPL("/g/c/d/e"))));

  EXPECT_FALSE(provider.HasMountPoint(FilePath(FPL("/g/c/d/e"))));
  EXPECT_FALSE(sibling_provider.HasMountPoint(FilePath(FPL("/g/c/d/e"))));

  // Can't remove system mount point.
  provider.RemoveMountPoint(FilePath(FPL("/g/c/d/e")));
  EXPECT_TRUE(system_mount_points->GetRegisteredPath("e", &ignored));

  // Add a mount points whose paths overlap with the system one's.
  // The same path:
  EXPECT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/g/c/d/e"))));
  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/g/c/d/e"))));
  provider.RemoveMountPoint(FilePath(FPL("/g/c/d/e")));
  EXPECT_FALSE(provider.HasMountPoint(FilePath(FPL("/g/c/d/e"))));
  // Parent path:
  EXPECT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/g"))));
  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/g"))));
  provider.RemoveMountPoint(FilePath(FPL("/g")));
  // Child path:
  EXPECT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/g/c/d/e/f/g"))));
  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/g/c/d/e/f/g"))));
  provider.RemoveMountPoint(FilePath(FPL("/g/c/d/e/f/g")));

  // Add mount point with the same name as a global one. Should succeed.
  EXPECT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/d/e"))));

  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/d/e"))));

  // Remove system mount point with the same name as the added one.
  // Should fail.
  provider.RemoveMountPoint(FilePath(FPL("/g/c/d/e")));

  EXPECT_TRUE(provider.HasMountPoint(FilePath(FPL("/d/e"))));
  EXPECT_TRUE(system_mount_points->GetRegisteredPath("e", &ignored));

  // Remove mount point.
  provider.RemoveMountPoint(FilePath(FPL("/d/e")));

  EXPECT_FALSE(provider.HasMountPoint(FilePath(FPL("/d/e"))));
}

TEST(CrosMountPointProviderTest, AccessPermissions) {
  url_util::AddStandardScheme("chrome-extension");

  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      system_mount_points.get());

  std::string extension("ddammdhioacbehjngdmkjcjbnfginlla");

  storage_policy->AddFileHandler(extension);

  // Initialize mount points.
  system_mount_points->RegisterFileSystem("system",
                                          fileapi::kFileSystemTypeNativeLocal,
                                          FilePath(FPL("/g/system")));
  ASSERT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/media/removable"))));
  ASSERT_TRUE(provider.AddRestrictedLocalMountPoint(
      FilePath(FPL("/usr/share/oem"))));

  // Provider specific mount point access.
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));

  provider.GrantFileAccessToExtension(extension,
                                      FilePath(FPL("removable/foo")));
  EXPECT_TRUE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo1", mount_points.get())));

  // System mount point access.
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));

  provider.GrantFileAccessToExtension(extension, FilePath(FPL("system/foo")));
  EXPECT_TRUE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));
  EXPECT_FALSE(provider.IsAccessAllowed(CreateFileSystemURL(
      extension, "system/foo1", system_mount_points.get())));

  // oem is restricted file system.
  provider.GrantFileAccessToExtension(extension, FilePath(FPL("oem/foo")));
  // The extension should not be able to access the file even if
  // GrantFileAccessToExtension was called.
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "oem/foo", mount_points.get())));

  provider.GrantFullAccessToExtension(extension);
  // The extension should be able to access restricted file system after it was
  // granted full access.
  EXPECT_TRUE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "oem/foo", mount_points.get())));
  // The extension which was granted full access  should be able to access any
  // path on curent file systems.
  EXPECT_TRUE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo1", mount_points.get())));
  EXPECT_TRUE(provider.IsAccessAllowed(CreateFileSystemURL(
      extension, "system/foo1", system_mount_points.get())));

  // The extension cannot access new mount points.
  // TODO(tbarzic): This should probably be changed.
  ASSERT_TRUE(provider.AddLocalMountPoint(FilePath(FPL("/foo/test"))));
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "test_/foo", mount_points.get())));

  provider.RevokeAccessForExtension(extension);
  EXPECT_FALSE(provider.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));

  fileapi::FileSystemURL internal_url = FileSystemURL::CreateForTest(
      GURL("chrome://foo"),
      fileapi::kFileSystemTypeExternal,
      FilePath(FPL("removable/")));
  // Internal WebUI should have full access.
  EXPECT_TRUE(provider.IsAccessAllowed(internal_url));
}

TEST(CrosMountPointProvider, GetVirtualPathConflictWithSystemPoints) {
  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(storage_policy,
      mount_points.get(),
      system_mount_points.get());

  const fileapi::FileSystemType type = fileapi::kFileSystemTypeNativeLocal;

  // Provider specific mount points.
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("b", type, FilePath(FPL("/a/b"))));
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("y", type, FilePath(FPL("/z/y"))));
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("n", type, FilePath(FPL("/m/n"))));

  // System mount points
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "gb", type, FilePath(FPL("/a/b"))));
  ASSERT_TRUE(
      system_mount_points->RegisterFileSystem("gz", type, FilePath(FPL("/z"))));
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
       "gp", type, FilePath(FPL("/m/n/o/p"))));

  struct TestCase {
    const FilePath::CharType* const local_path;
    bool success;
    const FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Same paths in both mount points.
    { FPL("/a/b/c/d"), true, FPL("b/c/d") },
    // System mount points path more specific.
    { FPL("/m/n/o/p/r/s"), true, FPL("n/o/p/r/s") },
    // System mount points path less specific.
    { FPL("/z/y/x"), true, FPL("y/x") },
    // Only system mount points path matches.
    { FPL("/z/q/r/s"), true, FPL("gz/q/r/s") },
    // No match.
    { FPL("/foo/xxx"), false, FPL("") },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Initialize virtual path with a value.
    FilePath virtual_path(FPL("/mount"));
    FilePath local_path(kTestCases[i].local_path);
    EXPECT_EQ(kTestCases[i].success,
              provider.GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << kTestCases[i].local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!kTestCases[i].success)
      continue;

    FilePath expected_virtual_path(kTestCases[i].virtual_path);
    EXPECT_EQ(expected_virtual_path, virtual_path)
        << "Resolving " << kTestCases[i].local_path;
  }
}

}  // namespace

