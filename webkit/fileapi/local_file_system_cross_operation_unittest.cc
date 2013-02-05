// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/test_file_set.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

namespace {

const int64 kDontCheckSize = -1;
typedef FileSystemOperation::FileEntryList FileEntryList;

void AssignAndQuit(base::RunLoop* run_loop,
                   base::PlatformFileError* result_out,
                   base::PlatformFileError result) {
  *result_out = result;
  run_loop->Quit();
}

base::Callback<void(base::PlatformFileError)>
AssignAndQuitCallback(base::RunLoop* run_loop,
                      base::PlatformFileError* result) {
  return base::Bind(&AssignAndQuit, run_loop, base::Unretained(result));
}

void GetMetadataCallback(base::RunLoop* run_loop,
                         base::PlatformFileError* result_out,
                         base::PlatformFileInfo* file_info_out,
                         base::PlatformFileError result,
                         const base::PlatformFileInfo& file_info,
                         const base::FilePath& /* platform_path */) {
  *result_out = result;
  *file_info_out = file_info;
  run_loop->Quit();
}

void ReadDirectoryCallback(base::RunLoop* run_loop,
                           base::PlatformFileError* result_out,
                           FileEntryList* entries_out,
                           base::PlatformFileError result,
                           const FileEntryList& entries,
                           bool has_more) {
  *result_out = result;
  *entries_out = entries;
  if (result != base::PLATFORM_FILE_OK || !has_more)
    run_loop->Quit();
}

void DidGetUsageAndQuota(quota::QuotaStatusCode* status_out,
                         int64* usage_out,
                         int64* quota_out,
                         quota::QuotaStatusCode status,
                         int64 usage,
                         int64 quota) {
  if (status_out)
    *status_out = status;
  if (usage_out)
    *usage_out = usage;
  if (quota_out)
    *quota_out = quota;
}

class CrossOperationTestHelper {
 public:
  CrossOperationTestHelper(
      const GURL& origin,
      FileSystemType src_type,
      FileSystemType dest_type)
      : origin_(origin),
        src_type_(src_type),
        dest_type_(dest_type) {}

  ~CrossOperationTestHelper() {
    file_system_context_ = NULL;
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    quota_manager_ = NULL;
    quota_manager_proxy_ = NULL;
    MessageLoop::current()->RunUntilIdle();
  }

  void SetUp() {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    FilePath base_dir = base_.path();
    quota_manager_ = new quota::MockQuotaManager(
        false /* is_incognito */, base_dir,
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        NULL /* special storage policy */);
    quota_manager_proxy_ = new quota::MockQuotaManagerProxy(
        quota_manager_,
        base::MessageLoopProxy::current());
    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        ExternalMountPoints::CreateRefCounted().get(),
        make_scoped_refptr(new quota::MockSpecialStoragePolicy),
        quota_manager_proxy_,
        base_dir,
        CreateAllowFileAccessOptions());

    // Prepare the origin's root directory.
    FileSystemMountPointProvider* mount_point_provider =
        file_system_context_->GetMountPointProvider(src_type_);
    mount_point_provider->GetFileSystemRootPathOnFileThread(
        SourceURL(""),
        true /* create */);
    mount_point_provider =
        file_system_context_->GetMountPointProvider(dest_type_);
    mount_point_provider->GetFileSystemRootPathOnFileThread(
        DestURL(""),
        true /* create */);

    // Grant relatively big quota initially.
    quota_manager_->SetQuota(origin_,
                             FileSystemTypeToQuotaStorageType(src_type_),
                             1024 * 1024);
    quota_manager_->SetQuota(origin_,
                             FileSystemTypeToQuotaStorageType(dest_type_),
                             1024 * 1024);
  }

  int64 GetSourceUsage() {
    int64 usage = 0;
    GetUsageAndQuota(src_type_, &usage, NULL);
    return usage;
  }

  int64 GetDestUsage() {
    int64 usage = 0;
    GetUsageAndQuota(dest_type_, &usage, NULL);
    return usage;
  }

