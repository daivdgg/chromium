// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_TEST_REQUEST_INTERCEPTOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_TEST_REQUEST_INTERCEPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace policy {

// Intercepts all requests to the given hostname while in scope, and allows
// queuing callbacks to handle expected requests. Must be created and destroyed
// while the IO thread is valid.
class TestRequestInterceptor {
 public:
  // A callback that returns a new URLRequestJob given a URLRequest.
  // This is used to queue callbacks that will handle expected requests.
  typedef base::Callback<
      net::URLRequestJob*(net::URLRequest*, net::NetworkDelegate*)> JobCallback;

  // Will intercept request to |hostname| made over HTTP.
  explicit TestRequestInterceptor(const std::string& hostname);
  virtual ~TestRequestInterceptor();

  // Returns the number of pending callback jobs that haven't been used yet.
  size_t GetPendingSize() const;

  // Queues |callback| to handle a request to |hostname_|. Each callback is
  // used only once, and in the order that they're pushed.
  void PushJobCallback(const JobCallback& callback);

  // Returns a JobCallback that will fail with the given network |error|.
  static JobCallback ErrorJob(int error);

  // Returns a JobCallback that will fail with HTTP 400 Bad Request.
  static JobCallback BadRequestJob();

  // Returns a JobCallback that will process a policy register request that
  // should succeed. The request parameters are validated, and an appropriate
  // response is sent back.
  // |expected_type| is the expected type in the register request.
  // If |expect_reregister| is true then the request must have the reregister
  // flag set; otherwise the flag must be not set.
  static JobCallback RegisterJob(
      enterprise_management::DeviceRegisterRequest::Type expected_type,
      bool expect_reregister);

 private:
  class Delegate;

  const std::string hostname_;

  // Owned by URLRequestFilter. This handle is valid on IO and only while the
  // interceptor is valid.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestInterceptor);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_TEST_REQUEST_INTERCEPTOR_H_
