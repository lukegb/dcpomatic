/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/audio_analysis.h"
#include "lib/playlist.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/signals2.hpp>

class AudioPlot;
class FilmViewer;
class Film;

class AudioDialog : public wxDialog
{
public:
	AudioDialog (wxWindow* parent, std::shared_ptr<Film> film, std::weak_ptr<FilmViewer> viewer, std::shared_ptr<Content> content = std::shared_ptr<Content>());

	bool Show (bool show = true);

	void set_cursor (boost::optional<dcpomatic::DCPTime> time, boost::optional<float> db);

private:
	void film_change (ChangeType, Film::Property);
	void content_change (ChangeType, int);
	void channel_clicked (wxCommandEvent &);
	void type_clicked (wxCommandEvent &);
	void smoothing_changed ();
	void try_to_load_analysis ();
	void analysis_finished ();
	void setup_statistics ();
	void show_or_hide_channel_checkboxes ();

	std::shared_ptr<AudioAnalysis> _analysis;
	std::weak_ptr<Film> _film;
	std::weak_ptr<FilmViewer> _viewer;
	/** content to analyse, or 0 to analyse all the film's content */
	std::weak_ptr<Content> _content;
	int _channels;
	std::shared_ptr<const Playlist> _playlist;
	wxStaticText* _cursor;
	AudioPlot* _plot;
	wxStaticText* _sample_peak;
	wxStaticText* _true_peak;
	wxStaticText* _integrated_loudness;
	wxStaticText* _loudness_range;
	wxStaticText* _leqm;
	wxCheckBox* _channel_checkbox[MAX_DCP_AUDIO_CHANNELS];
	wxCheckBox* _type_checkbox[AudioPoint::COUNT];
	wxSlider* _smoothing;
	boost::signals2::scoped_connection _film_connection;
	boost::signals2::scoped_connection _film_content_connection;
	boost::signals2::scoped_connection _analysis_finished_connection;
};
