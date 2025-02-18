/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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

#include "check_box.h"
#include "export_video_file_dialog.h"
#include "file_picker_ctrl.h"
#include "wx_util.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/filepicker.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>

using std::string;
using boost::bind;

#define FORMATS 2

wxString format_names[] = {
	_("MOV / ProRes"),
	_("MP4 / H.264"),
};

wxString format_filters[] = {
	_("MOV files (*.mov)|*.mov"),
	_("MP4 files (*.mp4)|*.mp4"),
};

wxString format_extensions[] = {
	"mov",
	"mp4",
};

ExportFormat formats[] = {
	ExportFormat::PRORES,
	ExportFormat::H264_AAC,
};

ExportVideoFileDialog::ExportVideoFileDialog (wxWindow* parent, string name)
	: TableDialog (parent, _("Export video file"), 2, 1, true)
	, _initial_name (name)
{
	add (_("Format"), true);
	_format = new wxChoice (this, wxID_ANY);
	add (_format);
	add_spacer ();
	_mixdown = new CheckBox (this, _("Mix audio down to stereo"));
	add (_mixdown, false);
	add_spacer ();
	_split_reels = new CheckBox (this, _("Write reels into separate files"));
	add (_split_reels, false);
	add_spacer ();
	_split_streams = new CheckBox (this, _("Write each audio channel to its own stream"));
	add (_split_streams, false);
	_x264_crf_label[0] = add (_("Quality"), true);
	_x264_crf = new wxSlider (this, wxID_ANY, 23, 0, 51, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
	add (_x264_crf, false);
	add_spacer ();
	_x264_crf_label[1] = add (_("0 is best, 51 is worst"), false);
	wxFont font = _x264_crf_label[1]->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_x264_crf_label[1]->SetFont(font);

	add (_("Output file"), true);
	/* Don't warn overwrite here, because on Linux (at least) if we specify a filename like foo
	   the wxFileDialog will check that foo exists, but we will add an extension so we actually
	   need to check if foo.mov (or similar) exists.  I can't find a way to make wxWidgets do this,
	   so disable its check and the caller will have to do it themselves.
	*/
	_file = new FilePickerCtrl (this, _("Select output file"), format_filters[0], false, false);
	_file->SetPath (_initial_name);
	add (_file);

	for (int i = 0; i < FORMATS; ++i) {
		_format->Append (format_names[i]);
	}
	_format->SetSelection (0);

	_x264_crf->Enable (false);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable (false);
	}

	_format->Bind (wxEVT_CHOICE, bind (&ExportVideoFileDialog::format_changed, this));
	_file->Bind (wxEVT_FILEPICKER_CHANGED, bind (&ExportVideoFileDialog::file_changed, this));

	layout ();

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (false);
}

void
ExportVideoFileDialog::format_changed ()
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	_file->SetWildcard (format_filters[_format->GetSelection()]);
	_file->SetPath (_initial_name);
	_x264_crf->Enable (_format->GetSelection() == 1);
	for (int i = 0; i < 2; ++i) {
		_x264_crf_label[i]->Enable (_format->GetSelection() == 1);
	}
	_mixdown->Enable (_format->GetSelection() != 2);
}

boost::filesystem::path
ExportVideoFileDialog::path () const
{
	wxFileName fn (_file->GetPath());
	fn.SetExt (format_extensions[_format->GetSelection()]);
	return wx_to_std (fn.GetFullPath());
}

ExportFormat
ExportVideoFileDialog::format () const
{
	DCPOMATIC_ASSERT (_format->GetSelection() >= 0 && _format->GetSelection() < FORMATS);
	return formats[_format->GetSelection()];
}

bool
ExportVideoFileDialog::mixdown_to_stereo () const
{
	return _mixdown->GetValue ();
}

bool
ExportVideoFileDialog::split_reels () const
{
	return _split_reels->GetValue ();
}

bool
ExportVideoFileDialog::split_streams () const
{
	return _split_streams->GetValue ();
}

int
ExportVideoFileDialog::x264_crf () const
{
	return _x264_crf->GetValue ();
}

void
ExportVideoFileDialog::file_changed ()
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	DCPOMATIC_ASSERT (ok);
	ok->Enable (path().is_absolute());
}
