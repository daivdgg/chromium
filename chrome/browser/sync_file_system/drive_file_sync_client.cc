// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_client.h"

#include <sstream>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"

namespace sync_file_system {

namespace {

const char kRootResourceId[] = "";
const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char kMimeTypeOctetStream[] = "application/octet-stream";

// This path is not actually used but is required by DriveUploaderInterface.
const FilePath::CharType kDummyDrivePath[] =
    FILE_PATH_LITERAL("/dummy/drive/path");

bool HasParentLinkTo(const ScopedVector<google_apis::Link>& links,
                     const GURL& parent_link) {
  bool should_not_have_parent = parent_link.is_empty();

  for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
       itr != links.end(); ++itr) {
    if ((*itr)->type() == google_apis::Link::LINK_PARENT) {
      if (should_not_have_parent)
        return false;
      if ((*itr)->href().GetOrigin() == parent_link.GetOrigin() &&
          (*itr)->href().path() == parent_link.path())
        return true;
    }
  }

  return should_not_have_parent;
}

struct TitleAndParentQuery
    : std::unary_function<const google_apis::ResourceEntry*, bool> {
  TitleAndParentQuery(const string16& title,
                      const GURL& parent_link)
      : title(title),
        parent_link(parent_link) {
  }

  bool operator()(const google_apis::ResourceEntry* entry) const {
    return entry->title() == title &&
        HasParentLinkTo(entry->links(), parent_link);
  }

  const string16& title;
  const GURL& parent_link;
};

void FilterEntriesByTitleAndParent(
    ScopedVector<google_apis::ResourceEntry>* entries,
    const string16& title,
    const GURL& parent_link) {
  typedef ScopedVector<google_apis::ResourceEntry>::iterator iterator;
  iterator itr = std::partition(entries->begin(), entries->end(),
                                TitleAndParentQuery(title, parent_link));
  entries->erase(itr, entries->end());
}

google_apis::ResourceEntry* GetDocumentByTitleAndParent(
    const ScopedVector<google_apis::ResourceEntry>& entries,
    const string16& title,
    const GURL& parent_link) {
  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  iterator found = std::find_if(entries.begin(), entries.end(),
                                TitleAndParentQuery(title, parent_link));
  if (found != entries.end())
    return *found;
  return NULL;
}

void ResourceIdAdapter(const std::string& resource_id,
                       const DriveFileSyncClient::ResourceIdCallback& callback,
                       google_apis::GDataErrorCode error) {
  callback.Run(error, resource_id);
}

}  // namespace

DriveFileSyncClient::DriveFileSyncClient(Profile* profile)
    : url_generator_(GURL(
          google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction)) {
  drive_service_.reset(new google_apis::GDataWapiService(
      profile->GetRequestContext(),
      GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
      "" /* custom_user_agent */));
  drive_service_->Initialize(profile);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_.reset(new google_apis::DriveUploader(drive_service_.get()));
}

scoped_ptr<DriveFileSyncClient> DriveFileSyncClient::CreateForTesting(
    Profile* profile,
    const GURL& base_url,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader) {
  return make_scoped_ptr(new DriveFileSyncClient(
      profile, base_url, drive_service.Pass(), drive_uploader.Pass()));
}

DriveFileSyncClient::DriveFileSyncClient(
    Profile* profile,
    const GURL& base_url,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader)
    : url_generator_(base_url) {
  drive_service_ = drive_service.Pass();
  drive_service_->Initialize(profile);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_ = drive_uploader.Pass();
}

