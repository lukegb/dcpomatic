/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DCP_CONTENT_H
#define DCPOMATIC_DCP_CONTENT_H

/** @file  src/lib/dcp_content.h
 *  @brief DCPContent class.
 */

#include "content.h"
#include <libcxml/cxml.h>
#include <dcp/encrypted_kdm.h>

class DCPContentProperty
{
public:
	static int const NEEDS_KDM;
	static int const NEEDS_ASSETS;
	static int const REFERENCE_VIDEO;
	static int const REFERENCE_AUDIO;
	static int const REFERENCE_TEXT;
	static int const NAME;
	static int const TEXTS;
	static int const CPL;
};

class ContentPart;

/** @class DCPContent
 *  @brief An existing DCP used as input.
 */
class DCPContent : public Content
{
public:
	DCPContent (boost::filesystem::path p);
	DCPContent (cxml::ConstNodePtr, int version);

	std::shared_ptr<DCPContent> shared_from_this () {
		return std::dynamic_pointer_cast<DCPContent> (Content::shared_from_this ());
	}

	std::shared_ptr<const DCPContent> shared_from_this () const {
		return std::dynamic_pointer_cast<const DCPContent> (Content::shared_from_this ());
	}

	dcpomatic::DCPTime full_length (std::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime approximate_length () const;

	void examine (std::shared_ptr<const Film> film, std::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	std::string identifier () const;
	void take_settings_from (std::shared_ptr<const Content> c);

	void set_default_colour_conversion ();
	std::list<dcpomatic::DCPTime> reel_split_points (std::shared_ptr<const Film> film) const;

	std::vector<boost::filesystem::path> directories () const;

	bool encrypted () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _encrypted;
	}

	void add_kdm (dcp::EncryptedKDM);
	void add_ov (boost::filesystem::path ov);

	boost::optional<dcp::EncryptedKDM> kdm () const {
		return _kdm;
	}

	bool can_be_played () const;
	bool needs_kdm () const;
	bool needs_assets () const;

	void set_reference_video (bool r);

	bool reference_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_video;
	}

	bool can_reference_video (std::shared_ptr<const Film> film, std::string &) const;

	void set_reference_audio (bool r);

	bool reference_audio () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_audio;
	}

	bool can_reference_audio (std::shared_ptr<const Film> film, std::string &) const;

	void set_reference_text (TextType type, bool r);

	/** @param type Original type of texts in the DCP.
	 *  @return true if these texts are to be referenced.
	 */
	bool reference_text (TextType type) const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_text[static_cast<int>(type)];
	}

	bool can_reference_text (std::shared_ptr<const Film> film, TextType type, std::string &) const;

	void set_cpl (std::string id);

	boost::optional<std::string> cpl () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _cpl;
	}

	std::string name () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _name;
	}

	bool three_d () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _three_d;
	}

	boost::optional<dcp::ContentKind> content_kind () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _content_kind;
	}

	dcp::Standard standard () const {
		boost::mutex::scoped_lock lm (_mutex);
		DCPOMATIC_ASSERT (_standard);
		return _standard.get ();
	}

	std::map<dcp::Marker, dcpomatic::ContentTime> markers () const {
		return _markers;
	}

	bool kdm_timing_window_valid () const;

	Resolution resolution () const;

	std::vector<dcp::Rating> ratings () const {
		return _ratings;
	}

	std::vector<std::string> content_versions () const {
		return _content_versions;
	}

private:
	friend struct reels_test5;

	void add_properties (std::shared_ptr<const Film> film, std::list<UserProperty>& p) const;

	void read_directory (boost::filesystem::path);
	void read_sub_directory (boost::filesystem::path);
	std::list<dcpomatic::DCPTimePeriod> reels (std::shared_ptr<const Film> film) const;
	bool can_reference (
		std::shared_ptr<const Film> film,
		std::function <bool (std::shared_ptr<const Content>)>,
		std::string overlapping,
		std::string& why_not
		) const;

	std::string _name;
	/** true if our DCP is encrypted */
	bool _encrypted;
	/** true if this DCP needs more assets before it can be played */
	bool _needs_assets;
	boost::optional<dcp::EncryptedKDM> _kdm;
	/** true if _kdm successfully decrypts the first frame of our DCP */
	bool _kdm_valid;
	/** true if the video in this DCP should be included in the output by reference
	 *  rather than by rewrapping.
	 */
	bool _reference_video;
	/** true if the audio in this DCP should be included in the output by reference
	 *  rather than by rewrapping.
	 */
	bool _reference_audio;
	/** true if the texts in this DCP should be included in the output by reference
	 *  rather than by rewrapping.  The types here are the original text types,
	 *  not what they are being used for.
	 */
	bool _reference_text[static_cast<int>(TextType::COUNT)];

	boost::optional<dcp::Standard> _standard;
	boost::optional<dcp::ContentKind> _content_kind;
	bool _three_d;
	/** ID of the CPL to use; older metadata might not specify this: in that case
	 *  just use the only CPL.
	 */
	boost::optional<std::string> _cpl;
	/** List of the lengths of the reels in this DCP */
	std::list<int64_t> _reel_lengths;
	std::map<dcp::Marker, dcpomatic::ContentTime> _markers;
	std::vector<dcp::Rating> _ratings;
	std::vector<std::string> _content_versions;
};

#endif
