// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/external_mount_points.h"

#include <string>

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_url.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

using fileapi::FileSystemURL;

namespace {

TEST(ExternalMountPointsTest, AddMountPoint) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  struct TestCase {
    // The mount point's name.
    const char* const name;
    // The mount point's path.
    const FilePath::CharType* const path;
    // Whether the mount point registration should succeed.
    bool success;
    // Path returned by GetRegisteredPath. NULL if the method is expected to
    // fail.
    const FilePath::CharType* const registered_path;
  };

  const TestCase kTestCases[] = {
    // Valid mount point.
    { "test", DRIVE FPL("/foo/test"), true, DRIVE FPL("/foo/test") },
    // Valid mount point with only one path component.
    { "bbb", DRIVE FPL("/bbb"), true, DRIVE FPL("/bbb") },
    // Existing mount point path is substring of the mount points path.
    { "test11", DRIVE FPL("/foo/test11"), true, DRIVE FPL("/foo/test11") },
    // Path substring of an existing path.
    { "test1", DRIVE FPL("/foo/test1"), true, DRIVE FPL("/foo/test1") },
    // Empty mount point name and path.
    { "", DRIVE FPL(""), false, NULL },
    // Empty mount point name.
    { "", DRIVE FPL("/ddd"), false, NULL },
    // Empty mount point path.
    { "empty_path", FPL(""), true, FPL("") },
    // Name different from path's base name.
    { "not_base_name", DRIVE FPL("/x/y/z"), true, DRIVE FPL("/x/y/z") },
    // References parent.
    { "invalid", DRIVE FPL("../foo/invalid"), false, NULL },
    // Relative path.
    { "relative", DRIVE FPL("foo/relative"), false, NULL },
    // Existing mount point path.
    { "path_exists", DRIVE FPL("/foo/test"), false, NULL },
    // Mount point with the same name exists.
    { "test", DRIVE FPL("/foo/a/test_name_exists"), false,
      DRIVE FPL("/foo/test") },
    // Child of an existing mount point.
    { "a1", DRIVE FPL("/foo/test/a"), false, NULL },
    // Parent of an existing mount point.
    { "foo1", DRIVE FPL("/foo"), false, NULL },
    // Bit bigger depth.
    { "g", DRIVE FPL("/foo/a/b/c/d/e/f/g"), true,
      DRIVE FPL("/foo/a/b/c/d/e/f/g") },
    // Sibling mount point (with similar name) exists.
    { "ff", DRIVE FPL("/foo/a/b/c/d/e/ff"), true,
       DRIVE FPL("/foo/a/b/c/d/e/ff") },
    // Lexicographically last among existing mount points.
    { "yyy", DRIVE FPL("/zzz/yyy"), true, DRIVE FPL("/zzz/yyy") },
    // Parent of the lexicographically last mount point.
    { "zzz1", DRIVE FPL("/zzz"), false, NULL },
    // Child of the lexicographically last mount point.
    { "xxx1", DRIVE FPL("/zzz/yyy/xxx"), false, NULL },
    // Lexicographically first among existing mount points.
    { "b", DRIVE FPL("/a/b"), true, DRIVE FPL("/a/b") },
    // Parent of lexicographically first mount point.
    { "a2", DRIVE FPL("/a"), false, NULL },
    // Child of lexicographically last mount point.
    { "c1", DRIVE FPL("/a/b/c"), false, NULL },
    // Parent to all of the mount points.
    { "root", DRIVE FPL("/"), false, NULL },
    // Path contains .. component.
    { "funky", DRIVE FPL("/tt/fun/../funky"), false, NULL },
    // Windows separators.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { "win", DRIVE FPL("\\try\\separators\\win"), true,
      DRIVE FPL("\\try\\separators\\win") },
    { "win1", DRIVE FPL("\\try/separators\\win1"), true,
      DRIVE FPL("\\try/separators\\win1") },
    { "win2", DRIVE FPL("\\try/separators\\win"), false, NULL },
#else
    { "win", DRIVE FPL("\\separators\\win"), false, NULL },
    { "win1", DRIVE FPL("\\try/separators\\win1"), false, NULL },
#endif
    // Win separators, but relative path.
    { "win2", DRIVE FPL("try\\separators\\win2"), false, NULL },
  };

  // Test adding mount points.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].success,
              mount_points->RegisterFileSystem(
                  kTestCases[i].name,
                  fileapi::kFileSystemTypeNativeLocal,
                  FilePath(kTestCases[i].path)))
        << "Adding mount point: " << kTestCases[i].name << " with path "
        << kTestCases[i].path;
  }

  // Test that final mount point presence state is as expected.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    FilePath found_path;
    EXPECT_EQ(kTestCases[i].registered_path != NULL,
              mount_points->GetRegisteredPath(kTestCases[i].name, &found_path))
        << "Test case: " << i;

    if (kTestCases[i].registered_path) {
      FilePath expected_path(kTestCases[i].registered_path);
      EXPECT_EQ(expected_path.NormalizePathSeparators(), found_path);
    }
  }
}