  FileSystemURL SourceURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, src_type_, FilePath::FromUTF8Unsafe(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, dest_type_, FilePath::FromUTF8Unsafe(path));
  }

  base::PlatformFileError Copy(const FileSystemURL& src,
                               const FileSystemURL& dest) {
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(dest, NULL);
    EXPECT_TRUE(operation != NULL);
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    base::RunLoop run_loop;
    operation->Copy(src, dest, AssignAndQuitCallback(&run_loop, &result));
    run_loop.Run();
    return result;
  }

  base::PlatformFileError Move(const FileSystemURL& src,
                               const FileSystemURL& dest) {
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(dest, NULL);
    EXPECT_TRUE(operation != NULL);
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    base::RunLoop run_loop;
    operation->Move(src, dest, AssignAndQuitCallback(&run_loop, &result));
    run_loop.Run();
    return result;
  }

  base::PlatformFileError SetUpTestCaseFiles(
      const FileSystemURL& root,
      const test::TestCaseRecord* const test_cases,
      size_t test_case_size) {
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    for (size_t i = 0; i < test_case_size; ++i) {
      const test::TestCaseRecord& test_case = test_cases[i];
      FileSystemURL url = root.WithPath(root.path().Append(test_case.path));
      if (test_case.is_directory)
        result = CreateDirectory(url);
      else
        result = CreateFile(url, test_case.data_file_size);
      EXPECT_EQ(base::PLATFORM_FILE_OK, result) << url.DebugString();
      if (result != base::PLATFORM_FILE_OK)
        return result;
    }
    return result;
  }

  void VerifyTestCaseFiles(
      const FileSystemURL& root,
      const test::TestCaseRecord* const test_cases,
      size_t test_case_size) {
    std::map<FilePath, const test::TestCaseRecord*> test_case_map;
    for (size_t i = 0; i < test_case_size; ++i)
      test_case_map[FilePath(test_cases[i].path).NormalizePathSeparators()] =
          &test_cases[i];

    std::queue<FileSystemURL> directories;
    FileEntryList entries;
    directories.push(root);
    while (!directories.empty()) {
      FileSystemURL dir = directories.front();
      directories.pop();
      ASSERT_EQ(base::PLATFORM_FILE_OK, ReadDirectory(dir, &entries));
      for (size_t i = 0; i < entries.size(); ++i) {
        FileSystemURL url = dir.WithPath(dir.path().Append(entries[i].name));
        FilePath relative;
        root.path().AppendRelativePath(url.path(), &relative);
        relative = relative.NormalizePathSeparators();
        ASSERT_TRUE(ContainsKey(test_case_map, relative));
        if (entries[i].is_directory) {
          EXPECT_TRUE(test_case_map[relative]->is_directory);
          directories.push(url);
        } else {
          EXPECT_FALSE(test_case_map[relative]->is_directory);
          EXPECT_TRUE(FileExists(url, test_case_map[relative]->data_file_size));
        }
        test_case_map.erase(relative);
      }
    }
    EXPECT_TRUE(test_case_map.empty());
  }

  base::PlatformFileError ReadDirectory(const FileSystemURL& url,
                                        FileEntryList* entries) {
    entries->clear();
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(url, NULL);
    EXPECT_TRUE(operation != NULL);
    base::RunLoop run_loop;
    operation->ReadDirectory(
        url, base::Bind(&ReadDirectoryCallback, &run_loop, &result, entries));
    run_loop.Run();
    return result;
  }

  base::PlatformFileError CreateDirectory(const FileSystemURL& url) {
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(url, NULL);
    EXPECT_TRUE(operation != NULL);
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    base::RunLoop run_loop;
    operation->CreateDirectory(url,
                               false /* exclusive */,
                               false /* recursive */,
                               AssignAndQuitCallback(&run_loop, &result));
    run_loop.Run();
    return result;
  }

  base::PlatformFileError CreateFile(const FileSystemURL& url, size_t size) {
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    {
      FileSystemOperation* operation =
          file_system_context_->CreateFileSystemOperation(url, NULL);
      EXPECT_TRUE(operation != NULL);
      base::RunLoop run_loop;
      operation->CreateFile(url, false /* exclusive */,
                            AssignAndQuitCallback(&run_loop, &result));
      run_loop.Run();
    }
    if (result != base::PLATFORM_FILE_OK)
      return result;

    {
      FileSystemOperation* operation =
          file_system_context_->CreateFileSystemOperation(url, NULL);
      EXPECT_TRUE(operation != NULL);
      base::RunLoop run_loop;
      operation->Truncate(url, size,
                          AssignAndQuitCallback(&run_loop, &result));
      run_loop.Run();
    }
    return result;
  }

  bool FileExists(const FileSystemURL& url, int64 expected_size) {
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    base::PlatformFileInfo file_info;
    base::RunLoop run_loop;
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(url, NULL);
    EXPECT_TRUE(operation != NULL);
    operation->GetMetadata(url, base::Bind(&GetMetadataCallback,
                                           &run_loop, &result, &file_info));
    run_loop.Run();
    if (result != base::PLATFORM_FILE_OK || file_info.is_directory)
      return false;
    return expected_size == kDontCheckSize || file_info.size == expected_size;
  }

  bool DirectoryExists(const FileSystemURL& url) {
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    base::PlatformFileInfo file_info;
    base::RunLoop run_loop;
    FileSystemOperation* operation =
        file_system_context_->CreateFileSystemOperation(url, NULL);
    EXPECT_TRUE(operation != NULL);
    operation->GetMetadata(url, base::Bind(&GetMetadataCallback,
                                           &run_loop, &result, &file_info));
    run_loop.Run();
    return (result == base::PLATFORM_FILE_OK) && file_info.is_directory;
  }

  GURL origin() const { return origin_; }
  FileSystemType src_type() const { return src_type_; }
  FileSystemType dest_type() const { return dest_type_; }

 private:
  void GetUsageAndQuota(FileSystemType type, int64* usage, int64* quota) {
    quota::QuotaStatusCode status = quota::kQuotaStatusUnknown;
    quota_manager_->GetUsageAndQuota(
        origin_,
        FileSystemTypeToQuotaStorageType(type),
        base::Bind(&DidGetUsageAndQuota, &status, usage, quota));
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

  base::ScopedTempDir base_;

  const GURL origin_;
  const FileSystemType src_type_;
  const FileSystemType dest_type_;

  MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<quota::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(CrossOperationTestHelper);
};

}  // namespace

TEST(LocalFileSystemCrossOperationTest, CopySingleFile) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateFile(src, 10));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.FileExists(src, 10));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveSingleFile) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateFile(src, 10));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.FileExists(src, kDontCheckSize));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, CopySingleDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveSingleDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, CopyDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            helper.SetUpTestCaseFiles(src,
                                      test::kRegularTestCases,
                                      test::kRegularTestCaseSize));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest,
                             test::kRegularTestCases,
                             test::kRegularTestCaseSize);

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            helper.SetUpTestCaseFiles(src,
                                      test::kRegularTestCases,
                                      test::kRegularTestCaseSize));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest,
                             test::kRegularTestCases,
                             test::kRegularTestCaseSize);

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

}  // namespace fileapi
