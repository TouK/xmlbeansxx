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


#include "Contents.h"
#include "Tracer.h"
#include "defs.h"
#include "XmlTypesGen.h"
#include <string>
#include <iostream>
#include <set>
#include <map>
#include "TypeSystem.h"
#include "TextUtils.h"
#include "SchemaProperty.h"
using namespace std;

//------------------------------------------------------------------------------------

#include <boost/config.hpp>
#ifdef BOOST_HAS_THREADS
#include <boost/thread/detail/lock.hpp>
#define SYNC(mutex) boost::detail::thread::scoped_lock<boost::recursive_mutex> lock(mutex);
#else
#define SYNC(mutex)
#endif


/*
//Disable XmlObject synchronization
#undef SYNC
#define SYNC(mutex)
*/

//Disable tracing
#undef TRACER
#define TRACER(logger, msg)

namespace xmlbeansxx {

log4cxx::LoggerPtr Contents::log = log4cxx::Logger::getLogger(std::string("xmlbeansxx.Contents"));

//Contents Impl

Contents::Contents() { }

Contents::~Contents() {
    free();
}

Contents::Contents(const Contents &orig) {
    simpleCopyFrom(orig);
}
	
void Contents::setSimpleContent(const std::string &c) {
    SYNC(mutex)
    value=c;
}

std::string Contents::getSimpleContent() const {
    SYNC(mutex)
    return value;
}

bool Contents::hasEmptyContent() const {
    return attrs.hasEmptyContent() && elems.hasEmptyContent();
}

void Contents::simpleCopyFrom(const Contents &orig) {
    TRACER(log,std::string("simpleCopyFrom"))
    SYNC(mutex)
/*
#ifdef BOOST_HAS_THREADS
    boost::detail::thread::scoped_lock<boost::recursive_mutex> lock2(orig.mutex);
#endif
*/
    FOREACH(it,orig.elems.contents) {
        elems.add(it->name,it->value);
    }

    FOREACH(it,orig.attrs.contents) {
        attrs.add(it->name,it->value);
    }
}

void Contents::copyFrom(const Contents &obj) {
    SYNC(mutex)
/*
#ifdef BOOST_HAS_THREADS
    boost::detail::thread::scoped_lock<boost::recursive_mutex> lock2(obj.mutex);
#endif
*/
    FOREACH(it,obj.elems.contents) {
        XmlObjectPtr o=it->value;
        if (o!=NULL)
            o=o->clone();
        elems.add(it->name,o);
    }

    FOREACH(it,obj.attrs.contents) {
        attrs.add(it->name,it->value);
    }
}

bool Contents::hasElements() const {
    SYNC(mutex)
    return elems.size()>0;
}

void Contents::serializeAttrs(ostream &o) const {
    SYNC(mutex)
    FOREACH(it,attrs.contents) {
        //if (it->value!=NULL) {
        o<<" "<<it->name<<"='"<<TextUtils::exchangeEntities(it->value)<<"'";
        //}
    }
}

namespace {
template<class T1,class T2,class T3>
struct three {
    T1 first; T2 second; T3 third;
    three(const T1 &a,const T2 &b,const T3 &c): first(a), second(b),third(c) {}
    bool operator <(const three<T1,T2,T3> &other) const {
        if (first==other.first) return second<other.second;
        else return first<other.first;
    }
};
    
bool shallPrintXsiType(const SchemaType *xsdDefined,const XmlObjectPtr &obj) {
    static XmlObjectPtr src=XmlObject::Factory::newInstance();
    return obj->getSchemaType()->classTypeInfo != src->getSchemaType()->classTypeInfo 
        && obj->getSchemaType()->classTypeInfo != xsdDefined->classTypeInfo;
}
}

void Contents::serializeElems(int emptyNsID,ostream &o,const std::map<std::string,SchemaPropertyPtr> *order) const {
    SYNC(mutex)
    TRACER(log,"serializeElems")
    //logger.debug("Contents::serializeElems - order=%p, elements:",order);
    //--
    if (order!=NULL) {
        //sorting
        set<three<int,int,SchemaProperty *> > s;
        int i=0;
        /*
        FOREACH(it,*order) {
        logger.debug("Contents::serializeElems - %s->%i",it->first.c_str(),it->second);
        }
        logger.debug("Contents::serializeElems ---");
         */

        FOREACH(it,elems.contents) {
            std::map<std::string,SchemaPropertyPtr>::const_iterator it2=(*order).find(it->name);
            if (it2!=(*order).end()) {
                s.insert(three<int,int,SchemaProperty *>(it2->second->order,i,it2->second.get()));
            } else {
                s.insert(three<int,int,SchemaProperty *>(-1,i,NULL));
            }
            i++;
        }

        /*
        FOREACH(it,s) {
        logger.debug("Contents::serializeElems - pair<%i,%i> name:%s",it->first,it->second,elems.contents[it->second].name);
        }
        logger.debug("Contents::serializeElems ---");
         */

        FOREACH(it,s) {
            XmlObjectPtr value(elems.contents[it->second].value);
            if (value!=NULL) {
                bool printXsiType= it->third==NULL || shallPrintXsiType(it->third->schemaType,value);
                value->contents.serialize(elems.contents[it->second].name,o,value.get(),emptyNsID,printXsiType);
            }
        }
    } else {
        LOG4CXX_DEBUG(log,"Elements order is NULL");
        throw IllegalStateException();
        /*
        FOREACH(it,elems.contents) {
            if (it->value!=NULL) {
                it->value->contents.serialize(it->name,o,it->value.get(),emptyNsID);
            }
        }*/
    }
}

void Contents::serialize2(int emptyNsID,bool printXsiType,std::string elemName,ostream &o,const XmlObject *obj) const {
    TRACER(log,"serialize2")
    SYNC(mutex)

    int ans=obj->getSchemaType()->arrayXsdNamespaceID;
    if (ans!=-1) {
        //it's an array
        std::string typeName=obj->getSchemaType()->arrayXsdTypeName;
        std::string def;

        o<<" xsi:array='";
        if (ans==globalTypeSystem()->xs_ns()) {
            o<<"xs:";
        } else if (ans==globalTypeSystem()->xsi_ns()) {
            o<<"xsi:";
        } else if (ans!=emptyNsID) {
            emptyNsID=ans;
            std::string emptyNs=globalTypeSystem()->getNamespaceName(emptyNsID);
            def=std::string(" xmlns='")+globalTypeSystem()->getNamespaceName(emptyNsID)+"'";
        }

        o<<typeName;
        o<<"'";

        o<<def;

    } else {
        //it's an object
        if (printXsiType) {
            int ns=obj->getSchemaType()->xsdNamespaceID;
            if (ns!=-1) {
                std::string typeName=obj->getSchemaType()->xsdTypeName;
                std::string def;

                o<<" xsi:type='";
                if (ns==globalTypeSystem()->xs_ns()) {
                    o<<"xs:";
                } else if (ns==globalTypeSystem()->xsi_ns()) {
                    o<<"xsi:";
                } else if (ns!=emptyNsID) {
                    emptyNsID=ns;
                    std::string emptyNs=globalTypeSystem()->getNamespaceName(emptyNsID);
                    def=std::string(" xmlns='")+globalTypeSystem()->getNamespaceName(emptyNsID)+"'";
                }

                o<<typeName;
                o<<"'";

                o<<def;
            } else {
                //no xsi type printed
                if (printXsiType && obj->documentElementNamespaceID()==-1) {
                    std::string msg=std::string("Inserted element '")+elemName+std::string("' of class: ")+std::string(obj->getSchemaType()->className)+std::string("' is a schema anonymous type and it's type is not equal to schema expected type.");
                    LOG4CXX_DEBUG(log,msg);
                    throw XmlException(msg);
                }
                
            }
        }
    }

    //o<<" xsi:class=\""<<obj->className()<<"\"";
    //
    serializeAttrs(o);

    {
        std::string cnt=obj->exchangeEntities(obj->getSimpleContent());
        if (cnt==std::string() && !obj->contents.hasElements()) {
            o<<"/>";
        } else {
            o<<">";
            o<<cnt;
            serializeElems(emptyNsID,o,&(obj->getSchemaType()->elements));
            o<<"</"<<elemName<<">";
        }
    }
}

void Contents::serialize(std::string elemName,ostream &o,const XmlObject *obj,int emptyNsID,bool printXsiType) const {
    o<<"<"<<elemName;
    serialize2(emptyNsID,printXsiType,elemName,o,obj);
}

std::string Contents::digest(XmlObject *parent) {
    std::string r;
    
    std::vector<std::pair<std::string,std::string> > as=getAttrs();
    std::vector<std::pair<std::string,XmlObjectPtr > > es=getElems();

    sort(as.begin(),as.end());
    FOREACH(it,as) {
        XmlObjectPtr a=getAttrObject2(it->first,it->second,parent);
        r+=" ";
        r+=it->first;
        r+="=\"";
        r+=a->exchangeEntities(a->getCanonicalContent());
        r+="\"";
    }
    r+=">";
    r+=parent->exchangeEntities(parent->getCanonicalContent());
    
    std::map<std::pair<std::string,int>, XmlObjectPtr> m;
    std::map<std::string, int> counters;
    FOREACH(it,es) {
        if (it->second!=NULL) {
            m[std::pair<std::string,int>(it->first,counters[it->first])]=it->second;
            counters[it->first]++;
        }
    }
    FOREACH(it,m) {
        r+="<";
        r+=it->first.first;
        r+=it->second->digest();
        r+="</";
        r+=it->first.first;
        r+=">";
    }
    return r;
}

void Contents::free() {
    //logger->debug(std::string("Contents::free() - start"));
    SYNC(mutex)
    elems.free();
    attrs.free();
    value=std::string();
    //logger->debug(std::string("Contents::free() - finish"));
}

void Contents::serializeDocument(ostream &o,const XmlOptionsPtr &options,const XmlObject *obj) const {
    TRACER(log,"serializeDocument")
    //XmlContextPtr xmlContext(new XmlContext());
    SYNC(mutex)

    if (options->getPrintXmlDeclaration())
        o<<"<?xml version='1.0' encoding='UTF-8'?>\n";

    VAL(it,elems.contents.begin());
    if (it==elems.contents.end())
        return;

    o<<"<"<<it->name;
    LOG4CXX_DEBUG(log,"document root element namespace id: "+TextUtils::intToString(it->value->getSchemaType()->xsdNamespaceID));
    int emptyNsID=obj->documentElementNamespaceID();
    std::string docns=globalTypeSystem()->getNamespaceName(emptyNsID);
    if (docns!="") {
        o<<" xmlns='"<<docns<<"'";
    }
    o<<" xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'";
    o<<" xmlns:xs='http://www.w3.org/2001/XMLSchema'";

    //----------
    const std::map<std::string,SchemaPropertyPtr> *order=&(obj->getSchemaType()->elements);
    std::map<std::string,SchemaPropertyPtr>::const_iterator propIt=(*order).find(it->name);
    SchemaPropertyPtr prop;
    if (propIt!=(*order).end())
        prop=propIt->second;

    if (prop==NULL) {
        std::string msg=std::string("Serializing document of class: ")+std::string(obj->getSchemaType()->className)+std::string(". It's root element name should not be '")+it->name+std::string("'");
        LOG4CXX_DEBUG(log,msg);
        throw XmlException(msg);
    }

    bool printXsiType=shallPrintXsiType(prop->schemaType,it->value);
    //----------
    it->value->contents.serialize2(emptyNsID,printXsiType,it->name,o,it->value.get());
    o<<"\n";
}

StringPtr Contents::getAttr(const DictNameType &elemName) const {
    SYNC(mutex)
    return attrs.find(elemName);
    /*
    const char *v=attrs.find(elemName);
    if (v==NULL)
        return constStringPtr();
    else
        return constStringPtr(new std::string(v));
    */
}

XmlObjectPtr Contents::getAttrObject(const DictNameType &attrName, XmlObject *parent) const {
    SYNC(mutex)
    StringPtr v=attrs.find(attrName);
    if (v==NULL) return XmlObjectPtr();
    else return getAttrObject2(attrName,*v,parent);
}

XmlObjectPtr Contents::getAttrObject2(const DictNameType &attrName, const std::string &attrValue, XmlObject *parent) const {
    XmlObjectPtr r;
    SchemaPropertyPtr prop=parent->getSchemaType()->findAttribute(attrName);
    if (prop!=NULL) {
        r=prop->schemaType->createFn();
    } else {
        r=XmlObject::Factory::newInstance();
    }
    r->setSimpleContent(attrValue);
    return r;
}

void Contents::setAttr(const DictNameType &attrName,const StringPtr &value) {
    SYNC(mutex)
    attrs.del(attrName);
    //cout<<"\n!adding "<<elemName<<"->"<<value<<"\n";
    if (value!=NULL)
        attrs.add(attrName,*value);
}

XmlObjectPtr Contents::getElem(const DictNameType &elemName) const {
    SYNC(mutex)
    return elems.find(elemName,0);
}

XmlObjectPtr Contents::getElemAt(const DictNameType &elemName,int index) const {
    SYNC(mutex)
    return elems.find(elemName,index);
}

XmlObjectPtr Contents::cgetElemAt(const DictNameType &elemName,int index,ObjectCreatorFn createFn,XmlObject *creator) {
    TRACER(log,"cgetElemAt")

    XmlObjectPtr e(getElemAt(elemName,index));
    if (e!=NULL)
        return e;

    if (createFn!=NULL) {
        e=createFn();
        LOG4CXX_DEBUG(log,"CreateFn returned: "+TextUtils::ptrToString(e.get()));
    } else {
        LOG4CXX_DEBUG(log,"creating subObject on creator");
        e=creator->getSchemaType()->createSubObject(elemName);
    }
    setElemAt(elemName,index,e);
    return e;
}

/*
XmlObjectPtr Contents::cgetElemAt(std::string elemName,int index,int namespaceID,std::string typeName,XmlObject *creator) {
    TRACER(logger,"Contents::cgetElemAt()")
    VAL(e,getElemAt(elemName,index));
    if (e!=NULL)
        return e;
    if (namespaceID!=-1) {
        e=globalTypeSystem()->createByName(typeName,namespaceID);
    } else {
        logger->debug("Contents::cgetElemAt() - creating subObject");
        e=creator->createSubObject(elemName);
    }
    setElemAt(elemName,index,e);
    return e;
}*/

void Contents::setElem(const DictNameType &elemName,const XmlObjectPtr &value) {
    SYNC(mutex)
    elems.del(elemName);
    if (value!=NULL)
        elems.add(elemName,value);
}

void Contents::setElemAt(const DictNameType &elemName,int index,const XmlObjectPtr &value) {
    SYNC(mutex)
    elems.set(elemName,index,value);
}

void Contents::removeElemAt(const DictNameType &elemName,int index) {
    SYNC(mutex)
    elems.removeAt(elemName,index);
    
}


void Contents::removeElems(const DictNameType &name) {
    SYNC(mutex)
    elems.del(name);
}

int Contents::countElems(const DictNameType &name) const {
    SYNC(mutex)
    return elems.count(name);
}

vector<pair<string,XmlObjectPtr > > Contents::getElems() const {
    SYNC(mutex)
    vector<pair<string,XmlObjectPtr > > v;
    FOREACH(it,elems.contents) {
        v.push_back(pair<string,XmlObjectPtr >(it->name,it->value));
    }
    return v;
}

vector<pair<string,string> > Contents::getAttrs() const {
    SYNC(mutex)
    vector<pair<string,string> > v;
    FOREACH(it,attrs.contents) {
        v.push_back(pair<string,string>(it->name,it->value));
    }
    return v;
}

void Contents::appendAttr(const DictNameType &name,const std::string &value) {
    SYNC(mutex)
    attrs.add(name,value);
}

void Contents::appendElem(const DictNameType &name,const XmlObjectPtr &value) {
    SYNC(mutex)
    elems.add(name,value);
}

shared_array<XmlObjectPtr > Contents::getElemArray(const DictNameType &elemName) const {
    SYNC(mutex)
    std::vector<XmlObjectPtr > r;
    FOREACH(it,elems.contents) {
        if (elemName==it->name) {
            r.push_back(it->value);
        }
    }
    return toSharedArray(r);
}

void Contents::setElemArray(const DictNameType &elemName,const shared_array<XmlObjectPtr > &value) {
    SYNC(mutex)
    elems.del(elemName);
    for (int i=0;i<value.size();i++) {
        elems.add(elemName,value[i]);
    }
}

void Contents::insertDefaults(const SchemaType *schemaType) {
    SYNC(mutex)
    {
        std::set<std::string> exist;
        FOREACH(it,attrs.contents) {
            exist.insert(it->name);
        }
        FOREACH(it, schemaType->attributes) {
            StringPtr def = it->second->singletonDefault;
            if (def != NULL && exist.count(it->first)==0) {
                appendAttr(it->first, *def);
            }
        }
    }
    {
        std::set<std::string> exist;
        FOREACH(it,elems.contents) {
            exist.insert(it->name);
        }
        FOREACH(it, schemaType->elements) {
            StringPtr def = it->second->singletonDefault;
            if (def != NULL && exist.count(it->first)==0) {
                XmlObjectPtr obj = it->second->schemaType->createFn();
                obj->setSimpleContent(*def);
                appendElem(it->first, obj);
            }
        }
    }
}

}

