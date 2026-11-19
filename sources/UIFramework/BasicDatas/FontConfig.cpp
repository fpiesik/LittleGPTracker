#include "FontConfig.h"
#include "System/Console/Trace.h"
#include "Externals/TinyXML/tinyxml.h"
#include "Application/Model/Config.h"

unsigned char font[8*8*128]= {
	#include "Resources/font.h"
};

const unsigned char embeddedFontBank[FONT_TYPE_COUNT][8*8*128]= {
	{//0 original
		#include "Resources/original.txt"
	},
	{//1 digital
		#include "Resources/digital.txt"
	},
	{//2 monster
		#include "Resources/monster.txt"
	}
};

#if defined(PLATFORM_X64)
#include <fcntl.h>
#include <unistd.h>

namespace {
void RestoreFontSelection();
}
#endif

void FontConfig(){
    const char *fontType=Config::GetInstance()->GetValue("FONTTYPE") ;
	if (fontType)
	{
		if(strcmp(fontType, "CUSTOM") == 0 || strcmp(fontType, "custom") == 0){
			Path path("bin:custom_font.xml") ;
			TiXmlDocument document(path.GetPath());
			bool loadOkay = document.LoadFile();
			if (!loadOkay) {
				Trace::Log("CUSTOM_FONT","No (bad?) custom_font file") ;
			}
			else{
				TiXmlNode* rootnode = 0;
				rootnode = document.FirstChild( "FONT" );
				if (!rootnode)  {
					Trace::Error("No master node for custom_font") ;
				}
				else{
					TiXmlElement *rootelement = rootnode->ToElement();
					TiXmlNode *node = rootelement->FirstChildElement() ;
					if (node) {
						TiXmlElement *element = node->ToElement();
						while (element) {
							const char *elem=element->Value() ; // sould be DATA but we don't really care
							const char *font_data=element->Attribute("value") ;
							if (font_data) {
								int i = 0, j = 0;
								while (font_data[i] != '\0') {
									if (font_data[i] == '1') {
										font[j++] = 1;
									} else if (font_data[i] == '0') {
										font[j++] = 0;
									}
									i++;
								}
							}
							element = element->NextSiblingElement();
						}
					}
				}
			}
		}
		else if(0 <= atoi(fontType) && atoi(fontType) < FONT_TYPE_COUNT){
			for(int i=0;i<8*8*(FONT_COUNT+1);i++)
			{
				font[i]=embeddedFontBank[atoi(fontType)][i];
			}
        }
    }
#if defined(PLATFORM_X64)
    RestoreFontSelection();
#endif
}

#if defined(PLATFORM_X64)
namespace {
int activeFont = -1;
unsigned char customFont[sizeof(font)];
bool hasCustomFont = false;

void IdentifyFont() {
    if (activeFont >= 0)
        return;
    for (int i = 0; i < FONT_TYPE_COUNT; ++i) {
        if (!memcmp(font, embeddedFontBank[i], sizeof(font))) {
            activeFont = i;
            return;
        }
    }
    memcpy(customFont, font, sizeof(font));
    hasCustomFont = true;
    activeFont = FONT_TYPE_COUNT;
}

void RestoreFontSelection() {
    IdentifyFont(); // Keep the configured startup font available in the menu.
    Path path("bin:font_selection");
    int fd = open(path.GetPath().c_str(), O_RDONLY);
    if (fd < 0)
        return;
    char bytes[2];
    ssize_t length = read(fd, bytes, sizeof(bytes));
    close(fd);
    if (length != 1)
        return;
    int index = bytes[0] - '0';
    if (index >= 0 && index < FONT_TYPE_COUNT) {
        activeFont = index;
        memcpy(font, embeddedFontBank[index], sizeof(font));
    }
}

bool SaveFontSelection(int index) {
    Path path("bin:font_selection");
    std::string name = path.GetPath();
    std::string temp = name + ".XXXXXX";
    int fd = mkstemp(&temp[0]);
    if (fd < 0)
        return false;
    char digit = '0' + index;
    bool written = write(fd, &digit, 1) == 1;
    if (close(fd) == 0 && written && rename(temp.c_str(), name.c_str()) == 0)
        return true;
    unlink(temp.c_str());
    return false;
}
} // namespace

const char *CurrentFontLabel() {
    IdentifyFont();
    static const char *labels[] = {"Font: Original", "Font: Digital",
                                   "Font: Monster", "Font: Custom"};
    return labels[activeFont];
}

bool CycleFont(int direction) {
    IdentifyFont();
    int count = FONT_TYPE_COUNT + hasCustomFont;
    int next = (activeFont + direction + count) % count;
    if (!SaveFontSelection(next))
        return false;
    activeFont = next;
    memcpy(font,
           activeFont == FONT_TYPE_COUNT ? customFont
                                         : embeddedFontBank[activeFont],
           sizeof(font));
    return true;
}
#endif
