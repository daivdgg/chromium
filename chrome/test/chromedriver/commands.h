// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_COMMANDS_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/session_map.h"

namespace base {
class DictionaryValue;
class Value;
}

class ChromeLauncher;
struct Session;

// Gets status/info about ChromeDriver.
Status ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

// Creates a new session.
Status ExecuteNewSession(
    SessionMap* session_map,
    ChromeLauncher* launcher,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

// Quits all sessions.
Status ExecuteQuitAll(
    Command quit_command,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

// Quits a particular session.
Status ExecuteQuit(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Loads a URL.
Status ExecuteGet(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Evaluates a given script with arguments.
Status ExecuteExecuteScript(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Changes the targeted frame for the given session.
Status ExecuteSwitchToFrame(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Get the current page title.
Status ExecuteGetTitle(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for an element on the page, starting from the document root.
Status ExecuteFindElement(
    int interval_ms,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for multiple elements on the page, starting from the document root.
Status ExecuteFindElements(
    int interval_ms,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for an element on the page, starting from the given element.
Status ExecuteFindChildElement(
    int interval_ms,
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for multiple elements on the page, starting from the given element.
Status ExecuteFindChildElements(
    int interval_ms,
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Move the mouse to the given element.
Status ExecuteHoverOverElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click on the element.
Status ExecuteClickElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Clear a TEXTAREA or text INPUT element's value.
Status ExecuteClearElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Send a sequence of key strokes to an element.
Status ExecuteSendKeysToElement(
    Session* session,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Configure the amount of time that a particular type of operation can execute
// for before they are aborted and a timeout error is returned to the client.
Status ExecuteSetTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Get the current page url.
Status ExecuteGetCurrentUrl(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Navigate backward in the browser history.
Status ExecuteGoBack(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Navigate forward in the browser history.
Status ExecuteGoForward(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Refresh the current page.
Status ExecuteRefresh(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Move the mouse by an offset of the specificed element.
Status ExecuteMouseMoveTo(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click a mouse button at the coordinates set by the last moveto.
Status ExecuteMouseClick(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click and hold a mouse button at the coordinates set by the last moveto.
Status ExecuteMouseButtonDown(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Releases the mouse button previously held (where the mouse is currently at).
Status ExecuteMouseButtonUp(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Double-clicks at the current mouse coordinates (set by last moveto).
Status ExecuteMouseDoubleClick(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
