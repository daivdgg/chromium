// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns the number of children of |node| that are of type url.
int ChildURLCount(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
  }
  return result;
}

bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int child_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    child_count += ChildURLCount(nodes[i]);

  if (child_count < bookmark_utils::num_urls_before_prompting)
    return true;

  return chrome::ShowMessageBox(parent,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                                 base::IntToString16(child_count)),
      chrome::MESSAGE_BOX_TYPE_QUESTION) == chrome::MESSAGE_BOX_RESULT_YES;
}

// Implementation of OpenAll. Opens all nodes of type URL and any children of
// |node| that are of type URL. |navigator| is the PageNavigator used to open
// URLs. After the first url is opened |opened_url| is set to true and
// |navigator| is set to the PageNavigator of the last active tab. This is done
// to handle a window disposition of new window, in which case we want
// subsequent tabs to open in that window.
void OpenAllImpl(const BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 content::PageNavigator** navigator,
                 bool* opened_url) {
  if (node->is_url()) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    content::WebContents* opened_tab = (*navigator)->OpenURL(
        content::OpenURLParams(node->url(), content::Referrer(), disposition,
                               content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      *navigator = opened_tab;
    }
  } else {
    // For folders only open direct children.
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child_node = node->GetChild(i);
      if (child_node->is_url())
        OpenAllImpl(child_node, initial_disposition, navigator, opened_url);
    }
  }
}

// Returns the total number of descendants nodes.
int ChildURLCountTotal(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    result++;
    if (child->is_folder())
      result += ChildURLCountTotal(child);
  }
  return result;
}

}  // namespace

namespace chrome {

void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  bool opened_url = false;
  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, &opened_url);
}

void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  OpenAll(parent, navigator, nodes, initial_disposition);
}

bool ConfirmDeleteBookmarkNode(const BookmarkNode* node,
                               gfx::NativeWindow window) {
  DCHECK(node && node->is_folder() && !node->empty());
  return chrome::ShowMessageBox(window,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16Int(IDS_BOOKMARK_EDITOR_CONFIRM_DELETE,
                                    ChildURLCountTotal(node)),
      chrome::MESSAGE_BOX_TYPE_QUESTION) == chrome::MESSAGE_BOX_RESULT_YES;
}

}  // namespace chrome
