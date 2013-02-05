// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_ENTRY_PICKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_ENTRY_PICKER_H_

#include "chrome/browser/extensions/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

namespace extensions {

namespace api {

class EntryPickerClient {
 public:
  virtual ~EntryPickerClient() { }

  virtual void FileSelected(const FilePath& path) = 0;
  virtual void FileSelectionCanceled() = 0;
};

// Handles showing a dialog to the user to ask for the directory name.
class EntryPicker : public ui::SelectFileDialog::Listener {
 public:
  EntryPicker(EntryPickerClient* client,
              content::WebContents* web_contents,
              ui::SelectFileDialog::Type picker_type,
              const FilePath& last_directory,
              const string16& select_title);

  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(FilePath* path);
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();

 protected:

  virtual ~EntryPicker();

 private:
  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  EntryPickerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(EntryPicker);
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_ENTRY_PICKER_H_
