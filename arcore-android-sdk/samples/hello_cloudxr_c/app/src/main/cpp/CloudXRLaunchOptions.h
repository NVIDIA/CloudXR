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
    LogLevel mLogLevel = LogLevel_Standard;
    bool mWindowed;
    bool mBtnRemap;

    LaunchOptions() :
            mStream(nullptr),
            mServerIP{""},
            mLogLevel(LogLevel_Standard),
            mWindowed(false),
            mBtnRemap(true)
    { }

    // fast constructor for actual cmdline OSes
    LaunchOptions(int argc, char **argv) : LaunchOptions() {
        ParseArgs(argc, argv);
    }

    void ParseArgs(int argc, char **argv) {
        if (argc > 1) {
            std::stringstream ss;
            for (int i=1; i<argc; i++) ss << argv[i] << " ";
            mStream = &ss;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseFile(const char* path) {
        std::ifstream clifs(path);
        if (!clifs.fail()) {
            mStream = &clifs;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseString(std::string cmdline) {
        if (!cmdline.empty()) {
            std::istringstream inss(cmdline);
            mStream = &inss;
            ParseStream();
            mStream = nullptr;
        }
    }

    void ParseStream() {
        // early exit if null stream for some reason.
        if (mStream == nullptr) return;

        // make sure stream is skipping all whitespace!
        (*mStream) >> std::skipws;

        // loop over tokens, handle each, until run out.
        std::string tok;
        while (GetNextToken(tok)) {
            HandleArg(tok);
        }
    }

protected:
    void Init() {
        mServerIP = "";
        mLogLevel = LogLevel_Standard;
        mWindowed = false;
        mBtnRemap = true;
    }

    bool GetNextToken(std::string &token)
    {
        // clear incoming, so at EOF, output is definitely empty.
        token.clear();

        // read next token with stream op, skips all whitespace.
        (*mStream) >> token;

        // return true if not yet reached EOF
        return !mStream->eof();
    }

    // this method may be overridden by a subclass that wants to handle EXTRA arguments.
    virtual void HandleArg(std::string &tok)
    {
        if (tok == "-s" || tok == "-server") { // grab server ip address as next token
            GetNextToken(tok);
            mServerIP = tok; // TODO validate the input is an ip address
        }
        else if (tok == "-v" || tok == "-verbose") { // set logging to verbose
            // only set if higher valued.. TODO remove if tracing split to sep flag.
            if (mLogLevel < LogLevel_Verbose)
                mLogLevel = LogLevel_Verbose;
        }
        else if (tok == "-w" || tok == "-windowed") { // flag windowed mode
            mWindowed = true;
        }
        else if (tok == "-n" || tok == "-noBtnRemap") {
            mBtnRemap = false;
        }
    }
};

}; // namespace CloudXR

#endif // LAUNCHOPTIONS_H
