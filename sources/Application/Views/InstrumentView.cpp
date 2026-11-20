#include "InstrumentView.h"
#include "Application/Controllers/Shortcuts.h"
#ifdef LGPT_PLAITS
#include "Application/Instruments/PlaitsInstrument.h"
#endif

#define SIP_EDIT MAKE_FOURCC('E', 'D', 'I', 'T')
#define SIP_ENGINE MAKE_FOURCC('E', 'N', 'G', 'N')
#include "Application/Instruments/MidiInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Model/Config.h"
#include "BaseClasses/UIBigHexVarField.h"
#include "BaseClasses/UIIntVarOffField.h"
#include "BaseClasses/UINoteVarField.h"
#include "BaseClasses/UIStaticField.h"
#include "Foundation/Variables/Variable.h"
#include "ModalDialogs/ImportSampleDialog.h"
#include "ModalDialogs/MessageBox.h"
#include "System/System/System.h"

#ifdef LGPT_PLAITS
static const char *engineNames[] = {"Sampler", "Plaits"};
#endif

void ImportSampleDialogCallback(View &v, ModalView &dialog) {
    ((InstrumentView &)v).OnFocus();
}

InstrumentView::InstrumentView(GUIWindow &w, ViewData *data)
    : FieldView(w, data),
#ifdef LGPT_PLAITS
      engine_("engine", SIP_ENGINE, engineNames, 2, 0),
#endif
      editSample_("edit", SIP_EDIT, 0) {

    project_ = data->project_;
    lastFocusID_=0 ;
	current_=0 ;
    currentIndex_ = -1;
    onInstrumentChange() ;
}

InstrumentView::~InstrumentView() {
    if (current_ &&
        project_->GetInstrumentBank()->GetInstrument(currentIndex_) == current_)
        current_->RemoveObserver(*this);
}

InstrumentType InstrumentView::getInstrumentType() {
	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instrument=bank->GetInstrument(i) ;
    return instrument->GetType() ;
} ;

void InstrumentView::onInstrumentChange() {

	ClearFocus() ;

    int i = viewData_->currentInstrument_;
    InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
    I_Instrument *next = bank->GetInstrument(i);
    bool changed = current_ != next;
    // Another view may have replaced the old slot while this view was hidden.
    // Never dereference a cached pointer unless the bank still owns it.
    if (changed && current_ && bank->GetInstrument(currentIndex_) == current_)
        current_->RemoveObserver(*this);
    current_ = next;
    currentIndex_ = i;
    T_SimpleList<UIField>::Empty();

    InstrumentType it = getInstrumentType();

#ifdef LGPT_PLAITS
    if (it == IT_SAMPLE || it == IT_PLAITS) {
        engine_.SetInt(it == IT_PLAITS ? 1 : 0);
        GUIPoint position = GetAnchor();
        T_SimpleList<UIField>::Insert(
            new UIIntVarField(position, engine_, "engine: %s", 0, 1, 1, 1));
    }
#endif

    switch (it) {
#ifdef LGPT_PLAITS
    case IT_PLAITS: {
        PlaitsInstrument *instrument =
            static_cast<PlaitsInstrument *>(current_);
        GUIPoint position = GetAnchor();
        ++position._y;
        const char *labels[] = {"MODEL: %d", "HARMONICS: %2.2X",
                                "TIMBRE: %2.2X", "MORPH: %2.2X",
                                "DECAY: %2.2X"};
        for (int p = 0; p < PlaitsInstrument::PARAMETER_COUNT; ++p) {
            T_SimpleList<UIField>::Insert(new UIIntVarField(
                position, instrument->Parameter(p), labels[p], 0,
                p == PlaitsInstrument::MODEL ? 23 : 255, 1, 16));
            ++position._y;
        }
        break;
    }
#endif
    case IT_MIDI:
        fillMidiParameters();
        break;
    case IT_SAMPLE:
        fillSampleParameters();
        break;
    default:
        break;
    } ;

	SetFocus(T_SimpleList<UIField>::GetFirst()) ;
	IteratorPtr<UIField> it2(T_SimpleList<UIField>::GetIterator()) ;
	for (it2->Begin();!it2->IsDone();it2->Next()) {
        UIIntVarField &field=(UIIntVarField &)it2->CurrentItem() ;
        if (field.GetVariableID()==lastFocusID_) {
            SetFocus(&field) ;
            break ;
        }
    } ;
    // A parameter notification may already be iterating this observer list.
    if (changed)
        current_->AddObserver(*this);
} ;

