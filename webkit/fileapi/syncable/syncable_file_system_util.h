// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_UTIL_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/file_path.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/fileapi_export.h"

namespace fileapi {

// Registers a syncable filesystem with the given |service_name|.
FILEAPI_EXPORT bool RegisterSyncableFileSystem(const std::string& service_name);

// Revokes the syncable filesystem that was registered with |service_name|.
FILEAPI_EXPORT bool RevokeSyncableFileSystem(const std::string& service_name);

// Returns the root URI of the syncable filesystem that can be specified by a
// pair of |origin| and |service_name|.
FILEAPI_EXPORT GURL GetSyncableFileSystemRootURI(
    const GURL& origin, const std::string& service_name);

// Creates a FileSystem URL for the |path| in a syncable filesystem
// identifiable by a pair of |origin| and |service_name|.
//
// Example: Assume following arguments are given:
//   origin: 'http://www.example.com/',
//   service_name: 'service_name',
//   path: '/foo/bar',
// returns 'filesystem:http://www.example.com/external/service_name/foo/bar'
FILEAPI_EXPORT FileSystemURL CreateSyncableFileSystemURL(
    const GURL& origin, const std::string& service_name, const FilePath& path);

// Serializes a given FileSystemURL |url| and sets the serialized string to
// |serialized_url|. If the URL does not represent a syncable filesystem,
// |serialized_url| is not filled in, and returns false. Separators of the
// path will be normalized depending on its platform.
//
// Example: Assume a following FileSystemURL object is given:
//   origin() returns 'http://www.example.com/',
//   type() returns the kFileSystemTypeSyncable,
//   filesystem_id() returns 'service_name',
//   path() returns '/foo/bar',
// this URL will be serialized to
// (on Windows)
//   'filesystem:http://www.example.com/external/service_name/foo\\bar'
// (on others)
//   'filesystem:http://www.example.com/external/service_name/foo/bar'
FILEAPI_EXPORT bool SerializeSyncableFileSystemURL(
    const FileSystemURL& url, std::string* serialized_url);

// Deserializes a serialized FileSystem URL string |serialized_url| and sets the
// deserialized value to |url|. If the reconstructed object is invalid or does
// not represent a syncable filesystem, returns false.
//
// NOTE: On any platform other than Windows, this function assumes that
// |serialized_url| does not contain '\\'. If it contains '\\' on such
// platforms, '\\' may be replaced with '/' (It would not be an expected
// behavior).
//
// See the comment of SerializeSyncableFileSystemURL() for more details.
FILEAPI_EXPORT bool DeserializeSyncableFileSystemURL(
    const std::string& serialized_url, FileSystemURL* url);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_UTIL_H_