TEST(ExternalMountPointsTest, GetVirtualPath) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c")));
  // Note that "/a/b/c" < "/a/b/c(1)" < "/a/b/c/".
  mount_points->RegisterFileSystem("c(1)",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("x",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/z/y/x")));
  mount_points->RegisterFileSystem("o",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/m/n/o")));
  // A mount point whose name does not match its path base name.
  mount_points->RegisterFileSystem("mount",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/root/foo")));
  // A mount point with an empty path.
  mount_points->RegisterFileSystem("empty_path",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(FPL("")));

  struct TestCase {
    const FilePath::CharType* const local_path;
    bool success;
    const FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Empty path.
    { FPL(""), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/a/b"), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/z/y"), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/m/n"), false, FPL("") },
    // No registered mount point.
    { DRIVE FPL("/foo/mount"), false, FPL("") },
    // An existing mount point path is substring.
    { DRIVE FPL("/a/b/c1"), false, FPL("") },
    // No leading /.
    { DRIVE FPL("a/b/c"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/a/b/d/e"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/z/y/v/u"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/m/n/p/q"), false, FPL("") },
    // Mount point root path.
    { DRIVE FPL("/a/b/c"), true, FPL("c") },
    // Mount point root path.
    { DRIVE FPL("/z/y/x"), true, FPL("x") },
    // Mount point root path.
    { DRIVE FPL("/m/n/o"), true, FPL("o") },
    // Mount point child path.
    { DRIVE FPL("/a/b/c/d/e"), true, FPL("c/d/e") },
    // Mount point child path.
    { DRIVE FPL("/z/y/x/v/u"), true, FPL("x/v/u") },
    // Mount point child path.
    { DRIVE FPL("/m/n/o/p/q"), true, FPL("o/p/q") },
    // Name doesn't match mount point path base name.
    { DRIVE FPL("/root/foo/a/b/c"), true, FPL("mount/a/b/c") },
    { DRIVE FPL("/root/foo"), true, FPL("mount") },
    // Mount point contains character whose ASCII code is smaller than file path
    // separator's.
    { DRIVE FPL("/a/b/c(1)/d/e"), true, FPL("c(1)/d/e") },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Path with win separators mixed in.
    { DRIVE FPL("/a\\b\\c/d"), true, FPL("c/d") },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Initialize virtual path with a value.
    FilePath virtual_path(DRIVE FPL("/mount"));
    FilePath local_path(kTestCases[i].local_path);
    EXPECT_EQ(kTestCases[i].success,
              mount_points->GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << kTestCases[i].local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!kTestCases[i].success)
      continue;

    FilePath expected_virtual_path(kTestCases[i].virtual_path);
    EXPECT_EQ(expected_virtual_path.NormalizePathSeparators(), virtual_path)
        << "Resolving " << kTestCases[i].local_path;
  }
}

TEST(ExternalMountPointsTest, HandlesFileSystemMountType) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  const GURL test_origin("http://chromium.org");
  const FilePath test_path(FPL("/mount"));

  // Should handle External File System.
  EXPECT_TRUE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeExternal));

  // Shouldn't handle the rest.
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeIsolated));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeTemporary));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypePersistent));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeTest));
  // Not even if it's external subtype.
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeNativeLocal));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeRestrictedNativeLocal));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeDrive));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(
      fileapi::kFileSystemTypeSyncable));
}

