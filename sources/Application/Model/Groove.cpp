
#include "Groove.h"

unsigned char Groove::data_[MAX_GROOVES][16] ;

static unsigned char GrooveTicks(unsigned char value) {
    // Preserve the 255-tick interval when the first step is empty.
    return value == NO_GROOVE_DATA ? 0xFF : value;
}

Groove::Groove():Persistent("GROOVES") {
	Clear() ;
} ;

Groove::~Groove() {
} ;

void Groove::Clear() {
	// Init all grooves with basic datas
    memset(data_, NO_GROOVE_DATA, sizeof(data_));
    for (int i=0;i<MAX_GROOVES;i++) {
		data_[i][0]=6 ;
		data_[i][1]=6 ;
	} ;
	// init grooves selectah
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		// Channel
		channelGroove_[i].groove_=0 ;
		channelGroove_[i].position_=0 ;
		channelGroove_[i].ticks_=data_[0][0] ;
	} ;
} ;
// Resest groove data at song startup

void Groove::Reset() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		//Channel
		ChannelGroove &c=channelGroove_[i] ;
		c.position_=0 ;
        c.ticks_ = GrooveTicks(data_[c.groove_][c.position_]);
    }
} ;

void Groove::GetChannelData(int channel,int *groove,int *position) {
		ChannelGroove &c=channelGroove_[channel] ;
		*groove=c.groove_ ;
		*position=c.position_ ;
} ;

void Groove::SaveContent(TiXmlNode *node) {
    unsigned char legacy[MAX_GROOVES * 16];
    unsigned char fullTicks[MAX_GROOVES * 2] = {};
    bool hasFullTicks = false;
    for (unsigned i = 0; i < sizeof(legacy); ++i) {
        unsigned char value = data_[i / 16][i % 16];
        legacy[i] = value == NO_GROOVE_DATA ? 0xFF : value;
        if (value == 0xFF) {
            // Older builds play FE ticks here.
            legacy[i] = 0xFE;
            fullTicks[i / 8] |= 1 << (i % 8);
            hasFullTicks = true;
        }
    }
    saveHexBuffer(node, "DATA", legacy, sizeof(legacy));
    if (hasFullTicks)
        saveHexBuffer(node, "FF_STEPS", fullTicks, sizeof(fullTicks));
}

void Groove::RestoreContent(TiXmlElement *element) {
    unsigned char legacy[MAX_GROOVES * 16];
    TiXmlElement *data = element->FirstChildElement("DATA");
    if (!data)
        return;
    int count = restoreHexBuffer(data, legacy, sizeof(legacy));
    if (count < 0)
        return;

    unsigned char fullTicks[MAX_GROOVES * 2];
    TiXmlElement *ffSteps = element->FirstChildElement("FF_STEPS");
    bool hasFullTicks =
        ffSteps && restoreHexBuffer(ffSteps, fullTicks, sizeof(fullTicks)) ==
                       sizeof(fullTicks);
    for (int i = 0; i < count; ++i) {
        if (hasFullTicks && (fullTicks[i / 8] & (1 << (i % 8))))
            data_[i / 16][i % 16] = 0xFF;
        else
            data_[i / 16][i % 16] =
                legacy[i] == 0xFF ? NO_GROOVE_DATA : legacy[i];
    }
}
// Trigger grooves so we go to the next step

void Groove::Trigger() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		ChannelGroove &c=channelGroove_[i] ;
		UpdateGroove(c,false) ;
	}
} ;

bool Groove::UpdateGroove(ChannelGroove &c,bool reverse) {

	bool stepped=false ;

	if (reverse) { //Table
		c.ticks_++ ;
		if (c.groove_==255) { // Default table groove
			if (c.ticks_==1) {
				stepped=true  ;
				c.ticks_=0 ;
			}
		} else {
            if (c.ticks_ == GrooveTicks(data_[c.groove_][c.position_])) {
                c.position_=(c.position_+1)%16 ;
                if (data_[c.groove_][c.position_] == NO_GROOVE_DATA) {
                    c.position_=0 ;
                };
                c.ticks_=0 ;
				stepped=true ;
            }
        }
	} else {  // Note
		if (c.ticks_==0) {
			c.position_=(c.position_+1)%16 ;
            if (data_[c.groove_][c.position_] == NO_GROOVE_DATA) {
                c.position_=0 ;
            };
            c.ticks_ = GrooveTicks(data_[c.groove_][c.position_]);
            stepped=true ;
		} ;
		c.ticks_-- ;
	}
	return stepped ;
}

void Groove::SetGroove(int channel,int groove) {
		if (groove>=MAX_GROOVES) return ;
		channelGroove_[channel].groove_=groove ;
		channelGroove_[channel].position_=0 ;
        channelGroove_[channel].ticks_ = GrooveTicks(data_[groove][0]);
} ;

// Returns true if, according to current groove setting it is time to go
// to the next sequencing step

bool Groove::TriggerChannel(int i) {
	ChannelGroove &c=channelGroove_[i] ;
    return ((c.ticks_) % GrooveTicks(data_[c.groove_][c.position_]) == 0);
} ;

unsigned char *Groove::GetGrooveData(int groove) {
	return data_[groove] ;
} ;
