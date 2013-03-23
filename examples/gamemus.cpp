/**
 * @file   gamemus.cpp
 * @brief  Command-line interface to libgamemusic.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
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

#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <camoto/gamemusic.hpp>
#include <camoto/util.hpp>
#include <camoto/stream_file.hpp>
#include <stdlib.h>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;
namespace gm = camoto::gamemusic;
using namespace camoto;

#define PROGNAME "gamemus"

/*** Return values ***/
// All is good
#define RET_OK                 0
// Bad arguments (missing/invalid parameters)
#define RET_BADARGS            1
// Major error (couldn't open file, etc.)
#define RET_SHOWSTOPPER        2
// More info needed (-t auto didn't work, specify a type)
#define RET_BE_MORE_SPECIFIC   3
// One or more files failed, probably user error (file not found, etc.)
#define RET_NONCRITICAL_FAILURE 4
// Some files failed, but not in a common way (cut off write, disk full, etc.)
#define RET_UNCOMMON_FAILURE   5

/// @see libgamearchive/examples/gamearch.cpp
bool split(const std::string& in, char delim, std::string *out1, std::string *out2)
{
	std::string::size_type iEqualPos = in.find_last_of(delim);
	*out1 = in.substr(0, iEqualPos);
	// Does the destination have a different filename?
	bool bAltDest = iEqualPos != std::string::npos;
	*out2 = bAltDest ? in.substr(iEqualPos + 1) : *out1;
	return bAltDest;
}

/// Open a music file.
/**
 * @param strFilename
 *   Filename of music or instrument file to open.
 *
 * @param typeArg
 *   Name of command line argument used to specify strType.  Shown to user
 *   when strType is invalid to indicate which option was at fault.
 *
 * @param strType
 *   File type, empty string for autodetect.
 *
 * @param pManager
 *   Manager instance.
 *
 * @param bForceOpen
 *   True to force files to be opened, even if they are not of the indicated
 *   file format.
 *
 * @return Shared pointer to MusicReader instance.
 *
 * @throws int on failure (one of the RET_* values)
 */
