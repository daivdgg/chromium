// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "grit/webkit_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"

using WebKit::WebAutofillClient;

namespace {

// Used to indicate that no line is currently selected by the user.
const int kNoSelection = -1;

// Size difference between value text and label text in pixels.
const int kLabelFontSizeDelta = -2;

// The vertical height of each row in pixels.
const size_t kRowHeight = 24;

// The vertical height of a separator in pixels.
const size_t kSeparatorHeight = 1;

// The amount of minimum padding between the Autofill value and label in pixels.
const size_t kLabelPadding = 15;

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
  { "americanExpressCC", IDR_AUTOFILL_CC_AMEX },
  { "dinersCC", IDR_AUTOFILL_CC_DINERS },
  { "discoverCC", IDR_AUTOFILL_CC_DISCOVER },
  { "genericCC", IDR_AUTOFILL_CC_GENERIC },
  { "jcbCC", IDR_AUTOFILL_CC_JCB },
  { "masterCardCC", IDR_AUTOFILL_CC_MASTERCARD },
  { "soloCC", IDR_AUTOFILL_CC_SOLO },
  { "visaCC", IDR_AUTOFILL_CC_VISA },
};

}  // end namespace

const size_t AutofillPopupView::kBorderThickness = 1;
const size_t AutofillPopupView::kIconPadding = 5;
const size_t AutofillPopupView::kEndPadding = 3;
const size_t AutofillPopupView::kDeleteIconHeight = 16;
const size_t AutofillPopupView::kDeleteIconWidth = 16;
const size_t AutofillPopupView::kAutofillIconHeight = 16;
const size_t AutofillPopupView::kAutofillIconWidth = 25;

AutofillPopupView::AutofillPopupView(
    content::WebContents* web_contents,
    AutofillExternalDelegate* external_delegate)
    : external_delegate_(external_delegate),
      selected_line_(kNoSelection) {
  if (!web_contents)
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 content::Source<content::WebContents>(web_contents));
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents->GetController())));

  label_font_ = value_font_.DeriveFont(kLabelFontSizeDelta);
}

AutofillPopupView::~AutofillPopupView() {}

void AutofillPopupView::Hide() {
  HideInternal();

  external_delegate_->ClearPreviewedForm();
}

void AutofillPopupView::Show(const std::vector<string16>& autofill_values,
                             const std::vector<string16>& autofill_labels,
                             const std::vector<string16>& autofill_icons,
                             const std::vector<int>& autofill_unique_ids) {
  autofill_values_ = autofill_values;
  autofill_labels_ = autofill_labels;
  autofill_icons_ = autofill_icons;
  autofill_unique_ids_ = autofill_unique_ids;

  ShowInternal();
}

void AutofillPopupView::SetSelectedLine(int selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line_ != kNoSelection)
    InvalidateRow(selected_line_);

  if (selected_line != kNoSelection)
    InvalidateRow(selected_line);

  selected_line_ = selected_line;

  if (selected_line_ != kNoSelection) {
    external_delegate_->SelectAutofillSuggestionAtIndex(
        autofill_unique_ids_[selected_line_]);
  }
}

void AutofillPopupView::ClearSelectedLine() {
  SetSelectedLine(kNoSelection);
}

void AutofillPopupView::SelectNextLine() {
  int new_selected_line = selected_line_ + 1;

  if (new_selected_line == static_cast<int>(autofill_values_.size()))
    new_selected_line = 0;

  SetSelectedLine(new_selected_line);
}

void AutofillPopupView::SelectPreviousLine() {
  int new_selected_line = selected_line_ - 1;

  if (new_selected_line <= kNoSelection)
    new_selected_line = autofill_values_.size() - 1;

  SetSelectedLine(new_selected_line);
}

bool AutofillPopupView::AcceptSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  if (!CanAccept(autofill_unique_ids_[selected_line_]))
    return false;

  return external_delegate()->DidAcceptAutofillSuggestions(
      autofill_values_[selected_line_],
      autofill_unique_ids_[selected_line_],
      selected_line_);
}

