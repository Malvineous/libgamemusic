/**
 * @file   gamemusic.i
 * @brief  SWIG configuration file for libgamemusic.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
%module gamemusic

%include <std_string.i>
%include <std_map.i>
%include <exception.i>
%include <boost_shared_ptr.i>

%shared_ptr(std::istream)
%shared_ptr(camoto::gamemusic::Manager)
%shared_ptr(camoto::gamemusic::MusicType)
%shared_ptr(camoto::gamemusic::MusicReader)
%shared_ptr(camoto::gamemusic::MusicWriter)
%shared_ptr(camoto::gamemusic::Patch)
%shared_ptr(camoto::gamemusic::PatchBank)
%shared_ptr(camoto::gamemusic::Event)
%shared_ptr(camoto::gamemusic::TempoEvent)
%shared_ptr(camoto::gamemusic::NoteOnEvent)
%shared_ptr(camoto::gamemusic::NoteOffEvent)
%shared_ptr(camoto::gamemusic::PitchbendEvent)
%shared_ptr(camoto::gamemusic::ConfigurationEvent)

%exception {
	try {
		$action
	} catch (const std::ios::failure& e) {
		SWIG_exception(SWIG_IOError, e.what());
	} catch (const std::exception& e) {
		SWIG_exception(SWIG_RuntimeError, e.what());
	}
}

#%rename(getManager) camoto::gamemusic::getManager;

%{
	#include <fstream>
	#include <boost/pointer_cast.hpp>
	#include <camoto/gamemusic.hpp>

	using namespace camoto;
	using namespace camoto::gamemusic;

	istream_sptr openFile(const char *filename)
	{
		std::ifstream *f = new std::ifstream(filename);
		return istream_sptr(f);
	}
	
	template<class T>
	boost::shared_ptr<T> get_event_as(EventPtr ev)
	{
		return boost::dynamic_pointer_cast<T>(ev);
	}
%}

class std::istream;
%include <camoto/types.hpp>
%include <camoto/gamemusic/events.hpp>
%include <camoto/gamemusic/patchbank.hpp>
%include <camoto/gamemusic/music.hpp>
%include <camoto/gamemusic/musictype.hpp>
%include <camoto/gamemusic/manager.hpp>
%include <camoto/gamemusic.hpp>

camoto::istream_sptr openFile(const char *filename);

template<class T> boost::shared_ptr<T> get_event_as(camoto::gamemusic::EventPtr ev);
%template(get_event_as_tempo) get_event_as<camoto::gamemusic::TempoEvent>;

namespace camoto {
namespace gamemusic {

%template(MP_SUPPDATA) ::std::map<E_SUPPTYPE, SuppItem>;

}
}
