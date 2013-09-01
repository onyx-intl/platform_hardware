
/* Copyright (C) 2012 Freescale Semiconductor, Inc. */

#include <hardware/XmlTool.h>

namespace android {


void XmlTool::handleStartElement(const XML_Char *name, const XML_Char **attrs)
{
    if(name == NULL) return;

    if(!strcmp(name, "string")) {
        mIsString = true;

        if(mPrint) {
            free(mPrint);
            mPrint = NULL;
        }

        if(attrs[1] != NULL)
            mPrint = strdup(attrs[1]);
    }
    else if(!strcmp(name, "boolean") || !strcmp(name, "int") ||
            !strcmp(name, "long") || !strcmp(name, "float")) {
        if(attrs[1] == NULL || attrs[3] == NULL){
            return;
        }

        String8 name(attrs[1]);
        String8 value(attrs[3]);
        mContent.insert(name, value);
    }
}

void XmlTool::handleEndElement(const XML_Char *name)
{
    if(mPrint) {
        free(mPrint);
        mPrint = NULL;
    }
    mIsString = false;
}

void XmlTool::handleDataElement(const XML_Char *s, int len)
{
    if(!mIsString) return;

    if(s == NULL || mPrint == NULL) {
        return;
    }

    String8 value(s, len);
    String8 name(mPrint);
    mContent.insert(name, value);
}

void XmlTool::startElementHandler(void *userData, const XML_Char *name, const XML_Char **atts)
{
    XmlTool* pXmlTool = (XmlTool*)userData;
    return pXmlTool->handleStartElement(name, atts);
}

void XmlTool::endElementHandler(void *userData, const XML_Char *name)
{
    XmlTool* pXmlTool = (XmlTool*)userData;
    return pXmlTool->handleEndElement(name);
}

void XmlTool::characterDataHandler(void *userData, const XML_Char *s, int len)
{
    XmlTool* pXmlTool = (XmlTool*)userData;
    return pXmlTool->handleDataElement(s, len);
}

XmlTool::XmlTool(const char* file)
    : mLoaded(false), mLock(), mParser(NULL),
      mBuffer(NULL), mDepth(0),
      mFileName(file), mFileHandle(NULL), mContent()
{
    LOGI("XmlTool()");
    init();
    loadAndParseFile();
}

void XmlTool::init()
{
    if(mFileName == NULL) {
        LOGE("invalide file name");
        return;
    }

    mFileHandle = fopen(mFileName, "r");
    if(mFileHandle == NULL) {
        LOGE("open %s failed", mFileName);
        return;
    }

    mParser = XML_ParserCreate(NULL);
    if(mParser == NULL)  {
        LOGE("create parser failed");
        return;
    }

    XML_SetElementHandler(mParser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(mParser, characterDataHandler);
    XML_SetUserData(mParser, (void*)this);
}

void XmlTool::loadAndParseFile()
{
    Mutex::Autolock _l(mLock);
    if(mFileHandle == NULL || mParser == NULL){
        LOGE("invalidate parameter in loadAndParseFile");
        return;
    }

    mBuffer = (char*)malloc(BUFFSIZE);
    if(mBuffer == NULL) {
        LOGE("malloc buffer failed");
        return;
    }

    mPrint = NULL;
    mIsString = false;

    while(1) {
        int done;
        int len;

        len = fread(mBuffer, 1, BUFFSIZE, mFileHandle);
        if(ferror(mFileHandle)) {
            LOGE("read file error");
            return;
        }

        done = feof(mFileHandle);
        if(!XML_Parse(mParser, mBuffer, len, done)) {
            LOGE("Parse error at line %d:/n%s", (int)XML_GetCurrentLineNumber(mParser),
                         XML_ErrorString(XML_GetErrorCode(mParser)));
            return;
        }

        if(done) break;
    }

    free(mBuffer);
    mBuffer = NULL;
    fclose(mFileHandle);
    mFileHandle = NULL;
    XML_ParserFree(mParser);
    mParser = NULL;
}

String8 XmlTool::getString(const char* key, String8 defaultVal)
{
    const String8 nullValue;
    String8 name(key);
    String8 value = mContent.find(name);
    if(value != nullValue) {
        return value;//.string();
    }
    else {
        return defaultVal;
    }
}

int XmlTool::getInt(const char* key, int defaultVal)
{
    const String8 nullValue;
    String8 name(key);
    String8 value = mContent.find(name);
    if(value != nullValue) {
        int nvalue = atoi(value.string());
        return nvalue;
    }
    else {
        return defaultVal;
    }
}

int XmlTool::getHex(const char* key, int defaultVal)
{
    const String8 nullValue;
    String8 name(key);
    String8 value = mContent.find(name);
    if(value != nullValue) {
        int nvalue = (int)strtoimax(value.string(), NULL, 16);
        return nvalue;
    }
    else {
        return defaultVal;
    }
}

bool XmlTool::getBool(const char* key, bool defaultVal)
{
    const String8 nullValue;
    String8 name(key);
    String8 value = mContent.find(name);
    if(value != nullValue) {
        bool bvalue = 0;
        if(!strcmp(value.string(), "true")) bvalue = 1;
        if(!strcmp(value.string(), "fale")) bvalue = 0;

        return bvalue;
    }
    else {
        return defaultVal;
    }
}


};