TEST(ExternalMountPointsTest, CreateCrackedFileSystemURL) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  const GURL kTestOrigin("http://chromium.org");

  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c")));
  mount_points->RegisterFileSystem("c(1)",
                                   fileapi::kFileSystemTypeDrive,
                                   FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("empty_path",
                                   fileapi::kFileSystemTypeSyncable,
                                   FilePath(FPL("")));
  mount_points->RegisterFileSystem("mount",
                                   fileapi::kFileSystemTypeDrive,
                                   FilePath(DRIVE FPL("/root")));

  // Try cracking invalid GURL.
  FileSystemURL invalid = mount_points->CrackURL(GURL("http://chromium.og"));
  EXPECT_FALSE(invalid.is_valid());

  // Try cracking isolated path.
  FileSystemURL isolated = mount_points->CreateCrackedFileSystemURL(
      kTestOrigin, fileapi::kFileSystemTypeIsolated, FilePath(FPL("c")));
  EXPECT_FALSE(isolated.is_valid());

  // Try native local which is not cracked.
  FileSystemURL native_local = mount_points->CreateCrackedFileSystemURL(
      kTestOrigin, fileapi::kFileSystemTypeNativeLocal, FilePath(FPL("c")));
  EXPECT_FALSE(native_local.is_valid());

  struct TestCase {
    const FilePath::CharType* const path;
    bool expect_valid;
    fileapi::FileSystemType expect_type;
    const FilePath::CharType* const expect_path;
    const char* const expect_fs_id;
  };

  const TestCase kTestCases[] = {
    { FPL("c/d/e"),
      true, fileapi::kFileSystemTypeNativeLocal, DRIVE FPL("/a/b/c/d/e"), "c" },
    { FPL("c(1)/d/e"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/a/b/c(1)/d/e"), "c(1)" },
    { FPL("c(1)"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/a/b/c(1)"), "c(1)" },
    { FPL("empty_path/a"),
      true, fileapi::kFileSystemTypeSyncable, FPL("a"), "empty_path" },
    { FPL("empty_path"),
      true, fileapi::kFileSystemTypeSyncable, FPL(""), "empty_path" },
    { FPL("mount/a/b"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root/a/b"), "mount" },
    { FPL("mount"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root"), "mount" },
    { FPL("cc"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL(""),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL(".."),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    // Absolte paths.
    { FPL("/c/d/e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/c(1)/d/e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/empty_path"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    // PAth references parent.
    { FPL("c/d/../e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/empty_path/a/../b"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("c/d\\e"),
      true, fileapi::kFileSystemTypeNativeLocal, DRIVE FPL("/a/b/c/d/e"), "c" },
    { FPL("mount\\a\\b"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root/a/b"), "mount" },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    FileSystemURL cracked = mount_points->CreateCrackedFileSystemURL(
        kTestOrigin,
        fileapi::kFileSystemTypeExternal,
        FilePath(kTestCases[i].path));

    EXPECT_EQ(kTestCases[i].expect_valid, cracked.is_valid())
        << "Test case index: " << i;

    if (!kTestCases[i].expect_valid)
      continue;

    EXPECT_EQ(kTestOrigin, cracked.origin())
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_type, cracked.type())
        << "Test case index: " << i;
    EXPECT_EQ(FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
                       cracked.path())
        << "Test case index: " << i;
    EXPECT_EQ(FilePath(kTestCases[i].path).NormalizePathSeparators(),
                       cracked.virtual_path())
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_fs_id, cracked.filesystem_id())
        << "Test case index: " << i;
    EXPECT_EQ(fileapi::kFileSystemTypeExternal, cracked.mount_type())
        << "Test case index: " << i;
  }
}

TEST(ExternalMountPointsTest, CrackVirtualPath) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  const GURL kTestOrigin("http://chromium.org");

  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c")));
  mount_points->RegisterFileSystem("c(1)",
                                   fileapi::kFileSystemTypeDrive,
                                   FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("empty_path",
                                   fileapi::kFileSystemTypeSyncable,
                                   FilePath(FPL("")));
  mount_points->RegisterFileSystem("mount",
                                   fileapi::kFileSystemTypeDrive,
                                   FilePath(DRIVE FPL("/root")));

  struct TestCase {
    const FilePath::CharType* const path;
    bool expect_valid;
    fileapi::FileSystemType expect_type;
    const FilePath::CharType* const expect_path;
    const char* const expect_name;
  };

  const TestCase kTestCases[] = {
    { FPL("c/d/e"),
      true, fileapi::kFileSystemTypeNativeLocal, DRIVE FPL("/a/b/c/d/e"), "c" },
    { FPL("c(1)/d/e"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/a/b/c(1)/d/e"), "c(1)" },
    { FPL("c(1)"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/a/b/c(1)"), "c(1)" },
    { FPL("empty_path/a"),
      true, fileapi::kFileSystemTypeSyncable, FPL("a"), "empty_path" },
    { FPL("empty_path"),
      true, fileapi::kFileSystemTypeSyncable, FPL(""), "empty_path" },
    { FPL("mount/a/b"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root/a/b"), "mount" },
    { FPL("mount"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root"), "mount" },
    { FPL("cc"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL(""),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL(".."),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    // Absolte paths.
    { FPL("/c/d/e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/c(1)/d/e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/empty_path"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    // PAth references parent.
    { FPL("c/d/../e"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
    { FPL("/empty_path/a/../b"),
      false, fileapi::kFileSystemTypeUnknown, FPL(""), "" },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("c/d\\e"),
      true, fileapi::kFileSystemTypeNativeLocal, DRIVE FPL("/a/b/c/d/e"), "c" },
    { FPL("mount\\a\\b"),
      true, fileapi::kFileSystemTypeDrive, DRIVE FPL("/root/a/b"), "mount" },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    std::string cracked_name;
    fileapi::FileSystemType cracked_type;
    FilePath cracked_path;
    EXPECT_EQ(kTestCases[i].expect_valid,
              mount_points->CrackVirtualPath(FilePath(kTestCases[i].path),
                  &cracked_name, &cracked_type, &cracked_path))
        << "Test case index: " << i;

    if (!kTestCases[i].expect_valid)
      continue;

    EXPECT_EQ(kTestCases[i].expect_type, cracked_type)
        << "Test case index: " << i;
    EXPECT_EQ(FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
                       cracked_path)
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_name, cracked_name)
        << "Test case index: " << i;
  }
}

}  // namespace

