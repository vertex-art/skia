/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_Context_DEFINED
#define skgpu_graphite_Context_DEFINED

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkShader.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/GraphiteTypes.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/private/base/SingleOwner.h"

#include <chrono>
#include <functional>
#include <memory>

class SkColorSpace;
class SkRuntimeEffect;
class SkTraceMemoryDump;

namespace skgpu::graphite {

class BackendTexture;
class Buffer;
class ClientMappedBufferManager;
class Context;
class ContextPriv;
class GlobalCache;
class PaintOptions;
class PlotUploadTracker;
class QueueManager;
class Recording;
class ResourceProvider;
class SharedContext;
class TextureProxy;

class SK_API Context final {
public:
    Context(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context();

    BackendApi backend() const;

    std::unique_ptr<Recorder> makeRecorder(const RecorderOptions& = {});

    bool insertRecording(const InsertRecordingInfo&);
    bool submit(SyncToCpu = SyncToCpu::kNo);

    /** Returns true if there is work that was submitted to the GPU that has not finished. */
    bool hasUnfinishedGpuWork() const;

    void asyncRescaleAndReadPixels(const SkImage* image,
                                   const SkImageInfo& dstImageInfo,
                                   const SkIRect& srcRect,
                                   SkImage::RescaleGamma rescaleGamma,
                                   SkImage::RescaleMode rescaleMode,
                                   SkImage::ReadPixelsCallback callback,
                                   SkImage::ReadPixelsContext context);

    void asyncRescaleAndReadPixels(const SkSurface* surface,
                                   const SkImageInfo& dstImageInfo,
                                   const SkIRect& srcRect,
                                   SkImage::RescaleGamma rescaleGamma,
                                   SkImage::RescaleMode rescaleMode,
                                   SkImage::ReadPixelsCallback callback,
                                   SkImage::ReadPixelsContext context);

    void asyncRescaleAndReadPixelsYUV420(const SkImage*,
                                         SkYUVColorSpace yuvColorSpace,
                                         sk_sp<SkColorSpace> dstColorSpace,
                                         const SkIRect& srcRect,
                                         const SkISize& dstSize,
                                         SkImage::RescaleGamma rescaleGamma,
                                         SkImage::RescaleMode rescaleMode,
                                         SkImage::ReadPixelsCallback callback,
                                         SkImage::ReadPixelsContext context);

    void asyncRescaleAndReadPixelsYUV420(const SkSurface*,
                                         SkYUVColorSpace yuvColorSpace,
                                         sk_sp<SkColorSpace> dstColorSpace,
                                         const SkIRect& srcRect,
                                         const SkISize& dstSize,
                                         SkImage::RescaleGamma rescaleGamma,
                                         SkImage::RescaleMode rescaleMode,
                                         SkImage::ReadPixelsCallback callback,
                                         SkImage::ReadPixelsContext context);

    void asyncRescaleAndReadPixelsYUVA420(const SkImage*,
                                          SkYUVColorSpace yuvColorSpace,
                                          sk_sp<SkColorSpace> dstColorSpace,
                                          const SkIRect& srcRect,
                                          const SkISize& dstSize,
                                          SkImage::RescaleGamma rescaleGamma,
                                          SkImage::RescaleMode rescaleMode,
                                          SkImage::ReadPixelsCallback callback,
                                          SkImage::ReadPixelsContext context);

    void asyncRescaleAndReadPixelsYUVA420(const SkSurface*,
                                          SkYUVColorSpace yuvColorSpace,
                                          sk_sp<SkColorSpace> dstColorSpace,
                                          const SkIRect& srcRect,
                                          const SkISize& dstSize,
                                          SkImage::RescaleGamma rescaleGamma,
                                          SkImage::RescaleMode rescaleMode,
                                          SkImage::ReadPixelsCallback callback,
                                          SkImage::ReadPixelsContext context);

    /**
     * Checks whether any asynchronous work is complete and if so calls related callbacks.
     */
    void checkAsyncWorkCompletion();

    /**
     * Called to delete the passed in BackendTexture. This should only be called if the
     * BackendTexture was created by calling Recorder::createBackendTexture on a Recorder created
     * from this Context. If the BackendTexture is not valid or does not match the BackendApi of the
     * Context then nothing happens.
     *
     * Otherwise this will delete/release the backend object that is wrapped in the BackendTexture.
     * The BackendTexture will be reset to an invalid state and should not be used again.
     */
    void deleteBackendTexture(const BackendTexture&);

    /**
     * Frees GPU resources created and held by the Context. Can be called to reduce GPU memory
     * pressure. Any resources that are still in use (e.g. being used by work submitted to the GPU)
     * will not be deleted by this call. If the caller wants to make sure all resources are freed,
     * then they should first make sure to submit and wait on any outstanding work.
     */
    void freeGpuResources();

    /**
     * Purge GPU resources on the Context that haven't been used in the past 'msNotUsed'
     * milliseconds or are otherwise marked for deletion, regardless of whether the context is under
     * budget.
     */
    void performDeferredCleanup(std::chrono::milliseconds msNotUsed);

    /**
     * Returns the number of bytes of the Context's gpu memory cache budget that are currently in
     * use.
     */
    size_t currentBudgetedBytes() const;

    /**
     * Returns the size of Context's gpu memory cache budget in bytes.
     */
    size_t maxBudgetedBytes() const;

    /**
     * Enumerates all cached GPU resources owned by the Context and dumps their memory to
     * traceMemoryDump.
     */
    void dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const;

    /**
     * Returns true if the backend-specific context has gotten into an unrecoverarble, lost state
     * (e.g. if we've gotten a VK_ERROR_DEVICE_LOST in the Vulkan backend).
     */
    bool isDeviceLost() const;

    /**
     * Returns the maximum texture dimension supported by the underlying backend.
     */
    int maxTextureSize() const;

    /*
     * Does this context support protected content?
     */
    bool supportsProtectedContent() const;

    // Provides access to functions that aren't part of the public API.
    ContextPriv priv();
    const ContextPriv priv() const;  // NOLINT(readability-const-return-type)

    class ContextID {
    public:
        static Context::ContextID Next();

        ContextID() : fID(SK_InvalidUniqueID) {}

        bool operator==(const ContextID& that) const { return fID == that.fID; }
        bool operator!=(const ContextID& that) const { return !(*this == that); }

        void makeInvalid() { fID = SK_InvalidUniqueID; }
        bool isValid() const { return fID != SK_InvalidUniqueID; }

    private:
        constexpr ContextID(uint32_t id) : fID(id) {}
        uint32_t fID;
    };

    ContextID contextID() const { return fContextID; }

protected:
    Context(sk_sp<SharedContext>, std::unique_ptr<QueueManager>, const ContextOptions&);

private:
    friend class ContextPriv;
    friend class ContextCtorAccessor;

    struct PixelTransferResult {
        using ConversionFn = void(void* dst, const void* mappedBuffer);
        // If null then the transfer could not be performed. Otherwise this buffer will contain
        // the pixel data when the transfer is complete.
        sk_sp<Buffer> fTransferBuffer;
        // Size of the read.
        SkISize fSize;
        // RowBytes for transfer buffer data
        size_t fRowBytes;
        // If this is null then the transfer buffer will contain the data in the requested
        // color type. Otherwise, when the transfer is done this must be called to convert
        // from the transfer buffer's color type to the requested color type.
        std::function<ConversionFn> fPixelConverter;
    };

    SingleOwner* singleOwner() const { return &fSingleOwner; }

    // Must be called in Make() to handle one-time GPU setup operations that can possibly fail and
    // require Context::Make() to return a nullptr.
    bool finishInitialization();

    void checkForFinishedWork(SyncToCpu);

    template <typename SrcPixels> struct AsyncParams;

    template <typename ReadFn, typename... ExtraArgs>
    void asyncRescaleAndReadImpl(ReadFn Context::* asyncRead,
                                 SkImage::RescaleGamma rescaleGamma,
                                 SkImage::RescaleMode rescaleMode,
                                 const AsyncParams<SkImage>&,
                                 ExtraArgs...);

    // Recorder is optional and will be used if drawing operations are required. If no Recorder is
    // provided but drawing operations are needed, a new Recorder will be created automatically.
    void asyncReadPixels(std::unique_ptr<Recorder>, const AsyncParams<SkImage>&);
    void asyncReadPixelsYUV420(std::unique_ptr<Recorder>,
                               const AsyncParams<SkImage>&,
                               SkYUVColorSpace);

    // Like asyncReadPixels() except it performs no fallbacks, and requires that the texture be
    // readable. However, the texture does not need to be sampleable.
    void asyncReadTexture(std::unique_ptr<Recorder>,
                          const AsyncParams<TextureProxy>&,
                          const SkColorInfo& srcColorInfo);

    // Inserts a texture to buffer transfer task, used by asyncReadPixels methods. If the
    // Recorder is non-null, tasks will be added to the Recorder's list; otherwise the transfer
    // tasks will be added to the queue manager directly.
    PixelTransferResult transferPixels(Recorder*,
                                       const TextureProxy* srcProxy,
                                       const SkColorInfo& srcColorInfo,
                                       const SkColorInfo& dstColorInfo,
                                       const SkIRect& srcRect);

    // If the recorder is non-null, it will be snapped and inserted with the assumption that the
    // copy tasks (and possibly preparatory draw tasks) have already been added to the Recording.
    void finalizeAsyncReadPixels(std::unique_ptr<Recorder>,
                                 SkSpan<PixelTransferResult>,
                                 SkImage::ReadPixelsCallback callback,
                                 SkImage::ReadPixelsContext callbackContext);

    sk_sp<SharedContext> fSharedContext;
    std::unique_ptr<ResourceProvider> fResourceProvider;
    std::unique_ptr<QueueManager> fQueueManager;
    std::unique_ptr<ClientMappedBufferManager> fMappedBufferManager;

    // In debug builds we guard against improper thread handling. This guard is passed to the
    // ResourceCache for the Context.
    mutable SingleOwner fSingleOwner;

#if defined(GRAPHITE_TEST_UTILS)
    // In test builds a Recorder may track the Context that was used to create it.
    bool fStoreContextRefInRecorder = false;
    // If this tracking is on, to allow the client to safely delete this Context or its Recorders
    // in any order we must also track the Recorders created here.
    std::vector<Recorder*> fTrackedRecorders;
#endif

    // Needed for MessageBox handling
    const ContextID fContextID;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_Context_DEFINED
