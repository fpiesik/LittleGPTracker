#include "ProjectView.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Model/Scale.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/Views/ModalDialogs/NewProjectDialog.h"
#include "Application/Views/ModalDialogs/SelectProjectDialog.h"
#include "BaseClasses/UIActionField.h"
#include "BaseClasses/UIField.h"
#include "BaseClasses/UIIntVarField.h"
#include "BaseClasses/UIStaticField.h"
#include "BaseClasses/UITempoField.h"
#include "Services/Midi/MidiService.h"
#include "System/System/System.h"

#if defined(PLATFORM_X64)
#include "Adapters/SDL2/Audio/SDLAudioDriver.h"
class RecordOptionsDialog : public ModalView {
  public:
    explicit RecordOptionsDialog(View &parent)
        : ModalView(parent), selected_(0) {}
    virtual void OnFocus() {}
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void DrawView() {
        SetWindow(29, 9);
        SetColor(CD_NORMAL);
        GUITextProperties props;
        DrawString(0, 0, "Record Options", props);
        const char *metronome[] = {"off", "record", "play and rec"};
        char countIn[24];
        if (SDLAudioDriver::RecordCountIn == 0)
            snprintf(countIn, sizeof(countIn), "Count in: off");
        else
            snprintf(countIn, sizeof(countIn), "Count in: %d",
                     SDLAudioDriver::RecordCountIn);
        char volume[32];
        snprintf(volume, sizeof(volume), "Metronome volume: %d%%",
                 SDLAudioDriver::RecordMetronomeVolume);
        std::string rows[] = {SDLAudioDriver::RecordChannels == 2
                                  ? "Channels: Stereo"
                                  : "Channels: Mono",
                              std::string("Metronome: ") +
                                  metronome[SDLAudioDriver::RecordMetronome],
                              volume, countIn};
        for (int i = 0; i < 4; ++i) {
            props.invert_ = selected_ == i;
            DrawString(0, i + 2, rows[i].c_str(), props);
        }
        props.invert_ = false;
        const Shortcuts &keys = Shortcuts::Get();
        std::string hint =
            keys.KeyNames(Shortcuts::RecordOptionsToggle) + ": change  " +
            keys.KeyNames(Shortcuts::RecordOptionsBack) + ": back";
        DrawString(0, 7, hint.c_str(), props);
    }
    virtual void ProcessButtonMask(unsigned short mask, bool pressed) {
        if (!pressed)
            return;
        const Shortcuts &keys = Shortcuts::Get();
        if (keys.Matches(Shortcuts::RecordOptionsUp, mask, pressed)) {
            selected_ = (selected_ + 3) % 4;
            isDirty_ = true;
        } else if (keys.Matches(Shortcuts::RecordOptionsDown, mask, pressed)) {
            selected_ = (selected_ + 1) % 4;
            isDirty_ = true;
        } else if (keys.Matches(Shortcuts::RecordOptionsLeft, mask, pressed) ||
                   keys.Matches(Shortcuts::RecordOptionsRight, mask, pressed) ||
                   keys.Matches(Shortcuts::RecordOptionsToggle, mask,
                                pressed)) {
            int direction =
                keys.Matches(Shortcuts::RecordOptionsLeft, mask, pressed) ? -1
                                                                          : 1;
            if (selected_ == 0)
                SDLAudioDriver::RecordChannels =
                    3 - SDLAudioDriver::RecordChannels;
            else if (selected_ == 1)
                SDLAudioDriver::RecordMetronome =
                    (SDLAudioDriver::RecordMetronome + direction + 3) % 3;
            else if (selected_ == 2)
                SDLAudioDriver::RecordMetronomeVolume = SDL_max(
                    0, SDL_min(100, SDLAudioDriver::RecordMetronomeVolume +
                                        direction * 10));
            else
                SDLAudioDriver::RecordCountIn =
                    (SDLAudioDriver::RecordCountIn + direction + 9) % 9;
            isDirty_ = true;
        }
        if (keys.Matches(Shortcuts::RecordOptionsBack, mask, pressed))
            EndModal(0);
    }

  private:
    int selected_;
};
static void RecordOptionsClosed(View &, ModalView &) {}
#define ACTION_RECORD_OPTIONS MAKE_FOURCC('R', 'O', 'P', 'T')
#endif

