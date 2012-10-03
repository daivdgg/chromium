// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnthrottledTextureUploader_h
#define UnthrottledTextureUploader_h

#include "base/basictypes.h"
#include "CCResourceProvider.h"
#include "TextureUploader.h"

namespace cc {

class UnthrottledTextureUploader : public TextureUploader {
public:
    static PassOwnPtr<UnthrottledTextureUploader> create()
    {
        return adoptPtr(new UnthrottledTextureUploader());
    }
    virtual ~UnthrottledTextureUploader() { }

    virtual size_t numBlockingUploads() OVERRIDE;
    virtual void markPendingUploadsAsNonBlocking() OVERRIDE;
    virtual double estimatedTexturesPerSecond() OVERRIDE;
    virtual void uploadTexture(CCResourceProvider* resourceProvider, Parameters upload) OVERRIDE;

protected:
    UnthrottledTextureUploader() { }

private:
    DISALLOW_COPY_AND_ASSIGN(UnthrottledTextureUploader);
};

}

#endif