gm::MusicPtr openMusicFile(const std::string& strFilename,
	const char *typeArg, const std::string& strType, gm::ManagerPtr pManager,
	bool bForceOpen)
{
	stream::input_file_sptr psMusic(new stream::input_file());
	try {
		psMusic->open(strFilename.c_str());
	} catch (stream::open_error& e) {
		std::cerr << "Error opening " << strFilename << ": " << e.what()
			<< std::endl;
		throw RET_SHOWSTOPPER;
	}

	gm::MusicTypePtr pMusicType;
	if (strType.empty()) {
		// Need to autodetect the file format.
		gm::MusicTypePtr pTestType;
		int i = 0;
		while ((pTestType = pManager->getMusicType(i++))) {
			gm::MusicType::Certainty cert = pTestType->isInstance(psMusic);
			switch (cert) {
				case gm::MusicType::DefinitelyNo:
					// Don't print anything (TODO: Maybe unless verbose?)
					break;
				case gm::MusicType::Unsure:
					std::cout << "File could be: " << pTestType->getFriendlyName()
						<< " [" << pTestType->getCode() << "]" << std::endl;
					// If we haven't found a match already, use this one
					if (!pMusicType) pMusicType = pTestType;
					break;
				case gm::MusicType::PossiblyYes:
					std::cout << "File is likely to be: " << pTestType->getFriendlyName()
						<< " [" << pTestType->getCode() << "]" << std::endl;
					// Take this one as it's better than an uncertain match
					pMusicType = pTestType;
					break;
				case gm::MusicType::DefinitelyYes:
					std::cout << "File is definitely: " << pTestType->getFriendlyName()
						<< " [" << pTestType->getCode() << "]" << std::endl;
					pMusicType = pTestType;
					// Don't bother checking any other formats if we got a 100% match
					goto finishTesting;
			}
		}
finishTesting:
		if (!pMusicType) {
			std::cerr << "Unable to automatically determine the file type.  Use "
				"the " << typeArg << " option to manually specify the file format."
				<< std::endl;
			throw RET_BE_MORE_SPECIFIC;
		}
	} else {
		gm::MusicTypePtr pTestType(pManager->getMusicTypeByCode(strType));
		if (!pTestType) {
			std::cerr << "Unknown file type given to " << typeArg << ": " << strType
				<< std::endl;
			throw RET_BADARGS;
		}
		pMusicType = pTestType;
	}

	assert(pMusicType != NULL);

	// Check to see if the file is actually in this format
	if (!pMusicType->isInstance(psMusic)) {
		if (bForceOpen) {
			std::cerr << "Warning: " << strFilename << " is not a "
				<< pMusicType->getFriendlyName() << ", open forced." << std::endl;
		} else {
			std::cerr << "Invalid format: " << strFilename << " is not a "
				<< pMusicType->getFriendlyName() << "\n"
				<< "Use the -f option to try anyway." << std::endl;
			throw RET_BE_MORE_SPECIFIC;
		}
	}

	// See if the format requires any supplemental files
	camoto::SuppFilenames suppList = pMusicType->getRequiredSupps(psMusic, strFilename);
	camoto::SuppData suppData;
	if (suppList.size() > 0) {
		for (camoto::SuppFilenames::iterator i = suppList.begin(); i != suppList.end(); i++) {
			stream::file_sptr suppStream(new stream::file());
			try {
				suppStream->open(i->second);
				suppData[i->first] = suppStream;
			} catch (stream::open_error& e) {
				std::cerr << "Error opening supplemental file " << i->second << ": " << e.what()
					<< std::endl;
				throw RET_SHOWSTOPPER;
			}
		}
	}

	// Open the file
	gm::MusicPtr pMusic = pMusicType->read(psMusic, suppData);
	assert(pMusic);

	return pMusic;
}

