// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureUpdateController.h"

#include "CCSchedulerTestCommon.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCTiledLayerTestCommon.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "WebCompositorInitializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebThread.h>
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;
using testing::Test;


namespace {

const int kFlushPeriodFull = 4;
const int kFlushPeriodPartial = kFlushPeriodFull;

class CCTextureUpdateControllerTest;

class WebGraphicsContext3DForUploadTest : public FakeWebGraphicsContext3D {
public:
    WebGraphicsContext3DForUploadTest(CCTextureUpdateControllerTest *test)
        : m_test(test)
        , m_supportShallowFlush(true)
    { }

    virtual void flush(void);
    virtual void shallowFlushCHROMIUM(void);
    virtual GrGLInterface* onCreateGrGLInterface() { return 0; }

    virtual WebString getString(WGC3Denum name)
    {
        if (m_supportShallowFlush)
            return WebString("GL_CHROMIUM_shallow_flush");
        return WebString("");
    }

private:
    CCTextureUpdateControllerTest* m_test;
    bool m_supportShallowFlush;
};


class TextureUploaderForUploadTest : public FakeTextureUploader {
public:
    TextureUploaderForUploadTest(CCTextureUpdateControllerTest *test) : m_test(test) { }

    virtual void uploadTexture(cc::CCResourceProvider*, Parameters) OVERRIDE;

private:
    CCTextureUpdateControllerTest* m_test;
};

class TextureForUploadTest : public LayerTextureUpdater::Texture {
public:
    TextureForUploadTest() 
        : LayerTextureUpdater::Texture(nullptr)
        , m_evicted(false)
    {
    }
    virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) { }
    virtual bool backingResourceWasEvicted() const { return m_evicted; }
    void evictBackingResource() { m_evicted = true; }
private:
    bool m_evicted;
};


class CCTextureUpdateControllerTest : public Test {
public:
    CCTextureUpdateControllerTest()
        : m_queue(adoptPtr(new CCTextureUpdateQueue))
        , m_uploader(this)
        , m_compositorInitializer(m_thread.get())
        , m_fullUploadCountExpected(0)
        , m_partialCountExpected(0)
        , m_totalUploadCountExpected(0)
        , m_maxUploadCountPerUpdate(0)
        , m_numConsecutiveFlushes(0)
        , m_numDanglingUploads(0)
        , m_numTotalUploads(0)
        , m_numTotalFlushes(0)
    {
    }

public:
    void onFlush()
    {
        // Check for back-to-back flushes.
        EXPECT_EQ(0, m_numConsecutiveFlushes) << "Back-to-back flushes detected.";

        m_numDanglingUploads = 0;
        m_numConsecutiveFlushes++;
        m_numTotalFlushes++;
    }

    void onUpload()
    {
        // Check for too many consecutive uploads
        if (m_numTotalUploads < m_fullUploadCountExpected)
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodFull) << "Too many consecutive full uploads detected.";
        else
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodPartial) << "Too many consecutive partial uploads detected.";

        m_numConsecutiveFlushes = 0;
        m_numDanglingUploads++;
        m_numTotalUploads++;
    }

protected:
    virtual void SetUp()
    {
        m_context = FakeWebCompositorOutputSurface::create(adoptPtr(new WebGraphicsContext3DForUploadTest(this)));
        DebugScopedSetImplThread implThread;
        m_resourceProvider = CCResourceProvider::create(m_context.get());
    }


    void appendFullUploadsOfIndexedTextureToUpdateQueue(int count, int textureIndex)
    {
        m_fullUploadCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 300, 150);
        const TextureUploader::Parameters upload = { &m_textures[textureIndex], rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendFullUpload(upload);
    }

    void appendFullUploadsToUpdateQueue(int count)
    {
        appendFullUploadsOfIndexedTextureToUpdateQueue(count, 0);
    }

    void appendPartialUploadsOfIndexedTextureToUpdateQueue(int count, int textureIndex)
    {
        m_partialCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 100, 100);
        const TextureUploader::Parameters upload = { &m_textures[textureIndex], rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendPartialUpload(upload);
    }

    void appendPartialUploadsToUpdateQueue(int count)
    {
        appendPartialUploadsOfIndexedTextureToUpdateQueue(count, 0);
    }

    void setMaxUploadCountPerUpdate(int count)
    {
        m_maxUploadCountPerUpdate = count;
    }

    void updateTextures()
    {
        OwnPtr<CCTextureUpdateController> updateController =
            CCTextureUpdateController::create(
                NULL,
                CCProxy::implThread(),
                m_queue.release(),
                m_resourceProvider.get(),
                &m_uploader);
        updateController->finalize();
    }

