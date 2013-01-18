// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_

#include <vector>

#include "base/file_path.h"
#include "base/time.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace chrome {

typedef std::vector<MtpFileEntry> MtpFileEntries;

// Used to enumerate top-level files of an media file system.
class MTPDeviceObjectEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit MTPDeviceObjectEnumerator(const MtpFileEntries& entries);

  virtual ~MTPDeviceObjectEnumerator();

  // AbstractFileEnumerator:
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;

  // If the current file entry is valid, returns true and fills in |entry_id|
  // with the entry identifier else returns false and |entry_id| is not set.
  bool GetEntryId(uint32_t* entry_id) const;

  // Returns true if the enumerator has more entries to traverse, false
  // otherwise.
  bool HasMoreEntries() const;

 private:
  // List of directory file entries information.
  const MtpFileEntries file_entries_;

  // Iterator to access the individual file entries.
  MtpFileEntries::const_iterator file_entry_iter_;

  // Stores the current file information.
  MtpFileEntry current_file_info_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceObjectEnumerator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OBJECT_ENUMERATOR_H_
