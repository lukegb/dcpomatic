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

#ifndef DCPOMATIC_KDM_WITH_METADATA_H
#define DCPOMATIC_KDM_WITH_METADATA_H

#ifdef DCPOMATIC_VARIANT_SWAROOP
#include "encrypted_ecinema_kdm.h"
#endif
#include <dcp/encrypted_kdm.h>
#include <dcp/name_format.h>
#include <boost/shared_ptr.hpp>

class Cinema;

class KDMWithMetadata
{
public:
	KDMWithMetadata (dcp::NameFormat::Map const& name_values, boost::shared_ptr<Cinema> cinema)
		: _name_values (name_values)
		, _cinema (cinema)
	{}

	virtual ~KDMWithMetadata () {}

	virtual std::string kdm_as_xml () const = 0;
	virtual void kdm_as_xml (boost::filesystem::path out) const = 0;

	dcp::NameFormat::Map const& name_values () const {
		return _name_values;
	}

	boost::optional<std::string> get (char k) const;

	boost::shared_ptr<Cinema> cinema () const {
		return _cinema;
	}

private:
	dcp::NameFormat::Map _name_values;
	boost::shared_ptr<Cinema> _cinema;
};


typedef boost::shared_ptr<KDMWithMetadata> KDMWithMetadataPtr;


int write_files (
	std::list<KDMWithMetadataPtr> screen_kdms, boost::filesystem::path directory,
	dcp::NameFormat name_format, dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	);


void make_zip_file (std::list<KDMWithMetadataPtr> kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values);


std::list<std::list<KDMWithMetadataPtr> > collect (std::list<KDMWithMetadataPtr> kdms);


int write_directories (
		std::list<std::list<KDMWithMetadataPtr> > cinema_kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);


int write_zip_files (
		std::list<std::list<KDMWithMetadataPtr> > cinema_kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);


void email (
		std::list<std::list<KDMWithMetadataPtr> > cinema_kdms,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		std::string cpl_name
		);


template <class T>
class SpecialKDMWithMetadata : public KDMWithMetadata
{
public:
	SpecialKDMWithMetadata (dcp::NameFormat::Map const& name_values, boost::shared_ptr<Cinema> cinema, T k)
		: KDMWithMetadata (name_values, cinema)
		, kdm (k)
	{}

	std::string kdm_as_xml () const {
		return kdm.as_xml ();
	}

	void kdm_as_xml (boost::filesystem::path out) const {
		return kdm.as_xml (out);
	}

	T kdm;
};

typedef SpecialKDMWithMetadata<dcp::EncryptedKDM> DCPKDMWithMetadata;
#ifdef DCPOMATIC_VARIANT_SWAROOP
typedef SpecialKDMWithMetadata<EncryptedECinemaKDM> ECinemaKDMWithMetadata;
#endif

#endif