void InstrumentView::fillSampleParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	SampleInstrument *instrument=(SampleInstrument *)instr  ;
	GUIPoint position=GetAnchor() ;

#ifdef LGPT_PLAITS
    ++position._y;
#endif
    //	position._y+=View::fieldSpaceHeight_;
    Variable *v = instrument->FindVariable(SIP_SAMPLE);
    SamplePool *sp=SamplePool::GetInstance() ;
	UIIntVarField *f1=new UIIntVarField(position,*v,"sample: %s",0,sp->GetNameListSize()-1,1,0x10) ;
    T_SimpleList<UIField>::Insert(f1);

    position._y += 1;
    f1 = new UIIntVarField(position, editSample_, "Edit", 0, 0, 0, 0);
    T_SimpleList<UIField>::Insert(f1);
    position._y += 1;
#ifdef FFMPEG_ENABLED
    v = instrument->FindVariable(SIP_PRINTFX);
    f1 = new UIIntVarField(position, *v, "%s", 0, 3, 1, 2);
    T_SimpleList<UIField>::Insert(f1) ;

    position._x += 7;
    v = instrument->FindVariable(SIP_IR_WET);
    f1 = new UIIntVarField(position, *v, "wet:%d%%", 0, 100, 1, 10);
    T_SimpleList<UIField>::Insert(f1);
    position._x += 9;

    v = instrument->FindVariable(SIP_IR_PAD);
    f1 = new UIIntVarField(position, *v, "pad:%dms", 0, 5000, 5, 100);
    T_SimpleList<UIField>::Insert(f1);
    position._x -= 16;
