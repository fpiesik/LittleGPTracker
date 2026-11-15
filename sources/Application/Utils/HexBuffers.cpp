
#include "HexBuffers.h"
#include "Application/Utils/char.h"
#include <cctype>
#include <vector>

#define XML_CUT_LENGTH 64

void prepareHexChunk(TiXmlElement &d,unsigned char *datasrc,int len) {

	bool singleValue=true ;
	int singleValueData=-1 ;
	unsigned char hexBuffer[XML_CUT_LENGTH*2+1] ;
	std::string buffer;

	char *hex=(char *)hexBuffer ;
	for (int i=0;i<len;i++) {
		hex2char(*datasrc,hex) ;
		if (singleValueData==-1) {
			singleValueData=*datasrc ;
		} else {
			if (singleValueData!=*datasrc) {
				singleValue=false ;
			}
		};
		datasrc++ ;
		hex+=2 ;
	} ;
	buffer+=(const char *)hexBuffer ;
	if (singleValue) {
		d.SetAttribute("VALUE",singleValueData) ;
		d.SetAttribute("LENGTH",len) ;
	} else {
		TiXmlText text(buffer.c_str()) ;
		d.InsertEndChild(text) ;
	}

}

void saveHexBuffer(TiXmlNode *parent,const char *nodeName,unsigned char *src,unsigned len) {

	TiXmlElement data(nodeName) ;
	TiXmlNode *dataNode=parent->InsertEndChild(data) ;

	unsigned int count=len/XML_CUT_LENGTH ;
	unsigned char *datasrc=(unsigned char *)src ;

	for (unsigned i=0;i<count;i++) {
		TiXmlElement d("DATA") ;
		prepareHexChunk(d,datasrc,XML_CUT_LENGTH) ;
		datasrc+=XML_CUT_LENGTH ;
		dataNode->InsertEndChild(d) ;
	} ;

	len-=count*XML_CUT_LENGTH ;
	if (len>0) {
		TiXmlElement d("DATA") ;
		prepareHexChunk(d,datasrc,len) ;
		dataNode->InsertEndChild(d) ;
	}
};


void saveHexBuffer(TiXmlNode *parent,const char *nodeName,unsigned int *src,unsigned len) {
    saveHexBuffer(parent,nodeName,(unsigned char *)src,len*sizeof(int));
}

void saveHexBuffer(TiXmlNode *parent,const char *nodeName,unsigned short *src,unsigned len) {
    saveHexBuffer(parent,nodeName,(unsigned char *)src,len*sizeof(short));
}

void restoreHexBuffer(TiXmlNode *node,unsigned char *destination) {

	unsigned char *dst=destination ;
	TiXmlElement *current=node->FirstChildElement() ;
	// all datas
	while(current) {
		int value ;
		const char* s=current->Attribute("VALUE",&value) ;
		if (s) {
			int length ;
			current->Attribute("LENGTH",&length) ;
			unsigned char data=value ;
			memset(dst,data,length) ;
			dst+=length ;
		} else {
			TiXmlNode *textNode=current->FirstChild() ;
			if (textNode) {
				textNode->ToText();
				const char *buffer=textNode->Value() ;
				for (unsigned int i=0;i<strlen(buffer)/2;i++) {
					const char *src__=buffer+i*2 ;
					unsigned char b1=(c2h__(src__[0]))<<4 ;
					unsigned char b2=c2h__(src__[1]) ;
					*dst++=b1+b2 ;
	//				char2hex(buffer+i*2,dst++) ;
				} ;
			}
		}
		current=current->NextSiblingElement() ;

	} ;
} ;

int restoreHexBuffer(TiXmlNode *node, unsigned char *dst, unsigned capacity) {
    // Validate into temporary storage so invalid input cannot partially
    // restore.
    std::vector<unsigned char> decoded;
    for (TiXmlElement *part = node->FirstChildElement(); part;
         part = part->NextSiblingElement()) {
        if (part->Attribute("VALUE")) {
            int value, length;
            if (part->QueryIntAttribute("VALUE", &value) != TIXML_SUCCESS ||
                part->QueryIntAttribute("LENGTH", &length) != TIXML_SUCCESS ||
                value < 0 || value > 255 || length < 0 ||
                static_cast<unsigned>(length) > capacity - decoded.size())
                return -1;
            decoded.insert(decoded.end(), length, value);
        } else {
            TiXmlNode *text = part->FirstChild();
            if (!text || text->Type() != TiXmlNode::TEXT)
                return -1;
            const char *hex = text->Value();
            size_t length = strlen(hex);
            if (length % 2 || length / 2 > capacity - decoded.size())
                return -1;
            for (size_t i = 0; i < length; i += 2) {
                if (!isxdigit(static_cast<unsigned char>(hex[i])) ||
                    !isxdigit(static_cast<unsigned char>(hex[i + 1])))
                    return -1;
                decoded.push_back((c2h__(hex[i]) << 4) | c2h__(hex[i + 1]));
            }
        }
    }
    if (!decoded.empty())
        memcpy(dst, &decoded[0], decoded.size());
    return decoded.size();
}