#define ACTION_PURGE MAKE_FOURCC('P', 'U', 'R', 'G')
#define ACTION_SAVE MAKE_FOURCC('S', 'A', 'V', 'E')
#define ACTION_SAVE_AS MAKE_FOURCC('S', 'V', 'A', 'S')
#define ACTION_LOAD MAKE_FOURCC('L', 'O', 'A', 'D')
#define ACTION_QUIT MAKE_FOURCC('Q', 'U', 'I', 'T')
#define ACTION_PURGE_INSTRUMENT MAKE_FOURCC('P', 'R', 'G', 'I')
#define ACTION_TEMPO_CHANGED    MAKE_FOURCC('T','E','M','P')

static void SaveAsProjectCallback(View &v,ModalView &dialog) {

    FileSystemService FSS;
    NewProjectDialog &npd=(NewProjectDialog &)dialog;

    if (dialog.GetReturnCode()>0) {
		std::string str_dstprjdir;
		std::string str_dstsmpdir;

		Path root("root:");
		str_dstprjdir = root.GetName() + "/" + npd.GetName();
		str_dstsmpdir = str_dstprjdir + "/samples/";

		Path path_srcprjdir("project:");
		Path path_srcsmpdir("project:samples");
		Path path_dstprjdir = Path(str_dstprjdir);
		Path path_dstsmpdir = Path(str_dstsmpdir);

        Path path_srclgptdatsav = path_srcprjdir.GetPath() + "lgptsav_tmp.dat";
        Path path_dstlgptdatsav = path_dstprjdir.GetPath() + "/lgptsav.dat";

		if (path_dstprjdir.Exists()) {
			Trace::Log("ProjectView", "Dst Dir '%s' Exist == true",
			path_dstprjdir.GetPath().c_str());
		} else {
			if (FileSystem::GetInstance()->MakeDir(path_dstprjdir.GetPath().c_str()).Failed()) {
				Trace::Log("ProjectView", "Failed to create dir '%s'", path_dstprjdir.GetPath().c_str());
				return;
			};

		if (FileSystem::GetInstance()->MakeDir(path_dstsmpdir.GetPath().c_str()).Failed()) {
			Trace::Log("ProjectView", "Failed to create sample dir '%s'", path_dstprjdir.GetPath().c_str());
			return;
		};

        if (FSS.Copy(path_srclgptdatsav, path_dstlgptdatsav) > -1) {
            FSS.Delete(path_srclgptdatsav);
        }

        I_Dir *idir_srcsmpdir =
            FileSystem::GetInstance()->Open(path_srcsmpdir.GetPath().c_str());
        if (idir_srcsmpdir) {
				idir_srcsmpdir->GetContent("*");
				idir_srcsmpdir->Sort();
				IteratorPtr<Path>it(idir_srcsmpdir->GetIterator());
				for (it->Begin();!it->IsDone();it->Next()) {
					Path &current=it->CurrentItem();
					if (current.IsFile()) {
						Path dstfile = Path((str_dstsmpdir+current.GetName()).c_str());
						Path srcfile = Path(current.GetPath());
						FSS.Copy(srcfile.GetPath(),dstfile.GetPath());
					}
				}
			}

		((ProjectView &)v).OnSaveAsProject((char*)str_dstprjdir.c_str());
		}
    }
}

static void LoadCallback(View &v,ModalView &dialog) {
    MixerService::GetInstance()->SetRenderMode(0);
    if (dialog.GetReturnCode()==MBL_YES) {
		((ProjectView &)v).OnLoadProject() ;
	}
} ;

static void QuitCallback(View &v,ModalView &dialog) {
    MixerService::GetInstance()->SetRenderMode(0);
    if (dialog.GetReturnCode()==MBL_YES) {
		((ProjectView &)v).OnQuit() ;
	}
} ;

static void PurgeCallback(View &v,ModalView &dialog) {
	((ProjectView &)v).OnPurgeInstruments(dialog.GetReturnCode()==MBL_YES) ;
} ;