#endif
    position._y += 1;
    v=instrument->FindVariable(SIP_VOLUME) ;
	f1=new UIIntVarField(position,*v,"volume: %d [%2.2X]",0,255,1,10) ;
	T_SimpleList<UIField>::Insert(f1) ;

    position._y+=1 ;
	v=instrument->FindVariable(SIP_PAN) ;
	f1=new UIIntVarField(position,*v,"pan: %2.2X",0,0xFE,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_ROOTNOTE) ;
	f1=new UINoteVarField(position,*v,"root note: %s",0,0x7F,1,0x0C) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_FINETUNE) ;
	f1=new UIIntVarField(position,*v,"detune: %2.2X",0,255,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

#ifdef LGPT_PLAITS
    // Use one existing spacer row for the engine selector.
    position._y += 1;
#else
    position._y += 2;
#endif
    v=instrument->FindVariable(SIP_CRUSH);
	f1=new UIIntVarField(position,*v,"crush: %d",1,0x10,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;

    position._x += 10;
    v = instrument->FindVariable(SIP_CRUSHVOL);
    f1=new UIIntVarField(position,*v,"drive: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
    position._x -= 10;

    position._y += 1;
	v=instrument->FindVariable(SIP_DOWNSMPL) ;
	f1=new UIIntVarField(position,*v,"downsample: %d",0,8,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;


	position._y+=2 ;
	UIStaticField *sf=new UIStaticField(position,"flt cut/res:") ;
	T_SimpleList<UIField>::Insert(sf) ;

	position._x+=13 ;
	v=instrument->FindVariable(SIP_FILTCUTOFF) ;
	f1=new UIIntVarField(position,*v,"%2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._x+=3 ;
	v=instrument->FindVariable(SIP_FILTRESO) ;
	f1=new UIIntVarField(position,*v,"%2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._x-=16 ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_FILTMIX) ;
	f1=new UIIntVarField(position,*v,"type: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_FILTMODE) ;
	f1=new UIIntVarField(position,*v,"Mode: %s",0,2,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_ATTENUATE) ;
	f1=new UIIntVarField(position,*v,"attenuate: %d [%2.2X]",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	sf=new UIStaticField(position,"fb tune/mix: ") ;
	T_SimpleList<UIField>::Insert(sf) ;

	v=instrument->FindVariable(SIP_FBTUNE) ;
	position._x+=13 ;
	f1=new UIIntVarField(position,*v,"%2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._x+=3 ;
	v=instrument->FindVariable(SIP_FBMIX) ;
	f1=new UIIntVarField(position,*v,"%2.2X",0,0xFF,1,0X10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._x-=16 ;

	position._y+=2;
	v=instrument->FindVariable(SIP_INTERPOLATION) ;
	f1=new UIIntVarField(position,*v,"interpolation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;

    position._y+=1 ;
	v=instrument->FindVariable(SIP_LOOPMODE) ;
	f1=new UIIntVarField(position,*v,"loop mode: %s",0,SILM_LAST-1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	position._y+=1 ;

	v=instrument->FindVariable(SIP_SLICES) ;
	f1=new UIIntVarField(position,*v,"slices: %2.2X",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_START) ;
	f1=new UIBigHexVarField(position,*v,7,"start: %7.7X",0,instrument->GetSampleSize()-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_LOOPSTART) ;
	f1=new UIBigHexVarField(position,*v,7,"loop start: %7.7X",0,instrument->GetSampleSize()-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_END) ;
	f1=new UIBigHexVarField(position,*v,7,"loop end: %7.7X",0,instrument->GetSampleSize()-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	v=instrument->FindVariable(SIP_TABLEAUTO) ;
	position._y+=2 ;
	UIIntVarField *f2=new UIIntVarField(position,*v,"automation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_TABLE) ;
	f1=new UIIntVarOffField(position,*v,"table: %2.2X",0x00,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

} ;

void InstrumentView::fillMidiParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	MidiInstrument *instrument=(MidiInstrument *)instr  ;
	GUIPoint position=GetAnchor() ;

	Variable *v=instrument->FindVariable(MIP_CHANNEL) ;
	UIIntVarField* f1=new UIIntVarField(position,*v,"channel: %2.2d",0,0x0F,1,0x04,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	f1->SetFocus() ;

	position._y+=1;
	v=instrument->FindVariable(MIP_VOLUME) ;
	f1=new UIIntVarField(position,*v,"volume: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_NOTELENGTH) ;
	f1=new UIIntVarField(position,*v,"length: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	v=instrument->FindVariable(MIP_TABLEAUTO) ;
	position._y+=2 ;
	UIIntVarField *f2=new UIIntVarField(position,*v,"automation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_TABLE) ;
	f1=new UIIntVarOffField(position,*v,"table: %2.2X",0,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

} ;


void InstrumentView::warpToNext(int offset) {
	int instrument=viewData_->currentInstrument_+offset ;
	if (instrument>=MAX_INSTRUMENT_COUNT) {
		instrument=instrument-MAX_INSTRUMENT_COUNT ;
	} ;
	if (instrument<0) {
		instrument=MAX_INSTRUMENT_COUNT+instrument ;
	} ;
	viewData_->currentInstrument_=instrument ;
	onInstrumentChange() ;
	isDirty_=true ;
} ;

static unsigned short instrumentDirections(const Shortcuts &keys,
                                           unsigned short mask, bool edit) {
    static const struct {
        Shortcuts::Action move, adjust;
        unsigned short direction;
    } directions[] = {
        {Shortcuts::InstrumentDown, Shortcuts::InstrumentEditDown, EPBM_DOWN},
        {Shortcuts::InstrumentUp, Shortcuts::InstrumentEditUp, EPBM_UP},
        {Shortcuts::InstrumentLeft, Shortcuts::InstrumentEditLeft, EPBM_LEFT},
        {Shortcuts::InstrumentRight, Shortcuts::InstrumentEditRight,
         EPBM_RIGHT},
    };
    unsigned short result = 0;
    for (unsigned i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
        if (keys.Matches(edit ? directions[i].adjust : directions[i].move, mask,
                         true))
            result |= directions[i].direction;
    }
    return result;
}

void InstrumentView::ProcessButtonMask(unsigned short mask, bool pressed) {

    if (!pressed)
        return;

    isDirty_ = false;
    const Shortcuts &keys = Shortcuts::Get();
    bool engineField = false;
#ifdef LGPT_PLAITS
    // Handle engine edits before the generic field processing. Rebuild only
    // after ProcessArrow returns, so no field deletes itself during a callback.
    engineField =
        GetFocus() &&
        static_cast<UIIntVarField *>(GetFocus())->GetVariableID() == SIP_ENGINE;
    if (engineField &&
        keys.Matches(Shortcuts::InstrumentEngineModifier, mask, pressed)) {
        unsigned short remaining =
            keys.RemainingButtons(Shortcuts::InstrumentEngineModifier, mask);
        unsigned short directions =
            instrumentDirections(keys, remaining, false);
        if (remaining && !directions)
            return;
        viewMode_ = VM_NORMAL;
        FieldView::ProcessButtonMask(EPBM_A | directions);
        InstrumentType type = engine_.GetInt() ? IT_PLAITS : IT_SAMPLE;
        if (type != getInstrumentType()) {
            // AppWindow::onEvent already holds MixerService's lock here.
            ClearFocus();
            T_SimpleList<UIField>::Empty();
            current_->RemoveObserver(*this);
            current_ = 0;
            project_->GetInstrumentBank()->SetType(
                viewData_->currentInstrument_, type);
            lastFocusID_ = SIP_ENGINE;
            viewMode_ = VM_NORMAL;
            onInstrumentChange();
            isDirty_ = true;
        }
        return;
    }
#endif
    if (keys.Matches(Shortcuts::InstrumentOpenEditor, mask, pressed) &&
        GetFocus() &&
        ((UIIntVarField *)GetFocus())->GetVariableID() == SIP_EDIT) {
        viewMode_ = VM_NORMAL;
        lastFocusID_ = SIP_EDIT;
        ViewType type = VT_SAMPLE_EDITOR;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
        return;
    }

    if (viewMode_ == VM_NEW && GetFocus()) {
        Variable &value =
            static_cast<UIIntVarField *>(GetFocus())->GetVariable();
        FourCC field = value.GetID();
        if (field == SIP_SAMPLE &&
            keys.Matches(Shortcuts::InstrumentImportSample, mask, pressed)) {
            mask =
                keys.RemainingButtons(Shortcuts::InstrumentImportSample, mask);
            Path library(SamplePool::GetInstance()->GetSampleLib());
            if (FileSystem::GetInstance()->GetFileType(
                    library.GetPath().c_str()) != FT_DIR)
                DoModal(new MessageBox(*this, "Can't access the samplelib",
                                       MBBF_OK));
            else
                DoModal(new ImportSampleDialog(*this),
                        ImportSampleDialogCallback);
        } else if (field == SIP_TABLE &&
                   keys.Matches(Shortcuts::InstrumentNewTable, mask, pressed)) {
            mask = keys.RemainingButtons(Shortcuts::InstrumentNewTable, mask);
            int next = TableHolder::GetInstance()->GetNext();
            if (next != NO_MORE_TABLE) {
                value.SetInt(next);
                isDirty_ = true;
            }
        } else if (field == SIP_PRINTFX &&
                   keys.Matches(Shortcuts::InstrumentPrintFx, mask, pressed)) {
            mask = keys.RemainingButtons(Shortcuts::InstrumentPrintFx, mask);
            FxPrinter printer(viewData_);
            isDirty_ = printer.Run();
            View::SetNotification(printer.GetNotification());
        }
    }

    if (viewMode_ == VM_CLONE && getInstrumentType() != IT_PLAITS &&
        GetFocus() &&
        static_cast<UIIntVarField *>(GetFocus())->GetVariableID() ==
            SIP_TABLE) {
        if (keys.Matches(Shortcuts::InstrumentCloneTable, mask, pressed)) {
            Variable &value =
                static_cast<UIIntVarField *>(GetFocus())->GetVariable();
            if (value.GetInt() == VAR_OFF)
                return;
            int next = TableHolder::GetInstance()->Clone(value.GetInt());
            if (next != NO_MORE_TABLE) {
                value.SetInt(next);
                isDirty_ = true;
            }
        }
        mask = keys.RemainingButtons(Shortcuts::InstrumentCloneTable, mask);
    }

    if (viewMode_ == VM_SELECTION) {
    } else {
        viewMode_ = VM_NORMAL;
    }

    unsigned short edit =
        engineField ? 0 : instrumentDirections(keys, mask, true);
    FieldView::ProcessButtonMask(
        edit ? (EPBM_A | edit) : instrumentDirections(keys, mask, false));

    if (keys.Matches(Shortcuts::InstrumentPrevious, mask, pressed))
        warpToNext(-1);
    if (keys.Matches(Shortcuts::InstrumentNext, mask, pressed))
        warpToNext(1);
    if (keys.Matches(Shortcuts::InstrumentPrevious16, mask, pressed))
        warpToNext(-16);
    if (keys.Matches(Shortcuts::InstrumentNext16, mask, pressed))
        warpToNext(16);

    if (GetFocus()) {
        UIIntVarField *field = static_cast<UIIntVarField *>(GetFocus());
        if (field->GetVariableID() == SIP_SAMPLE &&
            getInstrumentType() == IT_SAMPLE &&
            keys.Matches(Shortcuts::InstrumentClearSample, mask, pressed)) {
            current_->Purge();
            isDirty_ = true;
        } else if (field->GetVariableID() == SIP_TABLE &&
                   keys.Matches(Shortcuts::InstrumentClearTable, mask,
                                pressed)) {
            field->GetVariable().SetInt(VAR_OFF);
            isDirty_ = true;
        }
    }
    if (keys.Matches(Shortcuts::InstrumentBeginClone, mask, pressed)) {
        viewMode_ = VM_CLONE;
    } else if (keys.Matches(Shortcuts::InstrumentBeginNew, mask, pressed)) {
        FourCC varID = ((UIIntVarField *)GetFocus())->GetVariableID();
        if ((varID == SIP_TABLE) || (varID == MIP_TABLE) ||
            (varID == SIP_SAMPLE) || (varID == SIP_PRINTFX)) {
            viewMode_ = VM_NEW;
        }
    }

    if (keys.Matches(Shortcuts::InstrumentPhrase, mask, pressed)) {
        ViewType type = VT_PHRASE;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
    }
    if (keys.Matches(Shortcuts::InstrumentTable, mask, pressed)) {
        InstrumentBank *bank = project_->GetInstrumentBank();
        int table =
            bank->GetInstrument(viewData_->currentInstrument_)->GetTable();
        if (table != VAR_OFF)
            viewData_->currentTable_ = table;
        ViewType type = VT_TABLE2;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
    }
    bool fromStart =
        keys.Matches(Shortcuts::InstrumentPlayFromStart, mask, pressed);
    if (fromStart || keys.Matches(Shortcuts::InstrumentPlay, mask, pressed))
        Player::GetInstance()->OnStartButton(PM_PHRASE, viewData_->songX_,
                                             fromStart, viewData_->chainRow_);

    UIIntVarField *field = (UIIntVarField *)GetFocus();
    if (field) {
        lastFocusID_ = field->GetVariableID();
    }
};

void InstrumentView::DrawView() {

	Clear() ;
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[20];
    SetColor(CD_NORMAL);
    sprintf(title, "Instrument %2.2X", viewData_->currentInstrument_);
    DrawString(pos._x, pos._y, title, props);

    // Draw fields

    FieldView::Redraw();
    drawMap() ;
} ;

void InstrumentView::OnFocus() { onInstrumentChange(); }

void InstrumentView::Update(Observable &o,I_ObservableData *d) {
	onInstrumentChange() ;
}