int main(int iArgC, char *cArgV[])
{
#ifdef __GLIBCXX__
	// Set a better exception handler
	std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif

	// Disable stdin/printf/etc. sync for a speed boost
	std::ios_base::sync_with_stdio(false);

	// Declare the supported options.
	po::options_description poActions("Actions");
	poActions.add_options()
		("list,l",
			"list all events in the song")

		("list-instruments,i",
			"list all instruments in the song")

		("convert,c", po::value<std::string>(),
			"convert the song to another file format")

		("newinst,n", po::value<std::string>(),
			"override the instrument bank used by subsequent conversions (-c)")

		("repeat-instruments,r", po::value<int>(),
			"repeat the instrument bank until there are this many valid instruments")

		("tag-title", po::value<std::string>(),
			"set the title tag for the next output file, blank to remove tag")

		("tag-artist", po::value<std::string>(),
			"set the artist tag for the next output file, blank to remove tag")

		("tag-comment", po::value<std::string>(),
			"set the comment tag for the next output file, blank to remove tag")
	;

	po::options_description poOptions("Options");
	poOptions.add_options()
		("type,t", po::value<std::string>(),
			"specify the music file format (default is autodetect)")
		("script,s",
			"format output suitable for script parsing")
		("force,f",
			"force open even if the file is not in the given format")
		("list-types",
			"list available input/output file formats")
		("no-pitchbends,b",
			"don't use pitchbends with -c")
		("force-opl3,3",
			"force OPL3 mode (18 channels) with -c")
		("force-opl2,2",
			"force OPL2 mode (11 channels) with -c")
	;

	po::options_description poHidden("Hidden parameters");
	poHidden.add_options()
		("music", "music file to manipulate")
		("help", "produce help message")
	;

	po::options_description poVisible("");
	poVisible.add(poActions).add(poOptions);

	po::options_description poComplete("Parameters");
	poComplete.add(poActions).add(poOptions).add(poHidden);
	po::variables_map mpArgs;

	std::string strFilename;
	std::string strType;

	// Get the format handler for this file format
	gm::ManagerPtr pManager(gm::getManager());

	bool bScript = false; // show output suitable for script parsing?
	bool bForceOpen = false; // open anyway even if not in given format?
	bool bUsePitchbends = true; // pass pitchbend events with -c
	bool bForceOPL2 = false; // force OPL2 mode?
	bool bForceOPL3 = false; // force OPL3 mode?
	try {
		po::parsed_options pa = po::parse_command_line(iArgC, cArgV, poComplete);

		// Parse the global command line options
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.empty()) {
				// If we've already got a filename, complain that a second one
				// was given (probably a typo.)
				if (!strFilename.empty()) {
					std::cerr << "Error: unexpected extra parameter (multiple "
						"filenames given?!)" << std::endl;
					return RET_BADARGS;
				}
				assert(i->value.size() > 0);  // can't have no values with no name!
				strFilename = i->value[0];
			} else if (i->string_key.compare("help") == 0) {
				std::cout <<
					"Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>\n"
					"This program comes with ABSOLUTELY NO WARRANTY.  This is free software,\n"
					"and you are welcome to change and redistribute it under certain conditions;\n"
					"see <http://www.gnu.org/licenses/> for details.\n"
					"\n"
					"Utility to manipulate music files used by games.\n"
					"Build date " __DATE__ " " __TIME__ << "\n"
					"\n"
					"Usage: gamemus [options] <infile> [actions...]\n" << poVisible << "\n"
					"--convert requires a filetype prefix, e.g. raw-rdos:out.raw.  Also works\n"
					"with --newinst if the file type cannot be autodetected."
					<< std::endl;
				return RET_OK;
			} else if (i->string_key.compare("list-types") == 0) {
				gm::MusicTypePtr nextType;
				int i = 0;
				while ((nextType = pManager->getMusicType(i++))) {
					std::string code = nextType->getCode();
					std::cout << code;
					int len = code.length();
					if (len < 20) std::cout << std::string(20-len, ' ');
					std::string desc = nextType->getFriendlyName();
					std::cout << ' ' << desc;
					std::vector<std::string> exts = nextType->getFileExtensions();
					if (exts.size() > 0) {
						len = desc.length();
						if (len < 40) std::cout << std::string(40-len, ' ');
						std::cout << " [";
						std::string sep;
						for (std::vector<std::string>::iterator i = exts.begin();
							i != exts.end();
							i++
						) {
							std::cout << sep << "*." << *i;
							if (sep.empty()) sep = "; ";
						}
						std::cout << ']';
					}
					std::cout << '\n';
				}
				return RET_OK;
			} else if (
				(i->string_key.compare("t") == 0) ||
				(i->string_key.compare("type") == 0)
			) {
				if (i->value.size() == 0) {
					std::cerr << PROGNAME ": --type (-t) requires a parameter."
						<< std::endl;
					return RET_BADARGS;
				}
				strType = i->value[0];
			} else if (
				(i->string_key.compare("s") == 0) ||
				(i->string_key.compare("script") == 0)
			) {
				bScript = true;
			} else if (
				(i->string_key.compare("f") == 0) ||
				(i->string_key.compare("force") == 0)
			) {
				bForceOpen = true;
			} else if (
				(i->string_key.compare("b") == 0) ||
				(i->string_key.compare("no-pitchbends") == 0)
			) {
				bUsePitchbends = false;
			} else if (
				(i->string_key.compare("3") == 0) ||
				(i->string_key.compare("force-opl3") == 0)
			) {
				bForceOPL3 = true;
			} else if (
				(i->string_key.compare("2") == 0) ||
				(i->string_key.compare("force-opl2") == 0)
			) {
				bForceOPL2 = true;
			}
		}

		if ((bForceOPL2 == true) && (bForceOPL3 == true)) {
			std::cerr << "Error: can't force OPL2 and OPL3 at the same time!" << std::endl;
			return RET_BADARGS;
		}

		if (strFilename.empty()) {
			std::cerr << "Error: no filename given.  Use --help for help." << std::endl;
			return RET_BADARGS;
		}
		if (!bScript) std::cout << "Opening " << strFilename << " as type "
			<< (strType.empty() ? "<autodetect>" : strType) << std::endl;

		gm::MusicPtr pMusic;
		try {
			pMusic = openMusicFile(strFilename, "-t/--type", strType, pManager, bForceOpen);
		} catch (int ret) {
			return ret;
		}

		if (bForceOPL2 || bForceOPL3) {
			gm::ConfigurationEvent *ev = new gm::ConfigurationEvent();
			gm::EventPtr gev(ev);
			ev->channel = 0; // global
			ev->sourceChannel = 0;
			ev->configType = gm::ConfigurationEvent::EnableOPL3;
			ev->value = bForceOPL3 ? 1 : 0;
			gm::EventVector::iterator i = pMusic->events->begin();
			i = pMusic->events->insert(i, gev);
			i++; // skip the one we just inserted
			for (; i != pMusic->events->end(); i++) {
				gm::ConfigurationEvent *ev = dynamic_cast<gm::ConfigurationEvent *>(i->get());
				if (ev) {
					switch (ev->configType) {
						case gm::ConfigurationEvent::EnableOPL3:
							// Got an OPL3 mode change event, delete it
							i = pMusic->events->erase(i);
							break;
						case gm::ConfigurationEvent::EnableDeepTremolo: {
							if (bForceOPL3) {
								// Duplicate this event for the other chip
								gm::ConfigurationEvent *ev2 = new gm::ConfigurationEvent();
								gm::EventPtr gev2(ev2);
								ev2->channel = 0; // global
								ev2->sourceChannel = 0;
								ev2->configType = gm::ConfigurationEvent::EnableDeepTremolo;
								ev2->value = 2 | ev->value; // 2| == chip index 1
								i = pMusic->events->insert(i+1, gev2);
							}
							break;
						}
						case gm::ConfigurationEvent::EnableDeepVibrato: {
							if (bForceOPL3) {
								// Duplicate this event for the other chip
								gm::ConfigurationEvent *ev2 = new gm::ConfigurationEvent();
								gm::EventPtr gev2(ev2);
								ev2->channel = 0; // global
								ev2->sourceChannel = 0;
								ev2->configType = gm::ConfigurationEvent::EnableDeepVibrato;
								ev2->value = 2 | ev->value; // 2| == chip index 1
								i = pMusic->events->insert(i+1, gev2);
							}
							break;
						}
						case gm::ConfigurationEvent::EnableRhythm:
							// Not sure how to deal with this yet
							break;
						case gm::ConfigurationEvent::EnableWaveSel:
							// Always enabled on OPL3, no need to do anything
							break;
					}
				}
			}
		}

		// Run through the actions on the command line
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.compare("list") == 0) {

				int n = 0;
				for (gm::EventVector::const_iterator i = pMusic->events->begin(); i != pMusic->events->end(); i++, n++) {
					if (bScript) std::cout << "index=" << n << ";";
					else std::cout << n << ": ";
					std::cout << (*i)->getContent() << "\n";
				}
				if (!bScript) std::cout << n << " events listed." << std::endl;

			} else if (i->string_key.compare("convert") == 0) {
				std::string strOutFile;
				std::string strOutType;
				if (!split(i->value[0], ':', &strOutType, &strOutFile)) {
					std::cerr << "-c/--convert requires a type and a filename, "
						"e.g. -c raw-rdos:out.raw" << std::endl;
					return RET_BADARGS;
				}

				if (!pMusic->patches) {
					std::cerr << "Unable to convert, the source file had no "
						"instruments!  Please supply some with -n." << std::endl;
					return RET_SHOWSTOPPER;
				}

				stream::output_file_sptr psMusicOut(new stream::output_file());
				try {
					psMusicOut->create(strOutFile.c_str());
				} catch (stream::open_error& e) {
					std::cerr << "Error creating " << strOutFile << ": " << e.what()
						<< std::endl;
					throw RET_SHOWSTOPPER;
				}

				gm::MusicTypePtr pMusicOutType(pManager->getMusicTypeByCode(strOutType));
				if (!pMusicOutType) {
					std::cerr << "Unknown file type given to -c/--convert: " << strOutType
						<< std::endl;
					return RET_BADARGS;
				}

				assert(pMusicOutType != NULL);

				// TODO: figure out whether we need to open any supplemental files
				// (e.g. Vinyl will need to create an external instrument file, whereas
				// Kenslab has one but won't need to use it for this.)
				camoto::SuppData suppData;
				unsigned int flags = 0;

				if (!bUsePitchbends) flags |= gm::MusicType::IntegerNotesOnly;

				try {
					pMusicOutType->write(psMusicOut, suppData, pMusic, flags);
					std::cout << "Wrote " << strOutFile << " as " << strOutType << std::endl;
				} catch (const gm::format_limitation& e) {
					std::cerr << PROGNAME ": Unable to write this song in format "
						<< strOutType << " - " << e.what() << std::endl;
					// Delete empty output file
					psMusicOut->remove();
					return RET_UNCOMMON_FAILURE;
				}

			} else if (i->string_key.compare("list-instruments") == 0) {
				std::cout << "Listing " << pMusic->patches->size() << " instruments:\n";
				unsigned int j = 0;
				for (gm::PatchBank::const_iterator
					     i = pMusic->patches->begin(); i != pMusic->patches->end(); i++, j++
				) {
					std::cout << " #" << j << ": ";
					gm::OPLPatchPtr oplNext = boost::dynamic_pointer_cast<gm::OPLPatch>(*i);
					if (oplNext) {
						std::cout << "OPL " << oplNext;
					} else {
						gm::MIDIPatchPtr midiNext = boost::dynamic_pointer_cast<gm::MIDIPatch>(*i);
						if (midiNext) {
							std::cout << "MIDI ";
							if (midiNext->percussion) {
								std::cout << "percussion note " << (int)midiNext->midiPatch;
							} else {
								std::cout << "patch " << (int)midiNext->midiPatch;
							}
						} else {
							std::cout << "<unknown patch type>";
						}
					}
					if (!(*i)->name.empty()) {
						std::cout << " \"" << (*i)->name << '"';
					}
					std::cout << "\n";
				}

			} else if (i->string_key.compare("newinst") == 0) {
				if (i->value[0].empty()) {
					std::cerr << "-n/--newinst requires filename" << std::endl;
					return RET_BADARGS;
				}
				std::string strInstFile, strInstType;
				if (!split(i->value[0], ':', &strInstType, &strInstFile)) {
					strInstFile = i->value[0];
					strInstType.clear();
				}

				gm::MusicPtr pInst;
				try {
					pInst = openMusicFile(strInstFile, "-n/--newinst", strInstType, pManager, bForceOpen);
				} catch (int ret) {
					return ret;
				}

				if (!pInst->patches) {
					std::cerr << "Replacement instrument file given with -n/--newinst "
						"has no instruments" << std::endl;
					return RET_BADARGS;
				}

				unsigned int newCount = pInst->patches->size();
				unsigned int oldCount;
				if (pMusic->patches) {
					oldCount = pMusic->patches->size();
				} else {
					oldCount = 0;
				}
				if (newCount < oldCount) {
					std::cout << "Warning: " << strInstFile << " has less instruments "
						"than the original song! (" << newCount << " vs " << oldCount
						<< ")" << std::endl;
					// Recycle the new instruments until there are the same as there were
					// originally.
					pInst->patches->reserve(oldCount);
					for (unsigned int i = newCount; i < oldCount; i++) {
						unsigned int srcInst = (i - newCount) % newCount;
						std::cout << " > Reusing new instrument #"
							<< srcInst + 1 << " as #" << i + 1 << "/"
							<< oldCount << std::endl;
						// NOTE: This will store the same instrument twice, instead of
						// making a copy - i.e. modifying one will change the other.
						// It's OK here since we won't be futher changing instruments.
						pInst->patches->push_back(pInst->patches->at(srcInst));
					}
				}

				// Repeat the instruments to make sure there are at least the value
				// given to --repeat-instruments.
				pMusic->patches = pInst->patches;

				std::cout << "Loaded replacement instruments from " << strInstFile
					<< std::endl;

			} else if (i->string_key.compare("repeat-instruments") == 0) {
				if (!pMusic->patches) {
					std::cerr << "No instruments available to repeat with "
						"-r/--repeat-instruments" << std::endl;
					return RET_BADARGS;
				}

				unsigned int instrumentRepeat = strtod(i->value[0].c_str(), NULL);

				unsigned int oldCount = pMusic->patches->size();
				pMusic->patches->reserve(instrumentRepeat);
				for (unsigned int i = oldCount; i < instrumentRepeat; i++) {
					unsigned int srcInst = (i - oldCount) % oldCount;
					std::cout << " > Repeating instrument #"
						<< srcInst + 1 << " as #" << i + 1 << "/"
						<< instrumentRepeat << std::endl;
					// NOTE: This will store the same instrument twice, instead of
					// making a copy - i.e. modifying one will change the other.
					// It's OK here since we won't be futher changing instruments.
					pMusic->patches->push_back(pMusic->patches->at(srcInst));
				}

			} else if (
				(i->string_key.compare("tag-title") == 0)
			) {
				if (i->value.size() == 0) {
					std::cerr << PROGNAME ": --tag-title requires a parameter."
						<< std::endl;
					return RET_BADARGS;
				}
				if (i->value[0].empty()) {
					pMusic->metadata.erase(Metadata::Title);
				} else {
					pMusic->metadata[Metadata::Title] = i->value[0];
				}

			} else if (
				(i->string_key.compare("tag-artist") == 0)
			) {
				if (i->value.size() == 0) {
					std::cerr << PROGNAME ": --tag-artist requires a parameter."
						<< std::endl;
					return RET_BADARGS;
				}
				if (i->value[0].empty()) {
					pMusic->metadata.erase(Metadata::Author);
				} else {
					pMusic->metadata[Metadata::Author] = i->value[0];
				}

			} else if (
				(i->string_key.compare("tag-comment") == 0)
			) {
				if (i->value.size() == 0) {
					std::cerr << PROGNAME ": --tag-comment requires a parameter."
						<< std::endl;
					return RET_BADARGS;
				}
				if (i->value[0].empty()) {
					pMusic->metadata.erase(Metadata::Description);
				} else {
					pMusic->metadata[Metadata::Description] = i->value[0];
				}

			// Ignore --type/-t
			} else if (i->string_key.compare("type") == 0) {
			} else if (i->string_key.compare("t") == 0) {
			// Ignore --script/-s
			} else if (i->string_key.compare("script") == 0) {
			} else if (i->string_key.compare("s") == 0) {
			// Ignore --force/-f
			} else if (i->string_key.compare("force") == 0) {
			} else if (i->string_key.compare("f") == 0) {
			}
		} // for (all command line elements)

	} catch (const po::unknown_option& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;
	} catch (const po::invalid_command_line_syntax& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;
	} catch (const stream::error& e) {
		std::cerr << PROGNAME ": I/O error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	} catch (const std::exception& e) {
		std::cerr << PROGNAME ": Unexpected error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	}

	return RET_OK;
}
