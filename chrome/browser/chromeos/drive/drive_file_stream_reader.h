// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "net/base/completion_callback.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace drive {
namespace util {
class FileReader;
}  // namespace util

namespace internal {

// An interface to dispatch the reading operation. If the file is locally
// cached, LocalReaderProxy defined below will be used. Otherwise (i.e. the
// file is being downloaded from the server), NetworkReaderProxy will be used.
class ReaderProxy {
 public:
  virtual ~ReaderProxy() {}

  // Called from DriveFileStreamReader::Read method.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) = 0;

  // Called when the data from the server is received.
  virtual void OnGetContent(scoped_ptr<std::string> data) = 0;

  // Called when the accessing to the file system is completed.
  virtual void OnCompleted(FileError error) = 0;
};

// The read operation implementation for the locally cached files.
class LocalReaderProxy : public ReaderProxy {
 public:
  // The |file_reader| should be the instance which is already opened.
  // This class takes its ownership.
  // |length| is the number of bytes to be read. It must be equal or
  // smaller than the remaining data size in the |file_reader|.
  LocalReaderProxy(scoped_ptr<util::FileReader> file_reader, int64 length);
  virtual ~LocalReaderProxy();

  // ReaderProxy overrides.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnGetContent(scoped_ptr<std::string> data) OVERRIDE;
  virtual void OnCompleted(FileError error) OVERRIDE;

 private:
  scoped_ptr<util::FileReader> file_reader_;

  // Callback for the FileReader::Read.
  void OnReadCompleted(
      const net::CompletionCallback& callback, int read_result);

  // The number of remaining bytes to be read.
  int64 remaining_length_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<LocalReaderProxy> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(LocalReaderProxy);
};

// The read operation implementation for the file which is being downloaded.
class NetworkReaderProxy : public ReaderProxy {
 public:
  // If the instance is deleted during the download process, it is necessary
  // to cancel the job. |job_canceller| should be the callback to run the
  // cancelling.
  NetworkReaderProxy(
      int64 offset, int64 content_length, const base::Closure& job_canceller);
  virtual ~NetworkReaderProxy();

  // ReaderProxy overrides.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnGetContent(scoped_ptr<std::string> data) OVERRIDE;
  virtual void OnCompleted(FileError error) OVERRIDE;

 private:
  // The data received from the server, but not yet read.
  ScopedVector<std::string> pending_data_;

  // The number of bytes to be skipped.
  int64 remaining_offset_;

  // The number of bytes of remaining data (including the data not yet
  // received from the server).
  int64 remaining_content_length_;

  int error_code_;

  // To support pending Read(), it is necessary to keep its arguments.
  scoped_refptr<net::IOBuffer> buffer_;
  int buffer_length_;
  net::CompletionCallback callback_;

  // Keeps the closure to cancel downloading job if necessary.
  // Will be reset when the job is completed (regardless whether the job is
  // successfully done or not).
  base::Closure job_canceller_;

  DISALLOW_COPY_AND_ASSIGN(NetworkReaderProxy);
};

}  // namespace internal

class FileSystemInterface;
class ResourceEntry;

// The stream reader for a file in FileSystem. Instances of this class
// should live on IO thread.
// Operations to communicate with a locally cached file will run on
// |file_task_runner| specified by the constructor.
class DriveFileStreamReader {
 public:
  // Callback to return the FileSystemInterface instance. This is an
  // injecting point for testing.
  // Note that the callback will be copied between threads (IO and UI), and
  // will be called on UI thread.
  typedef base::Callback<FileSystemInterface*()> FileSystemGetter;

  // Callback to return the result of Initialize().
  // |error| is net::Error code.
  typedef base::Callback<void(int error, scoped_ptr<ResourceEntry> entry)>
      InitializeCompletionCallback;

  DriveFileStreamReader(const FileSystemGetter& file_system_getter,
                        base::SequencedTaskRunner* file_task_runner);
  ~DriveFileStreamReader();

  // Returns true if the reader is initialized.
  bool IsInitialized() const;

  // Initializes the stream for the |drive_file_path|.
  // |callback| must not be null.
  void Initialize(const base::FilePath& drive_file_path,
                  uint64 range_offset,
                  uint64 range_length,
                  const InitializeCompletionCallback& callback);

  // Reads the data into |buffer| at most |buffer_length|, and returns
  // the number of bytes. If an error happened, returns an error code.
  // If no data is available yet, returns net::ERR_IO_PENDING immediately,
  // and when the data is available the actual Read operation is done
  // and |callback| will be run with the result.
  // The Read() method must not be called before the Initialize() is completed
  // successfully, or if there is pending read operation.
  // Neither |buffer| nor |callback| must be null.
  int Read(net::IOBuffer* buffer, int buffer_length,
           const net::CompletionCallback& callback);

 private:
  // The range of the data to be read.
  struct Range;

  // Part of Initialize. Called after GetFileContentByPath's initialization
  // is done.
  void InitializeAfterGetFileContentByPathInitialized(
      const Range& range,
      const InitializeCompletionCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry,
      const base::FilePath& local_cache_file_path,
      const base::Closure& cancel_download_closure);

  // Part of Initialize. Called when the local file open process is done.
  void InitializeAfterLocalFileOpen(
      uint64 length,
      const InitializeCompletionCallback& callback,
      scoped_ptr<ResourceEntry> entry,
      scoped_ptr<util::FileReader> file_reader,
      int open_result);

  // Called when the data is received from the server.
  void OnGetContent(google_apis::GDataErrorCode error_code,
                    scoped_ptr<std::string> data);

  // Called when GetFileContentByPath is completed.
  void OnGetFileContentByPathCompletion(
      const InitializeCompletionCallback& callback,
      FileError error);

  const FileSystemGetter file_system_getter_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_ptr<internal::ReaderProxy> reader_proxy_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveFileStreamReader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveFileStreamReader);
};

// TODO(hidehiko): Add thin wrapper class inheriting
// webkit_blob::FileStreamReader for the DriveFileStreamReader.

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
