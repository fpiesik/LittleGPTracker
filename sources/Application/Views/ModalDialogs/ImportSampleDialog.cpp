#include "ImportSampleDialog.h"
#include "Application/Controllers/Shortcuts.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Views/ModalDialogs/MessageBox.h"

#define LIST_SIZE 15
#define LIST_WIDTH 28

bool ImportSampleDialog::initStatic_=false ;
Path ImportSampleDialog::sampleLib_("") ;
Path ImportSampleDialog::currentPath_("") ;

static const char *buttonText[3]= {
	"Listen",
	"Import",
	"Exit"	
} ;

ImportSampleDialog::ImportSampleDialog(View &view):ModalView(view) {
	if (!initStatic_) {
		const char *slpath=SamplePool::GetInstance()->GetSampleLib() ;
		sampleLib_=Path(slpath) ;
		currentPath_=Path(slpath) ;
		initStatic_=true ;
	}
	selected_=0 ;
} ;

ImportSampleDialog::~ImportSampleDialog() {
} ;

void ImportSampleDialog::DrawView() {

    SetWindow(LIST_WIDTH, LIST_SIZE + 3);

    GUITextProperties props;

    // Draw title

    //	char title[40] ;

    SetColor(CD_NORMAL);

    //	sprintf(title,"Sample Import from %s",currentPath_.GetName()) ;
    //	w_.DrawString(title,pos,props) ;

    // Draw samples

    int x = 1;
    int y=1 ;

	if (currentSample_<topIndex_) {
		topIndex_=currentSample_ ;
	} ;
	if (currentSample_>=topIndex_+LIST_SIZE) {
		topIndex_=currentSample_ ;
	} ;

	IteratorPtr<Path> it(sampleList_.GetIterator()) ;
	int count=0 ;
	char buffer[256] ;
	for(it->Begin();!it->IsDone();it->Next()) {
		if ((count>=topIndex_)&&(count<topIndex_+LIST_SIZE)) {
			Path &current=it->CurrentItem() ;
			const std::string p=current.GetName() ;

			if (count==currentSample_) {
				SetColor(CD_HILITE2) ;
				props.invert_=true ;
			} else {
				SetColor(CD_NORMAL) ;
                props.invert_ = current.Matches("*.sf2");
            }
			if (!current.IsDirectory()) {
				strcpy(buffer,p.c_str()) ;
			} else {
				buffer[0]='[' ;
				strcpy(buffer+1,p.c_str()) ;
				strcat(buffer,"]") ;
			}
			buffer[LIST_WIDTH-1]=0 ;
			DrawString(x,y,buffer,props) ;
			y+=1 ;
		}
		count++ ;
	} ;

	y=LIST_SIZE+2 ;
	int offset=LIST_WIDTH/4 ;

	SetColor(CD_NORMAL) ;

	for (int i=0;i<3;i++) {
		const char *text=buttonText[i] ;
		x=offset*(i+1)-strlen(text)/2 ;
		props.invert_=(i==selected_)?true:false ;
		DrawString(x,y,text,props) ;
	}	
} ;

void ImportSampleDialog::warpToNextSample(int direction) {

	currentSample_+=direction ;
	int size=sampleList_.Size() ;
	if (currentSample_ < 0) currentSample_ = 0;
	if (currentSample_ >= size) currentSample_ = size - 1;
	endPreview();
	isDirty_=true ;
}

void ImportSampleDialog::OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) {
} ;

void ImportSampleDialog::OnFocus() {
	Path current(currentPath_) ;
	setCurrentFolder(&current) ;
	toInstr_=viewData_->currentInstrument_ ;
} ;

void ImportSampleDialog::preview(Path &element) {
	Player::GetInstance()->StartStreaming(element) ;
}

void ImportSampleDialog::endPreview() {
	Player::GetInstance()->StopStreaming() ;
}

void ImportSampleDialog::import(Path &element) {

	SamplePool *pool=SamplePool::GetInstance() ;
	int sampleID=pool->ImportSample(element) ;
	if (sampleID>=0) {
		I_Instrument *instr=viewData_->project_->GetInstrumentBank()->GetInstrument(toInstr_) ;
		if (instr->GetType()==IT_SAMPLE) {
			SampleInstrument *sinstr=(SampleInstrument *)instr ;
			sinstr->AssignSample(sampleID) ;
			toInstr_=viewData_->project_->GetInstrumentBank()->GetNext() ;
		};
	} else {
        const char *err_str = (sampleID == -SLOAD_ERR_MAX_SAMPLES)
                                  ? "Maximum number of samples exceeded"
                              : (sampleID == -SLOAD_ERR_MAX_SOUNDFONTS)
                                  ? "Maximum number of SoundFonts exceeded"
                              : (sampleID == -SLOAD_ERR_INVALID_DIR)
                                  ? "Invalid directory"
                                  : "Unable to open file";
        Trace::Error(err_str);
        MessageBox *mb = new MessageBox(*this, err_str);
        View::DoModal(mb);
	};
	isDirty_=true ;
} ;

