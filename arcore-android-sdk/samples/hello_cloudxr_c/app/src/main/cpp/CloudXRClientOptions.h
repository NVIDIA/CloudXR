/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef CLOUDXR_CLIENT_OPTIONS_H
#define CLOUDXR_CLIENT_OPTIONS_H

#include <string>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <iostream>
#include <algorithm>

#include "CloudXROptionsParser.h"
#include "CloudXRClient.h"

namespace CloudXR {

class ClientOptions : public OptionsParser
{
public:
    std::string mServerIP;
    bool mWindowed;
    bool mBtnRemap;
    bool mTestLatency;
    bool mEnableAlpha;
    bool mSendAudio;
    bool mReceiveAudio;
    float mMaxResFactor;
    int32_t mLogMaxAgeDays;
    int32_t mLogMaxSizeKB;
    uint32_t mNumVideoStreams;
    uint32_t mReceiverMode;
    uint32_t mDebugFlags;
    int32_t mFoveation;
    cxrGraphicsContextType mGfxType;
    std::string mUserData;

    ClientOptions() :
            mServerIP{""},
            mWindowed(false),
            mBtnRemap(true),
            mTestLatency(false),
            mEnableAlpha(false),
            mSendAudio(false),
            mReceiveAudio(true),
            mMaxResFactor(1.2f),
            mLogMaxAgeDays(-1),
            mLogMaxSizeKB(-1),
            mNumVideoStreams(CXR_NUM_VIDEO_STREAMS_XR),
            mReceiverMode(cxrStreamingMode_XR),
            mDebugFlags(0),
            mFoveation(0),
#ifdef _WIN32
            mGfxType(cxrGraphicsContext_D3D11)
#elif defined(__linux__)
#ifdef __aarch64__
            mGfxType(cxrGraphicsContext_GLES)
#else
            mGfxType(cxrGraphicsContext_Cuda)
#endif
#endif
    {
        AddOption("server", "s", true, "IP address of server to connect to",
                  HANDLER_LAMBDA_FN {mServerIP = tok; return ParseStatus_Success;});
                  
        AddOption("log-verbose", "v", false, "Enable more verbose logging",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_LogVerbose; return ParseStatus_Success;});
        AddOption("log-quiet", "q", false, "Disable logging to file, use only debug output",
                  HANDLER_LAMBDA_FN{ mDebugFlags |= cxrDebugFlags_LogQuiet; return ParseStatus_Success; });
        AddOption("trace-stream-events", "t", false, "Enable tracing of streaming events",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_TraceStreamEvents; return ParseStatus_Success;});
        AddOption("trace-local-events", "tle", false, "Enable tracing of local events",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_TraceLocalEvents; return ParseStatus_Success;});
        AddOption("trace-qos-stats", "tqs", false, "Enable tracing of QoS statistics",
                  HANDLER_LAMBDA_FN{ mDebugFlags |= cxrDebugFlags_TraceQosStats; return ParseStatus_Success; });
        AddOption("dump-images", "d", false, "Dump streamed images to disk periodically",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_DumpImages; return ParseStatus_Success;});
        AddOption("capture-client-bitstream", "ccb", false, "Capture the client-received video bitstream to CloudXR log folder on client.",
                  HANDLER_LAMBDA_FN{ mDebugFlags |= cxrDebugFlags_CaptureClientBitstream; return ParseStatus_Success; });
        AddOption("capture-server-bitstream", "csb", false, "Capture the server-sent video bitstream to CloudXR log folder on server.",
                  HANDLER_LAMBDA_FN{ mDebugFlags |= cxrDebugFlags_CaptureServerBitstream; return ParseStatus_Success; });
        AddOption("dump-audio", "da", false, "Dump streamed audio to disk",
                  HANDLER_LAMBDA_FN{ mDebugFlags |= cxrDebugFlags_DumpAudio; return ParseStatus_Success; });
        AddOption("embed-server-info", "esi", false, "Embed server info in frames during streaming",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_EmbedServerInfo; return ParseStatus_Success; });
        AddOption("embed-client-info", "eci", false, "Embed client info in framebuffers",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_EmbedClientInfo; return ParseStatus_Success; });
        AddOption("log-privacy-disable", "p", false, "Disable privacy filtering in logging",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_LogPrivacyDisabled; return ParseStatus_Success; });
        AddOption("enable-sxr-decoder", "sxr", false, "Enable experimental SXR decoder on Android devices that support it",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_EnableSXRDecoder; return ParseStatus_Success; });
        AddOption("enable-ir-decoder", "ird", false, "Enable experimental ImageReader decoder on Android devices (reqs sdk >= 26)",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_EnableImageReaderDecoder; return ParseStatus_Success; });
        AddOption("fallback-decoder", "fbd", false, "If available, try to use a fallback video decoder for the platform.",
                  HANDLER_LAMBDA_FN { mDebugFlags |= cxrDebugFlags_FallbackDecoder; return ParseStatus_Success; });

        AddOption("windowed", "w", false, "Use windowed mode instead of SteamVR",
                  HANDLER_LAMBDA_FN { mWindowed = true; return ParseStatus_Success;});
        AddOption("no-button-remap", "b", false, "Do not remap various controller buttons to SteamVR system menu and other functions",
                  HANDLER_LAMBDA_FN { mBtnRemap = false; return ParseStatus_Success;});
        AddOption("max-res-factor", "m", true, "Maximum stream resolution as factor of given device res (effectively oversampling). [0.5-2.0]",
            HANDLER_LAMBDA_FN
            {
                float max;
                std::stringstream ss(tok); ss >> max;
                if (max >= 0.5f && max <= 2.0f)
                {
                    mMaxResFactor = max;
                    return ParseStatus_Success;
                }
                return ParseStatus_BadVal;
            });
        AddOption("latency-test", "l", false, "Runs local latency testing, where screen is black when no input, changes to white with input",
            HANDLER_LAMBDA_FN{ mTestLatency = true; return ParseStatus_Success; });
        AddOption("enable-alpha", "a", false, "Enable streaming alpha",
            HANDLER_LAMBDA_FN{ mEnableAlpha = true; return ParseStatus_Success; });
        AddOption("enable-send-audio", "sa", false, "Enable sending audio to the server",
            HANDLER_LAMBDA_FN{ mSendAudio = true; return ParseStatus_Success; });
        AddOption("disable-receive-audio", "dra", false, "Disable receiving audio from the server",
            HANDLER_LAMBDA_FN{ mReceiveAudio = false; return ParseStatus_Success; });
        AddOption("log-max-days", "lmd", true, "Maximum number of days until which logs persist.  -1 resets to default, 0 to never prune, or [1-365] days.",
            HANDLER_LAMBDA_FN
            {
                int32_t max;
                std::stringstream ss(tok); ss >> max;
                // picking something arbitrarily large as cutoff - one year
                // and allow -1 as that will reset option member to 'logger default'
                // and allow 0 as 'never prune'
                if (max >= -1 && max <= 365)
                {
                    mLogMaxAgeDays = max;
                    return ParseStatus_Success;
                }
                return ParseStatus_BadVal;
            });
        AddOption("log-max-kb", "lmk", true, "Maximum log size in kilobytes. -1 resets default, 0 for no cap, max 1024*1024K (1GB)",
            HANDLER_LAMBDA_FN
            {
                int32_t max;
                std::stringstream ss(tok); ss >> max;
                // picking something arbitrarily large as cutoff - 1GB
                // and allow -1 as that will reset option member to 'logger default'
                // and allow 0 as 'never cap'
                if (max >= -1 && max <= 1024 * 1024)
                {
                    mLogMaxSizeKB = max;
                    return ParseStatus_Success;
                }
                return ParseStatus_BadVal;
            });
        AddOption("num-video-streams", "ns", true, "In case of generic streaming mode, this option specifies number of video stream",
            HANDLER_LAMBDA_FN { std::stringstream ss(tok); ss >> mNumVideoStreams; return ParseStatus_Success; });
        AddOption("receiver-mode", "r", true, "Choose XR or generic receiver mode",
            HANDLER_LAMBDA_FN { std::stringstream ss(tok); ss >> mReceiverMode; return ParseStatus_Success; });
        AddOption("graphics-type", "g", true, "Choose graphics context type. [gles|cuda|d3d11]",
            HANDLER_LAMBDA_FN 
            { 
                if (tok == "cuda")
                {
                    mGfxType = cxrGraphicsContext_Cuda;
                }
 #ifdef __aarch64__
                else if (tok == "gles")
                {
                    mGfxType = cxrGraphicsContext_GLES;
                }
 #elif _WIN32
                else if (tok == "d3d11")
                {
                    mGfxType = cxrGraphicsContext_D3D11;
                }
 #endif
                else
                {
                    return ParseStatus_BadVal;
                }

                return ParseStatus_Success;
            });
        AddOption("user-data", "u", true, "Send a user string to the server",
            HANDLER_LAMBDA_FN { mUserData = tok; return ParseStatus_Success; });
        AddOption("foveation", "f", true, "Enable foveated scaling at given percentage scale [0-100]",
            HANDLER_LAMBDA_FN
            {
                int fov;
                std::stringstream ss(tok); ss >> fov;
                if (fov >= 0 && fov <= 100)
                {
                    
                    if (fov==0)
                        mFoveation = 0; // explicitly catch 0 request as 'no foveation', important for overriding some default...
                    else
                        mFoveation = (fov>25) ? fov : 25; // values under 25% seem useless, so we floor there.
                    return ParseStatus_Success;
                }
                return ParseStatus_BadVal;
            });
    }

    // fast constructor for actual cmdline OSes
    ClientOptions(int argc, char **argv) : ClientOptions()
    {
        ParseArgs(argc, argv);
    }
};

}; // namespace CloudXR

#endif // CLOUDXR_CLIENT_OPTIONS_H
