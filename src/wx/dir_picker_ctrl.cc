/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "dir_picker_ctrl.h"
#include "wx_util.h"
#include "static_text.h"
#include "dcpomatic_button.h"
#include "lib/warnings.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/filepicker.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;

DirPickerCtrl::DirPickerCtrl (wxWindow* parent)
	: wxPanel (parent)
{
	_sizer = new wxBoxSizer (wxHORIZONTAL);

	_folder = new StaticText (this, wxT(""));
	wxFont font = _folder->GetFont ();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	_folder->SetFont (font);
	_sizer->Add (_folder, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
	_browse = new Button (this, _("Browse..."));
	_sizer->Add (_browse, 0);

	SetSizer (_sizer);

	_browse->Bind (wxEVT_BUTTON, boost::bind (&DirPickerCtrl::browse_clicked, this));
}

void
DirPickerCtrl::SetPath (wxString p)
{
	_path = p;

	if (_path == wxStandardPaths::Get().GetDocumentsDir()) {
		_folder->SetLabel (_("My Documents"));
	} else {
		_folder->SetLabel (_path);
	}

	wxCommandEvent ev (wxEVT_DIRPICKER_CHANGED, wxID_ANY);
	GetEventHandler()->ProcessEvent (ev);

	_sizer->Layout ();
	SetMinSize (wxSize (max (400, _sizer->GetSize().GetWidth()), -1));

	Changed ();
}

wxString
DirPickerCtrl::GetPath () const
{
	return _path;
}

void
DirPickerCtrl::browse_clicked ()
{
	wxDirDialog* d = new wxDirDialog (this);
	if (d->ShowModal () == wxID_OK) {
		SetPath (d->GetPath ());
	}
	d->Destroy ();
}