protected:
    // Classes required to interact and test the CCTextureUpdateController
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    TextureForUploadTest m_textures[4];
    TextureUploaderForUploadTest m_uploader;
    OwnPtr<WebThread> m_thread;
    WebCompositorInitializer m_compositorInitializer;


    // Properties / expectations of this test
    int m_fullUploadCountExpected;
    int m_partialCountExpected;
    int m_totalUploadCountExpected;
    int m_maxUploadCountPerUpdate;

    // Dynamic properties of this test
    int m_numConsecutiveFlushes;
    int m_numDanglingUploads;
    int m_numTotalUploads;
    int m_numTotalFlushes;
};

void WebGraphicsContext3DForUploadTest::flush(void)
{
    m_test->onFlush();
}

void WebGraphicsContext3DForUploadTest::shallowFlushCHROMIUM(void)
{
    m_test->onFlush();
}

void TextureUploaderForUploadTest::uploadTexture(cc::CCResourceProvider*, Parameters)
{
    m_test->onUpload();
}

// ZERO UPLOADS TESTS
TEST_F(CCTextureUpdateControllerTest, ZeroUploads)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(0, m_numTotalFlushes);
    EXPECT_EQ(0, m_numTotalUploads);
}


// ONE UPLOAD TESTS
TEST_F(CCTextureUpdateControllerTest, OneFullUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numTotalFlushes);
    EXPECT_EQ(1, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, OnePartialUpload)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numTotalFlushes);
    EXPECT_EQ(1, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, OneFullOnePartialUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numTotalFlushes);
    EXPECT_EQ(2, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}


// This class of tests upload a number of textures that is a multiple of the flush period.
const int fullUploadFlushMultipler = 7;
const int fullCount = fullUploadFlushMultipler * kFlushPeriodFull;

const int partialUploadFlushMultipler = 11;
const int partialCount = partialUploadFlushMultipler * kFlushPeriodPartial;