void ImportSampleDialog::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed)
        return;
    const Shortcuts &s = Shortcuts::Get();
    bool pageUp = s.Matches(Shortcuts::SampleBrowserPageUp, mask, pressed);
    bool pageDown = s.Matches(Shortcuts::SampleBrowserPageDown, mask, pressed);
    if (pageUp || pageDown) {
        if (pageUp)
            warpToNextSample(-LIST_SIZE);
        if (pageDown)
            warpToNextSample(LIST_SIZE);
        return;
    }

    bool previous =
        s.Matches(Shortcuts::SampleBrowserActivatePrevious, mask, pressed);
    bool next = s.Matches(Shortcuts::SampleBrowserActivateNext, mask, pressed);
    if (previous || next ||
        s.Matches(Shortcuts::SampleBrowserActivate, mask, pressed)) {
        if (previous)
            warpToNextSample(-1);
        if (next)
            warpToNextSample(1);
        if (selected_ == 2) {
            endPreview();
            EndModal(0);
            return;
        }
        Path *element = getImportElement();
        if (!element)
            return;
        if (element->IsDirectory()) {
            if (!previous && !next)
                enterFolder(*element);
        } else if (selected_ == 1) {
            import(*element);
        } else if (!element->Matches("*.sf2")) {
            preview(*element);
        }
        return;
    }

    previous =
        s.Matches(Shortcuts::SampleBrowserPreviewPrevious, mask, pressed);
    next = s.Matches(Shortcuts::SampleBrowserPreviewNext, mask, pressed);
    bool load = s.Matches(Shortcuts::SampleBrowserLoad, mask, pressed);
    bool parent = s.Matches(Shortcuts::SampleBrowserParent, mask, pressed);
    if (previous || next || load || parent ||
        s.Matches(Shortcuts::SampleBrowserPreview, mask, pressed)) {
        if (previous)
            warpToNextSample(-1);
        if (next)
            warpToNextSample(1);
        Path *element = getImportElement();
        if (element && !element->IsDirectory()) {
            preview(*element);
            if (load) {
                endPreview();
                import(*element);
            }
        }
        if (parent) {
            if (!isSampleLibRoot()) {
                Path folder = currentPath_.GetParent();
                setCurrentFolder(&folder);
            }
        } else if (element && element->IsDirectory() && selected_ != 2 &&
                   !previous && !next) {
            enterFolder(*element);
        }
        return;
    }
    if (s.Matches(Shortcuts::SampleBrowserPrevious, mask, pressed))
        warpToNextSample(-1);
    if (s.Matches(Shortcuts::SampleBrowserNext, mask, pressed))
        warpToNextSample(1);
    if (s.Matches(Shortcuts::SampleBrowserLeft, mask, pressed)) {
        selected_ = (selected_ + 2) % 3;
        isDirty_ = true;
    }
    if (s.Matches(Shortcuts::SampleBrowserRight, mask, pressed)) {
        selected_ = (selected_ + 1) % 3;
        isDirty_ = true;
    }
}

bool ImportSampleDialog::isSampleLibRoot() {
    // return sampleLib_.GetPath().find(currentPath_.GetPath()) !=
    // std::string::npos; // Causes issues in Win, Miyoo
    return currentPath_.GetPath() == sampleLib_.GetPath();
};

Path *ImportSampleDialog::getImportElement() {
    IteratorPtr<Path> it(sampleList_.GetIterator());
    int count = 0;
    for (it->Begin(); !it->IsDone(); it->Next()) {
        if (count++ == currentSample_) {
            return &it->CurrentItem();
        }
    }
    return 0;
}

void ImportSampleDialog::enterFolder(Path &entry) {
    if (entry.GetName() == ".." && isSampleLibRoot())
        return;
    // Changing directories replaces the list that owns entry.
    Path folder = entry.GetName() == ".." ? currentPath_.GetParent() : entry;
    setCurrentFolder(&folder);
}

void ImportSampleDialog::setCurrentFolder(Path *path) {

    Path formerPath(currentPath_);

    topIndex_ = 0;
    currentSample_=0 ;

	currentPath_=Path(*path) ;
	sampleList_.Empty() ;
	if (path) {
		int count=0 ;
		I_Dir *dir=FileSystem::GetInstance()->Open(path->GetPath().c_str()) ;	
		if (dir) {
			dir->GetContent("*") ;
			dir->Sort() ;
			IteratorPtr<Path>it(dir->GetIterator()) ;
			for (it->Begin();!it->IsDone();it->Next()) {
				Path &current=it->CurrentItem() ;
		 		if (current.IsDirectory()) {
					if (current.GetName().substr(0,1)!="." || current.GetName()=="..") {
						Path *sample=new Path(current) ;
						sampleList_.Insert(sample) ;
						if (!formerPath.Compare(current)) {
							currentSample_=count ;
						}
						count++ ;
					}
				}
			}
			for (it->Begin();!it->IsDone();it->Next()) {
                Path &current = it->CurrentItem();
                if (!current.IsDirectory()) {
                                  if ((current.Matches("*.wav") || current.Matches("*.sf2")) && current.GetName()[0]!='.') {
						Path *sample=new Path(current) ;
						sampleList_.Insert(sample) ;
					}
				};
			}
		}
	}
    isDirty_ = true;
};
