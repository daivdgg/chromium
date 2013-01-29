// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/app_launcher.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kDefaultPageTypeHistogram[] = "NewTabPage.DefaultPageType";

enum PromoAction {
  PROMO_VIEWED = 0,
  PROMO_CLOSED,
  PROMO_LINK_CLICKED,
  PROMO_ACTION_MAX,
};

}  // namespace

NewTabPageHandler::NewTabPageHandler() : page_switch_count_(0) {
}

NewTabPageHandler::~NewTabPageHandler() {
  HISTOGRAM_COUNTS_100("NewTabPage.SingleSessionPageSwitches",
                       page_switch_count_);
}

void NewTabPageHandler::RegisterMessages() {
  // Record an open of the NTP with its default page type.
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  int shown_page_type = prefs->GetInteger(prefs::kNtpShownPage) >>
      kPageIdOffset;
  UMA_HISTOGRAM_ENUMERATION(kDefaultPageTypeHistogram,
                            shown_page_type, kHistogramEnumerationMax);

  web_ui()->RegisterMessageCallback("notificationPromoClosed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoClosed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notificationPromoViewed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoViewed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notificationPromoLinkClicked",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoLinkClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoClosed",
      base::Bind(&NewTabPageHandler::HandleBubblePromoClosed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoViewed",
      base::Bind(&NewTabPageHandler::HandleBubblePromoViewed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoLinkClicked",
      base::Bind(&NewTabPageHandler::HandleBubblePromoLinkClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("pageSelected",
      base::Bind(&NewTabPageHandler::HandlePageSelected,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("logTimeToClick",
      base::Bind(&NewTabPageHandler::HandleLogTimeToClick,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getShouldShowApps",
      base::Bind(&NewTabPageHandler::HandleGetShouldShowApps,
                 base::Unretained(this)));
}

void NewTabPageHandler::HandleNotificationPromoClosed(const ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Notification",
                            PROMO_CLOSED, PROMO_ACTION_MAX);
  NotificationPromo::HandleClosed(NotificationPromo::NTP_NOTIFICATION_PROMO);
  Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleNotificationPromoViewed(const ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Notification",
                            PROMO_VIEWED, PROMO_ACTION_MAX);
  if (NotificationPromo::HandleViewed(
          NotificationPromo::NTP_NOTIFICATION_PROMO)) {
    Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
  }
}

void NewTabPageHandler::HandleNotificationPromoLinkClicked(
    const ListValue* args) {
  DVLOG(1) << "HandleNotificationPromoLinkClicked";
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Notification",
                            PROMO_LINK_CLICKED, PROMO_ACTION_MAX);
}

void NewTabPageHandler::HandleBubblePromoClosed(const ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Bubble",
                            PROMO_CLOSED, PROMO_ACTION_MAX);
  NotificationPromo::HandleClosed(NotificationPromo::NTP_BUBBLE_PROMO);
  Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleBubblePromoViewed(const ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Bubble",
                            PROMO_VIEWED, PROMO_ACTION_MAX);
  if (NotificationPromo::HandleViewed(NotificationPromo::NTP_BUBBLE_PROMO))
    Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleBubblePromoLinkClicked(const ListValue* args) {
  DVLOG(1) << "HandleBubblePromoLinkClicked";
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Promo.Bubble",
                            PROMO_LINK_CLICKED, PROMO_ACTION_MAX);
}

void NewTabPageHandler::HandlePageSelected(const ListValue* args) {
  page_switch_count_++;

  double page_id_double;
  CHECK(args->GetDouble(0, &page_id_double));
  int page_id = static_cast<int>(page_id_double);

  double index_double;
  CHECK(args->GetDouble(1, &index_double));
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  int previous_shown_page =
      prefs->GetInteger(prefs::kNtpShownPage) >> kPageIdOffset;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.PreviousSelectedPageType",
                            previous_shown_page, kHistogramEnumerationMax);

  prefs->SetInteger(prefs::kNtpShownPage, page_id | index);

  int shown_page_type = page_id >> kPageIdOffset;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SelectedPageType",
                            shown_page_type, kHistogramEnumerationMax);
}

void NewTabPageHandler::HandleLogTimeToClick(const ListValue* args) {
  std::string histogram_name;
  double duration;
  if (!args->GetString(0, &histogram_name) || !args->GetDouble(1, &duration)) {
    NOTREACHED();
    return;
  }

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(duration);

  if (histogram_name == "NewTabPage.TimeToClickMostVisited") {
    UMA_HISTOGRAM_LONG_TIMES("NewTabPage.TimeToClickMostVisited", delta);
  } else if (histogram_name == "NewTabPage.TimeToClickRecentlyClosed") {
    UMA_HISTOGRAM_LONG_TIMES("NewTabPage.TimeToClickRecentlyClosed", delta);
  } else if (histogram_name == "ExtendedNewTabPage.TimeToClickMostVisited") {
    UMA_HISTOGRAM_LONG_TIMES(
        "ExtendedNewTabPage.TimeToClickMostVisited", delta);
  } else if (histogram_name == "ExtendedNewTabPage.TimeToClickRecentlyClosed") {
    UMA_HISTOGRAM_LONG_TIMES(
        "ExtendedNewTabPage.TimeToClickRecentlyClosed", delta);
  } else {
    NOTREACHED();
  }
}

void NewTabPageHandler::HandleGetShouldShowApps(const ListValue* args) {
  extensions::UpdateIsAppLauncherEnabled(
      base::Bind(&NewTabPageHandler::GotIsAppLauncherEnabled,
                 AsWeakPtr()));
}

void NewTabPageHandler::GotIsAppLauncherEnabled(bool is_enabled) {
  base::FundamentalValue should_show_apps(!is_enabled);
  web_ui()->CallJavascriptFunction("ntp.gotShouldShowApps", should_show_apps);
}

// static
void NewTabPageHandler::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  // TODO(estade): should be syncable.
  prefs->RegisterIntegerPref(prefs::kNtpShownPage, APPS_PAGE_ID,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

// static
void NewTabPageHandler::GetLocalizedValues(Profile* profile,
                                           DictionaryValue* values) {
  values->SetInteger("most_visited_page_id", MOST_VISITED_PAGE_ID);
  values->SetInteger("apps_page_id", APPS_PAGE_ID);
  values->SetInteger("suggestions_page_id", SUGGESTIONS_PAGE_ID);
  // TODO(jeremycho): Add this to histograms.xml (see issue 144067).
  values->SetInteger("recently_closed_page_id", RECENTLY_CLOSED_PAGE_ID);
  // TODO(vadimt): Add this to histograms.xml (see issue 148871).
  values->SetInteger("other_devices_page_id", OTHER_DEVICES_PAGE_ID);

  PrefService* prefs = profile->GetPrefs();
  int shown_page = prefs->GetInteger(prefs::kNtpShownPage);
  values->SetInteger("shown_page_type", shown_page & ~INDEX_MASK);
  values->SetInteger("shown_page_index", shown_page & INDEX_MASK);
}

void NewTabPageHandler::Notify(chrome::NotificationType notification_type) {
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(notification_type,
                  content::Source<NewTabPageHandler>(this),
                  content::NotificationService::NoDetails());
}
