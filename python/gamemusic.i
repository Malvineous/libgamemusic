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
%module(directors="1") gamemusic

%include <std_string.i>
%include <std_map.i>
%include <exception.i>
%include <boost_shared_ptr.i>

%shared_ptr(std::istream)
%shared_ptr(camoto::gamemusic::Manager)
%shared_ptr(camoto::gamemusic::MusicType)
%shared_ptr(camoto::gamemusic::MusicReader)
%shared_ptr(camoto::gamemusic::MusicWriter)
%shared_ptr(camoto::gamemusic::MusicReader_GenericOPL)
%shared_ptr(camoto::gamemusic::MusicWriter_GenericOPL)
%shared_ptr(camoto::gamemusic::EventHandler)
%shared_ptr(camoto::gamemusic::Patch)
%shared_ptr(camoto::gamemusic::OPLPatch)
%shared_ptr(camoto::gamemusic::MIDIPatch)
%shared_ptr(camoto::gamemusic::Patch)
%shared_ptr(camoto::gamemusic::PatchBank)
%shared_ptr(camoto::gamemusic::Event)
%shared_ptr(camoto::gamemusic::TempoEvent)
%shared_ptr(camoto::gamemusic::NoteOnEvent)
%shared_ptr(camoto::gamemusic::NoteOffEvent)
%shared_ptr(camoto::gamemusic::PitchbendEvent)
%shared_ptr(camoto::gamemusic::ConfigurationEvent)

%apply          char      {   int8_t }
%apply unsigned char      {  uint8_t }
%apply          short     {  int16_t }
%apply unsigned short     { uint16_t }
%apply          int       {  int32_t }
%apply unsigned int       { uint32_t }
%apply          long long {  int64_t }
%apply unsigned long long { uint64_t }

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

%feature("director");

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

	template<class F, class T>
	boost::shared_ptr<T> shared_ptr_cast(boost::shared_ptr<F> f)
	{
		return boost::dynamic_pointer_cast<T>(f);
	}

	template<class F, class T>
	boost::shared_ptr<T> shared_ptr_always_cast(boost::shared_ptr<F> f)
	{
		boost::shared_ptr<T> out = boost::dynamic_pointer_cast<T>(f);
		assert(out);
		return out;
	}

%}

class std::istream;
%include <camoto/types.hpp>
%include <camoto/gamemusic/events.hpp>
%include <camoto/gamemusic/patch.hpp>
%include <camoto/gamemusic/patch-opl.hpp>
%include <camoto/gamemusic/patch-midi.hpp>
%include <camoto/gamemusic/patchbank.hpp>
%include <camoto/gamemusic/music.hpp>
%include <camoto/gamemusic/musictype.hpp>
%include <camoto/gamemusic/manager.hpp>
%include <camoto/gamemusic.hpp>
%include <camoto/gamemusic/mus-generic-opl.hpp>

camoto::istream_sptr openFile(const char *filename);

template<class F, class T> boost::shared_ptr<T> shared_ptr_cast(boost::shared_ptr<F> f);
%template(get_event_as_tempo)         shared_ptr_cast<camoto::gamemusic::Event, camoto::gamemusic::TempoEvent>;
%template(get_event_as_noteon)        shared_ptr_cast<camoto::gamemusic::Event, camoto::gamemusic::NoteOnEvent>;
%template(get_event_as_noteoff)       shared_ptr_cast<camoto::gamemusic::Event, camoto::gamemusic::NoteOffEvent>;
%template(get_event_as_pitchbend)     shared_ptr_cast<camoto::gamemusic::Event, camoto::gamemusic::PitchbendEvent>;
%template(get_event_as_configuration) shared_ptr_cast<camoto::gamemusic::Event, camoto::gamemusic::ConfigurationEvent>;

%template(get_patch_as_opl)  shared_ptr_cast<camoto::gamemusic::Patch, camoto::gamemusic::OPLPatch>;
%template(get_patch_as_midi) shared_ptr_cast<camoto::gamemusic::Patch, camoto::gamemusic::MIDIPatch>;

template<class F, class T> boost::shared_ptr<T> shared_ptr_always_cast(boost::shared_ptr<F> f);
%template(get_EventHandler_from_MusicWriter_GenericOPL) shared_ptr_always_cast<camoto::gamemusic::EventHandler, camoto::gamemusic::MusicWriter_GenericOPL>;

namespace camoto {
namespace gamemusic {

%template(MP_SUPPDATA) ::std::map<E_SUPPTYPE, SuppItem>;

}
}