DriveFileSyncClient::~DriveFileSyncClient() {
  DCHECK(CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  drive_service_->RemoveObserver(this);
  drive_service_->CancelAll();
}

void DriveFileSyncClient::AddObserver(DriveFileSyncClientObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void DriveFileSyncClient::RemoveObserver(
    DriveFileSyncClientObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void DriveFileSyncClient::GetDriveDirectoryForSyncRoot(
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string directory_name(kSyncRootDirectoryName);
  SearchFilesInDirectory(
      kRootResourceId,
      FormatTitleQuery(directory_name),
      base::Bind(&DriveFileSyncClient::DidGetDirectory, AsWeakPtr(),
                 kRootResourceId, directory_name, callback));
}

void DriveFileSyncClient::GetDriveDirectoryForOrigin(
    const std::string& sync_root_resource_id,
    const GURL& origin,
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string directory_name(OriginToDirectoryTitle(origin));
  SearchFilesInDirectory(
      sync_root_resource_id,
      FormatTitleQuery(directory_name),
      base::Bind(&DriveFileSyncClient::DidGetDirectory, AsWeakPtr(),
                 sync_root_resource_id, directory_name, callback));
}

void DriveFileSyncClient::DidGetDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsStringASCII(directory_name));

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }

  GURL parent_link;
  if (!parent_resource_id.empty())
    parent_link = ResourceIdToResourceLink(parent_resource_id);
  string16 title(ASCIIToUTF16(directory_name));
  google_apis::ResourceEntry* entry = GetDocumentByTitleAndParent(
      feed->entries(), title, parent_link);
  if (!entry) {
    // If the |parent_resource_id| is empty, create a directory under the root
    // directory. So here we use the result of GetRootResourceId() for such a
    // case.
    std::string resource_id = parent_resource_id.empty() ?
        drive_service_->GetRootResourceId() : parent_resource_id;
    drive_service_->AddNewDirectory(
        resource_id, directory_name,
        base::Bind(&DriveFileSyncClient::DidCreateDirectory, AsWeakPtr(),
                   parent_resource_id, title, callback));
    return;
  }

  // TODO(tzik): Handle error.
  DCHECK_EQ(google_apis::ENTRY_KIND_FOLDER, entry->kind());
  DCHECK_EQ(ASCIIToUTF16(directory_name), entry->title());

  callback.Run(error, entry->resource_id());
}

void DriveFileSyncClient::DidCreateDirectory(
    const std::string& parent_resource_id,
    const string16& title,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    callback.Run(error, std::string());
    return;
  }
  error = google_apis::HTTP_CREATED;

  DCHECK(entry);
  // Check if any other client creates a directory with same title.
  EnsureTitleUniqueness(
      parent_resource_id, title,
      base::Bind(&DriveFileSyncClient::DidEnsureUniquenessForCreateDirectory,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidEnsureUniquenessForCreateDirectory(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  // If error == HTTP_FOUND: the directory is successfully created without
  //   conflict.
  // If error == HTTP_SUCCESS: the directory is created with conflict, but
  //   the conflict was resolved.
  //
  if (error == google_apis::HTTP_FOUND)
    error = google_apis::HTTP_CREATED;
  callback.Run(error, resource_id);
}

void DriveFileSyncClient::GetLargestChangeStamp(
    const ChangeStampCallback& callback) {
  DCHECK(CalledOnValidThread());

  drive_service_->GetAccountMetadata(
      base::Bind(&DriveFileSyncClient::DidGetAccountMetadata,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::GetResourceEntry(
    const std::string& resource_id,
    const ResourceEntryCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidGetAccountMetadata(
    const ChangeStampCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AccountMetadataFeed> metadata) {
  DCHECK(CalledOnValidThread());

  int64 largest_changestamp = 0;
  if (error == google_apis::HTTP_SUCCESS) {
    DCHECK(metadata);
    largest_changestamp = metadata->largest_changestamp();
  }

  callback.Run(error, largest_changestamp);
}

void DriveFileSyncClient::SearchFilesInDirectory(
    const std::string& directory_resource_id,
    const std::string& search_query,
    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceList(
      GURL(),  // feed_url
      0,  // start_changestamp
      search_query,
      false,  // shared_with_me
      directory_resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ListFiles(const std::string& directory_resource_id,
                                    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  SearchFilesInDirectory(directory_resource_id,
                         std::string() /* search_query */,
                         callback);
}

void DriveFileSyncClient::ListChanges(int64 start_changestamp,
                                      const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceList(
      GURL(),  // feed_url
      start_changestamp,
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ContinueListing(
    const GURL& feed_url,
    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceList(
      feed_url,
      0,  // start_changestamp
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DownloadFile(
    const std::string& resource_id,
    const std::string& local_file_md5,
    const FilePath& local_file_path,
    const DownloadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DownloadFileInternal,
                            AsWeakPtr(), local_file_md5, local_file_path,
                            callback)));
}

void DriveFileSyncClient::UploadNewFile(
    const std::string& directory_resource_id,
    const FilePath& local_file_path,
    const std::string& title,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceEntry(
      directory_resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::UploadNewFileInternal,
                            AsWeakPtr(), local_file_path, title, callback)));
}

void DriveFileSyncClient::UploadExistingFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const FilePath& local_file_path,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::UploadExistingFileInternal,
                            AsWeakPtr(), remote_file_md5, local_file_path,
                            callback)));
}

void DriveFileSyncClient::DeleteFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DeleteFileInternal,
                            AsWeakPtr(), remote_file_md5, callback)));
}

