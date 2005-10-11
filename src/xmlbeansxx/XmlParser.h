/*
    Copyright 2004-2005 TouK s.c.
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0
 
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License. */


#ifndef _XMLBEANSXX_XMLPARSER_H_
#define _XMLBEANSXX_XMLPARSER_H_

#include "BoostAssert.h"
#include "String.h"
#include <string>
#include <vector>
#include "XmlOptions.h"
#include "String.h"

namespace xmlbeansxx {

class XmlObject;

/** This class is used for parsing of xml documents. It uses some xml parser and optionally can use xml schema validator.  */
class XmlParser_I {
public:
    virtual ~XmlParser_I() {}

    /**
     * Parses using some parser an xml document from std::istream to some XmlDocument. 
     * If XmlOptions validation is set, then uses schema validator
     * (apropriate grammars should be loaded using eg. loadGrammar method).
     */
    virtual void parse(std::istream &in, const XmlObject &documentRoot) = 0;
    virtual void parse(const String &in, const XmlObject &documentRoot) = 0;

    virtual XmlOptions getXmlOptions() const = 0;
    virtual void setXmlOptions(const XmlOptions &options) = 0;

    /** Loads grammars into parser from specified file names. */
    virtual void loadGrammars(const std::vector<String> &fileNames) = 0;
    /** Loads grammar into parser from specified file name. */
    virtual void loadGrammar(const String &fileName) = 0;
    /** Unloads all grammars from parser. */
    virtual void unloadGrammars() = 0;
};

BEGIN_CLASS(XmlParser, XmlParser_I)
    static XmlParser create();
    static XmlParser create(const XmlOptions &opts);
END_CLASS()
}

#endif//_XMLBEANSXX_XMLPARSER_H_
