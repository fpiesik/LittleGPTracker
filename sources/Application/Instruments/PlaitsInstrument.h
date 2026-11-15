#ifndef _PLAITS_INSTRUMENT_H_
#define _PLAITS_INSTRUMENT_H_

#include "I_Instrument.h"
class PlaitsEngine;

class PlaitsInstrument : public I_Instrument {
  public:
    enum { MODEL, HARMONICS, TIMBRE, MORPH, DECAY, PARAMETER_COUNT };
    PlaitsInstrument();
    virtual ~PlaitsInstrument();
    static void InitVoices(int sampleRate);
    virtual bool Init() { return true; }
    virtual void OnStart() {}
    virtual bool Start(int channel, unsigned char note, int flags = 1);
    virtual void Stop(int channel);
    virtual bool Render(int channel, fixed *buffer, int size, int flags);
    virtual bool IsInitialized();
    virtual bool IsEmpty() { return false; }
    virtual InstrumentType GetType() { return IT_PLAITS; }
    virtual const char *GetName() { return "PLTS"; }
    virtual void ProcessCommand(int channel, FourCC cc, ushort value);
    virtual void Purge();
    virtual int GetTable() { return VAR_OFF; }
    virtual bool GetTableAutomation() { return false; }
    virtual void GetTableState(TableSaveState &state) { state.Reset(); }
    virtual void SetTableState(TableSaveState &state) {}
    Variable &Parameter(int index) { return *parameters_[index]; }

  private:
    // Shared across instrument definitions: memory grows with channel count,
    // not with the number of saved patches. Pointers are cached off the
    // callback.
    Variable *parameters_[PARAMETER_COUNT];
    static PlaitsEngine voices_[SONG_CHANNEL_COUNT];
    static PlaitsInstrument *owners_[SONG_CHANNEL_COUNT];
};
#endif