// static
std::string DriveFileSyncClient::OriginToDirectoryTitle(const GURL& origin) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));
  return origin.host();
}

// static
GURL DriveFileSyncClient::DirectoryTitleToOrigin(const std::string& title) {
  return extensions::Extension::GetBaseURLFromExtensionId(title);
}

GURL DriveFileSyncClient::ResourceIdToResourceLink(
    const std::string& resource_id) const {
  return url_generator_.GenerateEditUrl(resource_id);
}

void DriveFileSyncClient::OnReadyToPerformOperations() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(DriveFileSyncClientObserver, observers_, OnAuthenticated());
}

void DriveFileSyncClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(CalledOnValidThread());
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    FOR_EACH_OBSERVER(DriveFileSyncClientObserver,
                      observers_, OnNetworkConnected());
}

void DriveFileSyncClient::DidGetResourceList(
    const ResourceListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, scoped_ptr<google_apis::ResourceList>());
    return;
  }

  DCHECK(resource_list);
  callback.Run(error, resource_list.Pass());
}

void DriveFileSyncClient::DidGetResourceEntry(
    const ResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  DCHECK(entry);
  callback.Run(error, entry.Pass());
}

// static
std::string DriveFileSyncClient::FormatTitleQuery(const std::string& title) {
  // TODO(tzik): This pattern matches partial and case-insensitive,
  // and also matches files in subdirectories.
  // Refine the query after we migrate to Drive API.
  std::ostringstream out;
  out << "title:";

  // Escape single quote and back slash with '\\'.
  // https://developers.google.com/drive/search-parameters
  out << '\'';
  for (std::string::const_iterator itr = title.begin();
       itr != title.end(); ++itr) {
    switch (*itr) {
      case '\'':
      case '\\':
        out << '\\' << *itr;
        break;
      default:
        out << *itr;
        break;
    }
  }
  out << '\'';
  return out.str();
}

void DriveFileSyncClient::DownloadFileInternal(
    const std::string& local_file_md5,
    const FilePath& local_file_path,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }
  DCHECK(entry);

  // If local file and remote file are same, cancel the download.
  if (local_file_md5 == entry->file_md5()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, local_file_md5);
    return;
  }

  drive_service_->DownloadFile(
      FilePath(kDummyDrivePath),
      local_file_path,
      entry->content_url(),
      base::Bind(&DriveFileSyncClient::DidDownloadFile,
                 AsWeakPtr(), entry->file_md5(), callback),
      google_apis::GetContentCallback());
}

void DriveFileSyncClient::DidDownloadFile(
    const std::string& downloaded_file_md5,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    const FilePath& downloaded_file_path) {
  DCHECK(CalledOnValidThread());
  callback.Run(error, downloaded_file_md5);
}

void DriveFileSyncClient::UploadNewFileInternal(
    const FilePath& local_file_path,
    const std::string& title,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> parent_directory_entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(parent_directory_entry);

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  // TODO(tzik): This may be creating duplicated files we there's conflicting
  // upload. There's no ETag support for this operation so we need to check
  // if we have duplicated files after the upload. (The API will always try
  // to use newer files so it won't cause inconsistent behavior in the
  // client side but it may leave stale files on the server)
  // http://crbug.com/172820
  drive_uploader_->UploadNewFile(
      parent_directory_entry->GetLinkByType(
          google_apis::Link::LINK_RESUMABLE_CREATE_MEDIA)->href(),
      FilePath(kDummyDrivePath),
      local_file_path,
      title,
      mime_type,
      base::Bind(&DriveFileSyncClient::DidUploadFile, AsWeakPtr(), callback));
}