bool AutofillPopupView::RemoveSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  if (!CanDelete(autofill_unique_ids_[selected_line_]))
    return false;

  if (autofill_unique_ids_[selected_line_] > 0) {
    external_delegate()->RemoveAutofillProfileOrCreditCard(
        autofill_unique_ids_[selected_line_]);
  } else {
    external_delegate()->RemoveAutocompleteEntry(
        autofill_values_[selected_line_]);
  }

  // Remove the deleted element.
  autofill_values_.erase(autofill_values_.begin() + selected_line_);
  autofill_labels_.erase(autofill_labels_.begin() + selected_line_);
  autofill_icons_.erase(autofill_icons_.begin() + selected_line_);
  autofill_unique_ids_.erase(autofill_unique_ids_.begin() + selected_line_);

  // Resize the popup.
  ResizePopup();

  SetSelectedLine(kNoSelection);

  external_delegate_->ClearPreviewedForm();

  if (!HasAutofillEntries())
    Hide();

  return true;
}

int AutofillPopupView::GetIconResourceID(const string16& resource_name) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (resource_name == ASCIIToUTF16(kDataResources[i].name))
      return kDataResources[i].id;
  }

  return -1;
}

bool AutofillPopupView::CanDelete(int id) {
  return id > 0 ||
      id == WebAutofillClient::MenuItemIDAutocompleteEntry ||
      id == WebAutofillClient::MenuItemIDPasswordEntry;
}

int AutofillPopupView::GetPopupRequiredWidth() {
  int popup_width = element_bounds().width();
  DCHECK_EQ(autofill_values().size(), autofill_labels().size());
  for (size_t i = 0; i < autofill_values().size(); ++i) {
    int row_size = kEndPadding +
        value_font_.GetStringWidth(autofill_values()[i]) +
        kLabelPadding +
        label_font_.GetStringWidth(autofill_labels()[i]);

    // Add the Autofill icon size, if required.
    if (!autofill_icons()[i].empty())
      row_size += kAutofillIconWidth + kIconPadding;

    // Add delete icon, if required.
    if (CanDelete(autofill_unique_ids()[i]))
      row_size += kDeleteIconWidth + kIconPadding;

    // Add the padding at the end
    row_size += kEndPadding;

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupView::GetPopupRequiredHeight() {
  int popup_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    popup_height += GetRowHeightFromId(autofill_unique_ids()[i]);
  }

  return popup_height;
}

int AutofillPopupView::LineFromY(int y) {
  int current_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    current_height += GetRowHeightFromId(autofill_unique_ids()[i]);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return autofill_unique_ids().size() - 1;
}

int AutofillPopupView::GetRowHeightFromId(int unique_id) {
  if (unique_id == WebAutofillClient::MenuItemIDSeparator)
    return kSeparatorHeight;

  return kRowHeight;
}

gfx::Rect AutofillPopupView::GetRectForRow(size_t row, int width) {
  int top = 0;
  for (size_t i = 0; i < row; ++i) {
    top += GetRowHeightFromId(autofill_unique_ids()[i]);
  }

  return gfx::Rect(
      0, top, width, GetRowHeightFromId(autofill_unique_ids()[row]));
}

bool AutofillPopupView::DeleteIconIsSelected(int x, int y) {
  if (!CanDelete(selected_line()))
    return false;

  int row_start_y = 0;
  for (int i = 0; i < selected_line(); ++i) {
    row_start_y += GetRowHeightFromId(autofill_unique_ids()[i]);
  }

  gfx::Rect delete_icon_bounds = gfx::Rect(
      GetPopupRequiredWidth() - kDeleteIconWidth - kIconPadding,
      row_start_y + ((kRowHeight - kDeleteIconHeight) / 2),
      kDeleteIconWidth,
      kDeleteIconHeight);

  return delete_icon_bounds.Contains(x, y);
}

bool AutofillPopupView::CanAccept(int id) {
  return id != WebAutofillClient::MenuItemIDSeparator;
}

bool AutofillPopupView::HasAutofillEntries() {
  return autofill_values_.size() != 0 &&
      (autofill_unique_ids_[0] > 0 ||
       autofill_unique_ids_[0] ==
         WebAutofillClient::MenuItemIDAutocompleteEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDPasswordEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDDataListEntry);
}

void AutofillPopupView::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED) {
    if (!*content::Details<bool>(details).ptr())
      Hide();
  } else if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    Hide();
  }
}
