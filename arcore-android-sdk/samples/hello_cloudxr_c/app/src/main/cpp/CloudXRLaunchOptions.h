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

#ifndef LAUNCHOPTIONS_H
#define LAUNCHOPTIONS_H

#include <string>
#include <sstream>
#include <fstream>

#include "CloudXR.h"

namespace CloudXR {

class LaunchOptions
{
private:
    std::istream *mStream;
public:
    std::string mServerIP;
    std::string mUserData;
    LogLevel mLogLevel;
    bool mWindowed;
    bool mBtnRemap;
    bool mTestLatency;
    bool mLogQosStats;
    uint32_t mMaxRes;

    LaunchOptions() :
            mStream(nullptr),
            mServerIP{""},
            mLogLevel(LogLevel_Standard),
            mWindowed(false),
            mBtnRemap(true),
            mTestLatency(false),
            mLogQosStats(false),
            mMaxRes(0)
    { }

    // fast constructor for actual cmdline OSes
    LaunchOptions(int argc, char **argv) : LaunchOptions()
    {
        ParseArgs(argc, argv);
    }

    void ParseArgs(int argc, char **argv)
    {
        if (argc > 1) {
            std::stringstream ss;
            for (int i=1; i<argc; i++) ss << argv[i] << " ";
            mStream = &ss;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseFile(const char* path)
    {
        std::ifstream clifs(path);
        if (!clifs.fail()) {
            mStream = &clifs;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseString(std::string cmdline)
    {
        if (!cmdline.empty()) {
            std::istringstream inss(cmdline);
            mStream = &inss;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseStream()
    {
        // early exit if null stream for some reason.
        if (mStream == nullptr) return;

        // make sure stream is skipping all whitespace!
        (*mStream) >> std::skipws;

        // loop over tokens, handle each, until run out.
        std::string tok;
        while (GetNextToken(tok)) {
            // check for minus prefix for options, drop on floor and loop if none.
            if (tok[0] != '-') continue;

            // lowercase the token to eliminate case testing issues for option names.
            // TODO: might need to use ICU or local routines to handle UTF8
            // need to use lambda as internal type is char, and android to* is int.
            std::transform(tok.begin(), tok.end(), tok.begin(),
                           [](unsigned char c){ return ::tolower(c); } );

            // handle the option argument.
            HandleArg(tok);
        }
    }

protected:
    bool GetNextToken(std::string &token)
    {
        // clear incoming, so at EOF, output is definitely empty.
        token.clear();

        // read next token with stream op, skips all whitespace.
        (*mStream) >> token;

        // return true if token isn't empty...
        return !token.empty();
    }

    // this method may be overridden by a subclass that wants to handle EXTRA arguments.
    virtual void HandleArg(std::string &tok)
    {
        if (tok == "-s" || tok == "-server") { // grab server ip address as next token
            GetNextToken(tok);
            mServerIP = tok; // TODO validate the input is an ip address
        }
        else if (tok == "-u" || tok == "-user-data") {
            GetNextToken(tok);
            mUserData = tok;
        }
        else if (tok == "-v" || tok == "-verbose") { // set logging to verbose
            // only set if higher valued.. TODO remove if tracing split to sep flag.
            if (mLogLevel < LogLevel_Verbose)
                mLogLevel = LogLevel_Verbose;
        }
        else if (tok == "-w" || tok == "-windowed") { // flag windowed mode
            mWindowed = true;
        }
        else if (tok == "-n" || tok == "-no-button-remap") {
            mBtnRemap = false;
        }
        else if (tok == "-m" || tok == "-max-stream-res") {
            GetNextToken(tok);
            uint32_t max = std::stoul(tok);
            if (max >= 512 && max <= 4096)
                mMaxRes = max;
        }
        else if (tok == "-l" || tok == "-latency") {
            mTestLatency = true;
        }
        else if (tok == "-q" || tok == "-qos-stats")
        {
            mLogQosStats = true;
        }
    }
};

}; // namespace CloudXR

#endif // LAUNCHOPTIONS_H