void DriveFileSyncClient::UploadExistingFileInternal(
    const std::string& remote_file_md5,
    const FilePath& local_file_path,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    callback.Run(google_apis::HTTP_CONFLICT, std::string(), std::string());
    return;
  }

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  drive_uploader_->UploadExistingFile(
      entry->GetLinkByType(
          google_apis::Link::LINK_RESUMABLE_EDIT_MEDIA)->href(),
      FilePath(kDummyDrivePath),
      local_file_path,
      mime_type,
      entry->etag(),
      base::Bind(&DriveFileSyncClient::DidUploadFile,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidUploadFile(
    const UploadFileCallback& callback,
    google_apis::DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  // Convert DriveUploadError to GDataErrorCode.
  switch (error) {
    case google_apis::DRIVE_UPLOAD_OK:
      DCHECK(entry);
      callback.Run(google_apis::HTTP_SUCCESS,
                   entry->resource_id(), entry->file_md5());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_NOT_FOUND:
      callback.Run(google_apis::HTTP_NOT_FOUND,
                   std::string(), std::string());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_NO_SPACE:
      callback.Run(google_apis::GDATA_NO_SPACE,
                   std::string(), std::string());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_CONFLICT:
      callback.Run(google_apis::HTTP_CONFLICT,
                   std::string(), std::string());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_ABORT:
      callback.Run(google_apis::GDATA_OTHER_ERROR,
                   std::string(), std::string());
      return;
  }
  NOTREACHED();
  callback.Run(google_apis::GDATA_OTHER_ERROR, std::string(), std::string());
}

void DriveFileSyncClient::DeleteFileInternal(
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    callback.Run(google_apis::HTTP_CONFLICT);
    return;
  }

  // Move the file to trash (don't delete it completely).
  // TODO(nhiroki): support ETag. Currently we assume there is no change between
  // GetResourceEntry and DeleteFile call.
  // http://crbug.com/156037
  drive_service_->DeleteResource(
      entry->resource_id(),
      entry->etag(),
      base::Bind(&DriveFileSyncClient::DidDeleteFile,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidDeleteFile(
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  callback.Run(error);
}

void DriveFileSyncClient::EnsureTitleUniqueness(
    const std::string& parent_resource_id,
    const string16& expected_title,
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  SearchFilesInDirectory(
      parent_resource_id,
      FormatTitleQuery(UTF16ToUTF8(expected_title)),
      base::Bind(&DriveFileSyncClient::DidListEntriesToEnsureUniqueness,
                 AsWeakPtr(), parent_resource_id, expected_title, callback));
}

void DriveFileSyncClient::DidListEntriesToEnsureUniqueness(
    const std::string& parent_resource_id,
    const string16& expected_title,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }

  // This filtering is needed only on WAPI. Once we move to Drive API we can
  // drop this.
  GURL parent_link;
  if (!parent_resource_id.empty())
    parent_link = ResourceIdToResourceLink(parent_resource_id);
  ScopedVector<google_apis::ResourceEntry> entries;
  entries.swap(*feed->mutable_entries());
  FilterEntriesByTitleAndParent(&entries, expected_title, parent_link);

  if (entries.empty()) {
    callback.Run(google_apis::HTTP_NOT_FOUND, std::string());
    return;
  }

  if (entries.size() >= 2) {
    for (size_t i = 0; i < entries.size() - 1; ++i) {
      // TODO(tzik): Replace published_time with creation time after we move to
      // Drive API.
      if (entries[i]->published_time() < entries.back()->published_time())
        std::swap(entries[i], entries.back());
    }

    scoped_ptr<google_apis::ResourceEntry> earliest_entry(entries.back());
    entries.back() = NULL;
    entries.get().pop_back();

    DeleteEntries(entries.Pass(),
                  base::Bind(&ResourceIdAdapter,
                             earliest_entry->resource_id(), callback));
    return;
  }

  DCHECK_EQ(1u, entries.size());
  scoped_ptr<google_apis::ResourceEntry> entry(entries.front());
  entries.weak_clear();
  callback.Run(google_apis::HTTP_FOUND, entry->resource_id());
}

void DriveFileSyncClient::DeleteEntries(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (entries.empty()) {
    callback.Run(google_apis::HTTP_SUCCESS);
    return;
  }

  scoped_ptr<google_apis::ResourceEntry> entry(entries.back());
  entries.back() = NULL;
  entries.get().pop_back();

  drive_service_->DeleteResource(
      entry->resource_id(),
      entry->etag(),
      base::Bind(&DriveFileSyncClient::DidDeleteEntry, AsWeakPtr(),
                 base::Passed(&entries), callback));
}

void DriveFileSyncClient::DidDeleteEntry(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    callback.Run(error);
    return;
  }

  DeleteEntries(entries.Pass(), callback);
}

}  // namespace sync_file_system
