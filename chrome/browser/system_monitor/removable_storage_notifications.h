// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_

#include "base/file_path.h"
#include "base/observer_list_threadsafe.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"

class MediaGalleriesPrivateApiTest;

namespace chrome {

class RemovableStorageObserver;

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
class RemovableStorageNotifications {
 public:
  struct StorageInfo {
    StorageInfo();
    StorageInfo(const std::string& id,
                const string16& device_name,
                const FilePath::StringType& device_location);

    // Unique device id - persists between device attachments.
    std::string device_id;

    // Human readable removable storage device name.
    string16 name;

    // Current attached removable storage device location.
    FilePath::StringType location;
  };

  virtual ~RemovableStorageNotifications();

  // Returns a pointer to an object owned by the BrowserMainParts, with lifetime
  // somewhat shorter than a process Singleton.
  static RemovableStorageNotifications* GetInstance();

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  virtual bool GetDeviceInfoForPath(
      const FilePath& path,
      StorageInfo* device_info) const = 0;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  virtual uint64 GetStorageSize(const std::string& location) const = 0;

#if defined(OS_WIN)
  // Gets the MTP device storage information specified by |storage_device_id|.
  // On success, returns true and fills in |device_location| with device
  // interface details and |storage_object_id| with the string ID that
  // uniquely identifies the object on the device. This ID need not be
  // persistent across sessions.
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const = 0;
#endif

  // Returns information for attached removable storage.
  std::vector<StorageInfo> GetAttachedStorage() const;

  void AddObserver(RemovableStorageObserver* obs);
  void RemoveObserver(RemovableStorageObserver* obs);

 protected:
  RemovableStorageNotifications();

  friend class MediaFileSystemRegistryTest;
  friend class ::MediaGalleriesPrivateApiTest;
  friend class MediaStorageUtilTest;
  // TODO(gbillock): remove these friends by making the classes owned by the
  // platform-specific implementation.
  friend class MediaTransferProtocolDeviceObserverLinux;
  friend class PortableDeviceWatcherWin;
  friend class VolumeMountWatcherWin;

  void ProcessAttach(const std::string& id,
                     const string16& name,
                     const FilePath::StringType& location);
  void ProcessDetach(const std::string& id);

 private:
  typedef std::map<std::string, StorageInfo> RemovableStorageMap;

  scoped_refptr<ObserverListThreadSafe<RemovableStorageObserver> >
      observer_list_;

  // For manipulating removable_storage_map_ structure.
  mutable base::Lock storage_lock_;

  // Map of all the attached removable storage devices.
  RemovableStorageMap storage_map_;
};

} // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
