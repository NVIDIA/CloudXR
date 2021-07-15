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

#ifndef CLOUDXR_OPTIONS_PARSER_H
#define CLOUDXR_OPTIONS_PARSER_H

#include <string>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <vector>

#include "CloudXRClient.h"

typedef enum
{
    ParseStatus_Success,
    ParseStatus_Fail,
    ParseStatus_ExitRequested,
    ParseStatus_BadVal,
    ParseStatus_FileNotFound
} ParseStatus;

namespace CloudXR {

struct OptionHandler
{
    bool valueRequired; // true if options needs a value, of the form -opt val
    bool longOpt;
    std::string helpText;
    std::function<ParseStatus(std::string key)> handler;
};

#define HANDLER_LAMBDA_FN [this](std::string tok) -> ParseStatus

class OptionsParser
{
private:
    std::istream *mStream;

protected:
    std::unordered_map<std::string,OptionHandler> args;

public:

    OptionsParser() :
            mStream(nullptr)
    {
        AddOption("help", "h", false, "display this help and exit",
            HANDLER_LAMBDA_FN
            {
                for (std::pair<std::string, OptionHandler> element: args)
                {
                    if (!element.second.longOpt) continue;
                    std::cout << element.second.helpText << '\n';
                }
                return ParseStatus_ExitRequested;
            });
    }

    // fast constructor for actual cmdline OSes
    OptionsParser(int argc, char **argv) : OptionsParser()
    {
        ParseArgs(argc, argv);
    }

    void AddOption(const std::string &longOpt, const std::string &shortOpt, bool valueRequired,
                   const std::string &helpText, std::function<ParseStatus(std::string key)> handler)
    {
        std::string help = "-" + longOpt;
        if(!shortOpt.empty())
        {
            help += ", -" + shortOpt;
        }

        help += " : ";
        help += helpText;
        args.insert({longOpt, {valueRequired, true, help, handler}});

        if(!shortOpt.empty())
        {
            args.insert({shortOpt, {valueRequired, false, "", handler}});
        }
    }

    ParseStatus ParseArgs(int argc, char **argv)
    {
        ParseStatus status = ParseStatus_Success;
        if (argc > 1)
        {
            std::stringstream ss;
            for (int i=1; i<argc; i++) ss << argv[i] << " ";
            mStream = &ss;
            status = ParseStream();
            mStream = nullptr;
        }
        return status;
    }

    ParseStatus ParseFile(const char* path)
    {
        ParseStatus status = ParseStatus_FileNotFound;
        std::ifstream clifs(path);
        if (!clifs.fail()) {
            mStream = &clifs;
            status = ParseStream();
            mStream = nullptr;
        }
        return status;
    }

    ParseStatus ParseString(std::string cmdline)
    {
        ParseStatus status = ParseStatus_Success;
        if (!cmdline.empty()) {
            std::istringstream inss(cmdline);
            mStream = &inss;
            status = ParseStream();
            mStream = nullptr;
        }
        return status;
    }

    ParseStatus ParseStream()
    {
        // early exit if null stream for some reason.
        if (mStream == nullptr) return ParseStatus_Fail;

        // make sure stream is skipping all whitespace!
        (*mStream) >> std::skipws;

        // loop over tokens, handle each, until run out.
        std::string tok;
        std::string key;
        while (GetNextToken(tok)) {

            if (tok[0] == '-')
            {
                tok.erase(tok.begin());
            }
            if (args.find(tok) == args.end())
            {
                std::cout << "Unknown argument " << tok << '\n';
                //return ParseStatus_Fail;
                // instead of failing, let's continue so bad params don't abort processing.
                continue;
            }

            // lowercase the token to eliminate case testing issues for option names.
            // TODO: might need to use ICU or local routines to handle UTF8
            // need to use lambda as internal type is char, and android to* is int.
            std::transform(tok.begin(), tok.end(), tok.begin(),
                           [](unsigned char c){ return ::tolower(c); } );
            
            // handle the option argument.
            if (args[tok].valueRequired && !GetNextToken(key))
            {
                std::cout << "Unable to read value for cmd option: " << tok << '\n';
                continue;
            }

            ParseStatus status = args[tok].handler(key);
            switch (status)
            {
                case ParseStatus_ExitRequested:
                    return status;
                    break;
                case ParseStatus_BadVal:
                case ParseStatus_Fail:
                case ParseStatus_Success:
                default:
                    break;
            }
        }
        return ParseStatus_Success;
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
};

}; // namespace CloudXR

#endif // CLOUDXR_OPTIONS_PARSER_H
