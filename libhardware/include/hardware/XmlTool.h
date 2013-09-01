
/* Copyright (C) 2012 Freescale Semiconductor, Inc. */

#ifndef _SYSTEM_XMLTOOL_H
#define _SYSTEM_XMLTOOL_H

#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#define LOG_TAG "XMLTOOL"
#include <cutils/log.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include "Map.h"
#include <inttypes.h>

#define BUFFSIZE 8192 

namespace android {

class XmlTool {
public:
    XmlTool(const char* file);
    ~XmlTool() {}

    int getInt(const char* key, int defaultVal);
    int getHex(const char* key, int defaultVal);
    bool getBool(const char* key, bool defaultVal);
    String8 getString(const char* key, String8 defaultVal);

private:
    void init();
    void loadAndParseFile();
    //void doParser();
    void waitForLoadComplete();

    void handleStartElement(const XML_Char *name, const XML_Char **atts);
    void handleEndElement(const XML_Char *name);
    void handleDataElement(const XML_Char *s, int len);

private:
    static void startElementHandler(void *userData, const XML_Char *name, const XML_Char **attrs);
    static void endElementHandler(void *userData, const XML_Char *name);
    static void characterDataHandler(void *userData, const XML_Char *s, int len);

    bool mLoaded;
    Mutex mLock;

    XML_Parser mParser;
    char* mBuffer;
    int mDepth;
    const char* mFileName;
    FILE* mFileHandle;
    Map<String8, String8> mContent;

    char* mPrint;
    int mIsString;
};

}; // namespace android

#endif