ProjectView::ProjectView(GUIWindow &w,ViewData *data):FieldView(w,data) {

    lastClock_ = 0;
    lastTick_ = 0;

	project_=data->project_ ;

	GUIPoint position=GetAnchor() ;
	
	Variable *v=project_->FindVariable(VAR_TEMPO) ;
    UITempoField *f = new UITempoField(ACTION_TEMPO_CHANGED, position, *v,
                                       "Tempo: %d [%2.2x]  ", 60, 400, 1, 10);
    T_SimpleList<UIField>::Insert(f) ;
	f->AddObserver(*this) ;
	tempoField_=f ;

    v = project_->FindVariable(VAR_MASTERVOL);
    position._y += 1;
    UIIntVarField *field =
        new UIIntVarField(position, *v, "Master: %d", 10, 100, 1, 10);
    T_SimpleList<UIField>::Insert(field);

    v = project_->FindVariable(VAR_PREGAIN);
    position._y += 2;
    field = new UIIntVarField(position, *v, "Drive: %d", 10, 200, 1, 10);
    T_SimpleList<UIField>::Insert(field);

    position._y += 1;
    v = project_->FindVariable(VAR_SOFTCLIP);
    field = new UIIntVarField(position, *v, "Type: %s", 0, 4, 1, 4);
    T_SimpleList<UIField>::Insert(field);

    v = project_->FindVariable(VAR_SOFTCLIP_GAIN);
    position._x += 13;
    field = new UIIntVarField(position, *v, "%s", 0, 1, 1, 1);
    T_SimpleList<UIField>::Insert(field);
    position._x -= 13;

    v = project_->FindVariable(VAR_TRANSPOSE);
    position._y += 2;
    UIIntVarField *f2=new UIIntVarField(position,*v,"Transpose: %3.2d",-48,48,0x1,0xC) ;
	T_SimpleList<UIField>::Insert(f2) ;

    v = project_->FindVariable(VAR_SCALE);
	// if scale name is not found, set the default chromatic scale
	if (v->GetInt() < 0) {
		v->SetInt(0);
    }
    position._y += 1;
    field =
        new UIIntVarField(position, *v, "Scale: %s", 0, scaleCount - 1, 1, 10);
    T_SimpleList<UIField>::Insert(field);

    position._y += 2;
    UIActionField *a1 =
        new UIActionField("Compact Sequencer", ACTION_PURGE, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

    position._y += 1;
    a1 = new UIActionField("Compact Instruments", ACTION_PURGE_INSTRUMENT,
                           position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

    position._y += 2;
    a1 = new UIActionField("Load Song", ACTION_LOAD, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

    position._y += 1;
    a1 = new UIActionField("Save Song", ACTION_SAVE, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

    position._y += 1;
    a1 = new UIActionField("Save Song As", ACTION_SAVE_AS, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

    v = project_->FindVariable(VAR_MIDIDEVICE);
    NAssert(v);
    position._y += 2;
    field = new UIIntVarField(position, *v, "MIDI: %s", 0,
                              MidiService::GetInstance()->Size(), 1, 1);
    T_SimpleList<UIField>::Insert(field);

    position._y += 2;
    v = project_->FindVariable(VAR_RENDER);
    NAssert(v);
    field = new UIIntVarField(position, *v, "Render: %s", 0,
                              project_->MAX_RENDER_MODE - 1, 1, 2);
    T_SimpleList<UIField>::Insert(field);

    position._y += 2;
#if defined(PLATFORM_X64)
    position._y -= 1;
    a1 = new UIActionField("Record Options", ACTION_RECORD_OPTIONS, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);
    position._y += 1;
#endif
    a1 = new UIActionField("Exit", ACTION_QUIT, position);
    a1->AddObserver(*this);
    T_SimpleList<UIField>::Insert(a1);

}

ProjectView::~ProjectView() {
}

void ProjectView::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;

    static const struct {
        Shortcuts::Action action;
        unsigned short fieldMask;
    } fields[] = {
        {Shortcuts::ProjectActivate, EPBM_A},
        {Shortcuts::ProjectTapTempo, EPBM_B},
        {Shortcuts::ProjectDown, EPBM_DOWN},
        {Shortcuts::ProjectUp, EPBM_UP},
        {Shortcuts::ProjectLeft, EPBM_LEFT},
        {Shortcuts::ProjectRight, EPBM_RIGHT},
        {Shortcuts::ProjectEditDown, EPBM_A | EPBM_DOWN},
        {Shortcuts::ProjectEditUp, EPBM_A | EPBM_UP},
        {Shortcuts::ProjectEditLeft, EPBM_A | EPBM_LEFT},
        {Shortcuts::ProjectEditRight, EPBM_A | EPBM_RIGHT},
        {Shortcuts::ProjectNudgeDown, EPBM_B | EPBM_LEFT},
        {Shortcuts::ProjectNudgeUp, EPBM_B | EPBM_RIGHT},
    };

    const Shortcuts &keys = Shortcuts::Get();
    unsigned short fieldMask = 0;
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        if (keys.Matches(fields[i].action, mask, pressed))
            fieldMask |= fields[i].fieldMask;
    FieldView::ProcessButtonMask(fieldMask);

    if (keys.Matches(Shortcuts::ProjectSong, mask, pressed)) {
        ViewType type = VT_SONG;
        ViewEvent event(VET_SWITCH_VIEW, &type);
        SetChanged();
        NotifyObservers(&event);
    } else if (keys.Matches(Shortcuts::ProjectPlay, mask, pressed)) {
        Player *player = Player::GetInstance();
        if (viewData_->renderMode_ > 0 && !player->IsRunning()) {
            viewData_->isRendering_ = true;
            View::SetNotification("Rendering started!");
        } else if (viewData_->isRendering_ && player->IsRunning()) {
            viewData_->isRendering_ = false;
            View::SetNotification("Rendering done!");
        }
        player->OnStartButton(PM_SONG, viewData_->songX_, false,
                              viewData_->songX_);
    }
}

void ProjectView::DrawView() {

    Clear();

    GUITextProperties props;
    GUIPoint pos=GetTitlePosition() ;

// Draw title

	char projectString[80] ;
    sprintf(projectString, "Project (Build %s.%s.%s)", PROJECT_NUMBER,
            PROJECT_RELEASE, BUILD_COUNT);

    SetColor(CD_NORMAL);
    DrawString(pos._x,pos._y,projectString,props) ;

    FieldView::Redraw();
    drawMap();

    int currentMode = project_->GetRenderMode();
    if ((viewData_->renderMode_ != currentMode) && !MixerService::GetInstance()->IsRendering()) {
        // Mode changed
        if (currentMode > 0 && viewData_->renderMode_ == 0) {
            const std::string hint =
                "Rendering on, press " +
                Shortcuts::Get().KeyNames(Shortcuts::ProjectPlay);
            View::SetNotification(hint.c_str());
        } else if (currentMode == 0 && viewData_->renderMode_ > 0) {
            View::SetNotification("Rendering off");
        }
        viewData_->renderMode_ = currentMode;
        MixerService::GetInstance()->SetRenderMode(currentMode);
    }

    View::EnableNotification();
};

void ProjectView::Update(Observable &,I_ObservableData *data) {

	if (!hasFocus_) {
		return ;
	}

# ifdef _64BIT
	int fourcc=*((int*)data);
#else
    int fourcc = (unsigned int)data;
#endif

    UIField *focus = GetFocus();
    if (fourcc!= ACTION_TEMPO_CHANGED) {
		focus->ClearFocus() ;
		focus->Draw(w_) ;
		w_.Flush() ;
		focus->SetFocus() ;
	} else {
		focus=tempoField_ ;
	}
    Player *player = Player::GetInstance();
    switch (fourcc) {
#if defined(PLATFORM_X64)
    case ACTION_RECORD_OPTIONS:
        DoModal(new RecordOptionsDialog(*this), RecordOptionsClosed);
        break;
#endif
		case ACTION_PURGE:
			project_->Purge() ;
			break ;
		case ACTION_PURGE_INSTRUMENT:
		{
            MessageBox *mb = new MessageBox(*this, "Purge unused samples?",
                                            MBBF_YES | MBBF_NO);
            DoModal(mb,PurgeCallback) ;
			break ;
		}
        case ACTION_SAVE: {
            MixerService::GetInstance()->SetRenderMode(0);
            PersistencyService *service = PersistencyService::GetInstance();
            service->Save();
            break;
        }
        case ACTION_SAVE_AS: {
            PersistencyService *service = PersistencyService::GetInstance();
            service->Save("project:lgptsav_tmp.dat");
            NewProjectDialog *mb = new NewProjectDialog(*this, "root:");
            DoModal(mb, SaveAsProjectCallback);
            break;
        }
        case ACTION_LOAD: {
            MessageBox *mb = new MessageBox(
                *this, "Load song and lose changes ?", MBBF_YES | MBBF_NO);
            DoModal(mb, LoadCallback);
            break;
        }
        case ACTION_QUIT: {
            MessageBox *mb = new MessageBox(*this, "Quit and lose faith ?",
                                            MBBF_YES | MBBF_NO);
            DoModal(mb, QuitCallback);
            break;
        }
        case ACTION_TEMPO_CHANGED:
			break ;
		default:
			NInvalid ;
			break ;
	} ;
    focus->Draw(w_) ;
	isDirty_=true ;
} ;

void ProjectView::OnPurgeInstruments(bool removeFromDisk) {
	project_->PurgeInstruments(removeFromDisk) ;
} ;

void ProjectView::OnLoadProject() {
	ViewEvent ve(VET_QUIT_PROJECT) ;
	SetChanged();
	NotifyObservers(&ve) ;
} ;

void ProjectView::OnSaveAsProject(char * data) {
        ViewEvent ve(VET_SAVEAS_PROJECT,data) ;
	SetChanged();
	NotifyObservers(&ve) ;
} ;

void ProjectView::OnQuit() {
	ViewEvent ve(VET_QUIT_APP) ;
	SetChanged();
	NotifyObservers(&ve) ;
} ;
