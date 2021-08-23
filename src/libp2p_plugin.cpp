#include <string>
#include <memory>

#include "p2p_api.h"
#include <src\headers\VapourSynth4.h>

typedef struct {
    VSVideoInfo vi;
    uint32_t src_format;
    VSNode* node;
    p2p_packing p2p_output_format;
} PackP2PData;

typedef struct {
    VSVideoInfo vi;
    VSNode* node;
    p2p_packing p2p_input_format;
} UnpackP2PData;

static const VSFrame* VS_CC packGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    PackP2PData* d = reinterpret_cast<PackP2PData*>(instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrame* dst = vsapi->newVideoFrame(&d->vi.format, d->vi.width, d->vi.height, src, core);

        p2p_buffer_param p = {};
        p.packing = d->p2p_output_format;
        p.width = d->vi.width;
        p.height = d->vi.height;
        p.dst[0] = vsapi->getWritePtr(dst, 0);
        p.dst_stride[0] = vsapi->getStride(dst, 0);

        for (int plane = 0; plane < 3; plane++) {
            p.src[plane] = vsapi->getReadPtr(src, plane);
            p.src_stride[plane] = vsapi->getStride(src, plane);
        }

        p2p_pack_frame(&p, P2P_ALPHA_SET_ONE);

        VSMap* m = vsapi->getFramePropertiesRW(dst);
        vsapi->mapSetInt(m, "_P2PInputFormat", d->src_format, maReplace);

        vsapi->freeFrame(src);

        return dst;
    }

    return nullptr;
}

static void VS_CC packFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    PackP2PData* d = reinterpret_cast<PackP2PData*>(instanceData);
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC packCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    std::unique_ptr<PackP2PData> d(new PackP2PData());

    d->node = vsapi->mapGetNode(in, "clip", 0, 0);
    const VSVideoInfo* vi = vsapi->getVideoInfo(d->node);
    d->src_format = vsapi->queryVideoFormatID(vi->format.colorFamily, vi->format.sampleType, vi->format.bitsPerSample, vi->format.subSamplingW, vi->format.subSamplingH, core);

    switch (d->src_format)
    {
        case pfRGB24:
            d->p2p_output_format = p2p_argb32;
            break;
        case pfRGB30:
            d->p2p_output_format = p2p_rgb30;
            break;
        case pfRGB48:
            d->p2p_output_format = p2p_argb64;
            break;
        default:
            vsapi->mapSetError(out, "Pack: only RGB24, RGB30 and RGB48 inputs are supported!");
            vsapi->freeNode(d->node);
            return;
    }

    d->vi = *vi;
    vsapi->getVideoFormatByID(&d->vi.format, pfGray32, core);

    if (d->src_format == pfRGB48) {
        d->vi.width *= 2;
    }

    VSFilterDependency deps[] = { {d->node, rpStrictSpatial} };
    vsapi->createVideoFilter(out, "Pack", &d->vi, packGetFrame, packFree, fmParallel, deps, 1, d.get(), core);
    d.release();
}

static const VSFrame* VS_CC unpackGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    UnpackP2PData* d = reinterpret_cast<UnpackP2PData*>(instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrame* dst = vsapi->newVideoFrame(&d->vi.format, d->vi.width, d->vi.height, src, core);

        p2p_buffer_param p = {};
        p.packing = d->p2p_input_format;
        p.width = d->vi.width;
        p.height = d->vi.height;
        p.src[0] = vsapi->getReadPtr(src, 0);
        p.src_stride[0] = vsapi->getStride(src, 0);

        for (int plane = 0; plane < 3; plane++) {
            p.dst[plane] = vsapi->getWritePtr(dst, plane);
            p.dst_stride[plane] = vsapi->getStride(dst, plane);
        }

        p2p_unpack_frame(&p, 0);

        vsapi->freeFrame(src);

        return dst;
    }

    return nullptr;
}

static void VS_CC unpackFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    UnpackP2PData* d = reinterpret_cast<UnpackP2PData*>(instanceData);
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC unpackCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    std::unique_ptr<UnpackP2PData> d(new UnpackP2PData());

    d->node = vsapi->mapGetNode(in, "clip", 0, 0);
    const VSVideoInfo* vi = vsapi->getVideoInfo(d->node);

    if (vsapi->queryVideoFormatID(vi->format.colorFamily, vi->format.sampleType, vi->format.bitsPerSample, vi->format.subSamplingW, vi->format.subSamplingH, core) != pfGray32) {
        vsapi->mapSetError(out, "Unpack: only pfGray32 intput is supported!");
        vsapi->freeNode(d->node);
        return;
    }

    char errorMsg[1024] = {0};
    const VSFrame* frame0 = vsapi->getFrame(0, d->node, errorMsg, 1024);
    if (!frame0) {
        vsapi->mapSetError(out, ("Unpack: failed to retrieve first frame from clip. Error message: " + std::string{ errorMsg }).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    const VSMap* m = vsapi->getFramePropertiesRO(frame0);

    int err;
    uint32_t input_format = static_cast<uint32_t>(vsapi->mapGetInt(m, "_P2PInputFormat", 0, &err));

    vsapi->freeFrame(frame0);

    if (err)
        return;

    switch (input_format)
    {
        case pfRGB24:
            d->p2p_input_format = p2p_argb32;
            break;
        case pfRGB30:
            d->p2p_input_format = p2p_rgb30;
            break;
        case pfRGB48:
            d->p2p_input_format = p2p_argb64;
            break;
        default:
            vsapi->mapSetError(out, "Unpack: unsupported source format!");
            return;
    }

    d->vi = *vi;
    vsapi->getVideoFormatByID(&d->vi.format, input_format, core);

    if (input_format == pfRGB48) {
        d->vi.width /= 2;
    }

    VSFilterDependency deps[] = { {d->node, rpStrictSpatial} };
    vsapi->createVideoFilter(out, "Unpack", &d->vi, unpackGetFrame, unpackFree, fmParallel, deps, 1, d.get(), core);
    d.release();
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
    vspapi->configPlugin("com.djatom.libp2p", "libp2p", "libp2p rgb formats packer/unpacker", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("Pack", "clip:vnode;", "clip:vnode;", packCreate, NULL, plugin);
    vspapi->registerFunction("Unpack", "clip:vnode;", "clip:vnode;", unpackCreate, NULL, plugin);
}