TEST_F(CCTextureUpdateControllerTest, ManyFullUploads)
{
    appendFullUploadsToUpdateQueue(fullCount);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(fullUploadFlushMultipler, m_numTotalFlushes);
    EXPECT_EQ(fullCount, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, ManyPartialUploads)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(partialCount);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(partialUploadFlushMultipler, m_numTotalFlushes);
    EXPECT_EQ(partialCount, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, ManyFullManyPartialUploads)
{
    appendFullUploadsToUpdateQueue(fullCount);
    appendPartialUploadsToUpdateQueue(partialCount);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(fullUploadFlushMultipler + partialUploadFlushMultipler, m_numTotalFlushes);
    EXPECT_EQ(fullCount + partialCount, m_numTotalUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

class FakeCCTextureUpdateControllerClient : public cc::CCTextureUpdateControllerClient {
public:
    FakeCCTextureUpdateControllerClient() { reset(); }
    void reset() { m_readyToFinalizeCalled = false; }
    bool readyToFinalizeCalled() const { return m_readyToFinalizeCalled; }

    virtual void readyToFinalizeTextureUpdates() OVERRIDE { m_readyToFinalizeCalled = true; }

protected:
    bool m_readyToFinalizeCalled;
};

class FakeCCTextureUpdateController : public cc::CCTextureUpdateController {
public:
    static PassOwnPtr<FakeCCTextureUpdateController> create(cc::CCTextureUpdateControllerClient* client, cc::CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
    {
        return adoptPtr(new FakeCCTextureUpdateController(client, thread, queue, resourceProvider, uploader));
    }

    void setNow(base::TimeTicks time) { m_now = time; }
    virtual base::TimeTicks now() const OVERRIDE { return m_now; }
    void setUpdateMoreTexturesTime(base::TimeDelta time) { m_updateMoreTexturesTime = time; }
    virtual base::TimeDelta updateMoreTexturesTime() const OVERRIDE { return m_updateMoreTexturesTime; }
    void setUpdateMoreTexturesSize(size_t size) { m_updateMoreTexturesSize = size; }
    virtual size_t updateMoreTexturesSize() const OVERRIDE { return m_updateMoreTexturesSize; }

protected:
    FakeCCTextureUpdateController(cc::CCTextureUpdateControllerClient* client, cc::CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
        : cc::CCTextureUpdateController(client, thread, queue, resourceProvider, uploader)
        , m_updateMoreTexturesSize(0) { }

    base::TimeTicks m_now;
    base::TimeDelta m_updateMoreTexturesTime;
    size_t m_updateMoreTexturesSize;
};

static void runPendingTask(FakeCCThread* thread, FakeCCTextureUpdateController* controller)
{
    EXPECT_TRUE(thread->hasPendingTask());
    controller->setNow(controller->now() + base::TimeDelta::FromMilliseconds(thread->pendingDelayMs()));
    thread->runPendingTask();
}

TEST_F(CCTextureUpdateControllerTest, UpdateMoreTextures)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(3);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setNow(
        controller->now() + base::TimeDelta::FromMilliseconds(1));
    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(100));
    controller->setUpdateMoreTexturesSize(1);
    // Not enough time for any updates.
    controller->performMoreUpdates(
        controller->now() + base::TimeDelta::FromMilliseconds(90));
    EXPECT_FALSE(thread.hasPendingTask());

    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(100));
    controller->setUpdateMoreTexturesSize(1);
    // Only enough time for 1 update.
    controller->performMoreUpdates(
        controller->now() + base::TimeDelta::FromMilliseconds(120));
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_EQ(1, m_numTotalUploads);

    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(100));
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for 2 updates.
    controller->performMoreUpdates(
        controller->now() + base::TimeDelta::FromMilliseconds(220));
    runPendingTask(&thread, controller.get());
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(3, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, NoMoreUpdates)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(2);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setNow(
        controller->now() + base::TimeDelta::FromMilliseconds(1));
    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(100));
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for 3 updates but only 2 necessary.
    controller->performMoreUpdates(
        controller->now() + base::TimeDelta::FromMilliseconds(310));
    runPendingTask(&thread, controller.get());
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numTotalUploads);

    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(100));
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for updates but no more updates left.
    controller->performMoreUpdates(
        controller->now() + base::TimeDelta::FromMilliseconds(310));
    // 0-delay task used to call readyToFinalizeTextureUpdates().
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, UpdatesCompleteInFiniteTime)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(2);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setNow(
        controller->now() + base::TimeDelta::FromMilliseconds(1));
    controller->setUpdateMoreTexturesTime(
        base::TimeDelta::FromMilliseconds(500));
    controller->setUpdateMoreTexturesSize(1);

    for (int i = 0; i < 100; i++) {
        if (client.readyToFinalizeCalled())
            break;

        // Not enough time for any updates.
        controller->performMoreUpdates(
            controller->now() + base::TimeDelta::FromMilliseconds(400));

        if (thread.hasPendingTask())
            runPendingTask(&thread, controller.get());
    }

    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, ClearUploadsToEvictedResources)
{
    appendFullUploadsOfIndexedTextureToUpdateQueue(1, 0);
    appendPartialUploadsOfIndexedTextureToUpdateQueue(1, 1);
    appendFullUploadsOfIndexedTextureToUpdateQueue(1, 2);
    appendPartialUploadsOfIndexedTextureToUpdateQueue(1, 3);
    DebugScopedSetImplThread implThread;

    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(2u, m_queue->fullUploadSize());
    EXPECT_EQ(2u, m_queue->partialUploadSize());

    m_textures[0].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(1u, m_queue->fullUploadSize());
    EXPECT_EQ(2u, m_queue->partialUploadSize());

    m_textures[3].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(1u, m_queue->fullUploadSize());
    EXPECT_EQ(1u, m_queue->partialUploadSize());

    m_textures[2].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(0u, m_queue->fullUploadSize());
    EXPECT_EQ(1u, m_queue->partialUploadSize());

    m_textures[1].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(0u, m_queue->fullUploadSize());
    EXPECT_EQ(0u, m_queue->partialUploadSize());
}

} // namespace
