/**
 * @file   gamemus.cpp
 * @brief  Command-line interface to libgamemusic.
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

#include <boost/program_options.hpp>
#include <camoto/gamemusic.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;
namespace gm = camoto::gamemusic;

#define PROGNAME "gamemus"

/*** Return values ***/
// All is good
#define RET_OK                 0
// Bad arguments (missing/invalid parameters)
#define RET_BADARGS            1
// Major error (couldn't open archive file, etc.)
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
gm::MusicReaderPtr openMusicFile(const std::string& strFilename,
	const char *typeArg, const std::string& strType, gm::ManagerPtr pManager,
	bool bForceOpen)
	throw (int)
{
	boost::shared_ptr<std::ifstream> psMusic(new std::ifstream());
	psMusic->exceptions(std::ios::badbit | std::ios::failbit);
	try {
		psMusic->open(strFilename.c_str(), std::ios::in | std::ios::binary);
	} catch (std::ios::failure& e) {
		std::cerr << "Error opening " << strFilename << std::endl;
		#ifdef DEBUG
			std::cerr << "e.what(): " << e.what() << std::endl;
		#endif
		throw RET_SHOWSTOPPER;
	}

	gm::MusicTypePtr pMusicType;
	if (strType.empty()) {
		// Need to autodetect the file format.
		gm::MusicTypePtr pTestType;
		int i = 0;
		while ((pTestType = pManager->getMusicType(i++))) {
			gm::E_CERTAINTY cert = pTestType->isInstance(psMusic);
			switch (cert) {
				case gm::EC_DEFINITELY_NO:
					// Don't print anything (TODO: Maybe unless verbose?)
					break;
				case gm::EC_UNSURE:
					std::cout << "File could be: " << pTestType->getFriendlyName()
						<< " [" << pTestType->getCode() << "]" << std::endl;
					// If we haven't found a match already, use this one
					if (!pMusicType) pMusicType = pTestType;
					break;
				case gm::EC_POSSIBLY_YES:
					std::cout << "File is likely to be: " << pTestType->getFriendlyName()
						<< " [" << pTestType->getCode() << "]" << std::endl;
					// Take this one as it's better than an uncertain match
					pMusicType = pTestType;
					break;
				case gm::EC_DEFINITELY_YES:
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
	gm::MP_SUPPLIST suppList = pMusicType->getRequiredSupps(strFilename);
	gm::MP_SUPPDATA suppData;
	if (suppList.size() > 0) {
		for (gm::MP_SUPPLIST::iterator i = suppList.begin(); i != suppList.end(); i++) {
			try {
				boost::shared_ptr<std::fstream> suppStream(new std::fstream());
				suppStream->exceptions(std::ios::badbit | std::ios::failbit);
				std::cout << "Opening supplemental file " << i->second << std::endl;
				suppStream->open(i->second.c_str(), std::ios::in | std::ios::out | std::ios::binary);
				gm::SuppItem si;
				si.stream = suppStream;
				//si.fnTruncate = boost::bind<void>(truncate, i->second.c_str(), _1);
				suppData[i->first] = si;
			} catch (std::ios::failure e) {
				std::cerr << "Error opening supplemental file " << i->second.c_str() << std::endl;
				#ifdef DEBUG
					std::cerr << "e.what(): " << e.what() << std::endl;
				#endif
				throw RET_SHOWSTOPPER;
			}
		}
	}

	// Open the archive file
	gm::MusicReaderPtr pMusicIn(pMusicType->open(psMusic, suppData));
	assert(pMusicIn);

	return pMusicIn;
}

int main(int iArgC, char *cArgV[])
{
	// Set a better exception handler
	std::set_terminate( __gnu_cxx::__verbose_terminate_handler );

	// Disable stdin/printf/etc. sync for a speed boost
	std::ios_base::sync_with_stdio(false);

	// Declare the supported options.
	po::options_description poActions("Actions");
	poActions.add_options()
		("list,l",
			"list all events in the song")

		("convert,c", po::value<std::string>(),
			"convert the song to another file format")

		("newinst,n", po::value<std::string>(),
			"override the instrument bank used by subsequent conversions (-c)")
	;

	po::options_description poOptions("Options");
	poOptions.add_options()
		("type,t", po::value<std::string>(),
			"specify the music file format (default is autodetect)")
		("script,s",
			"format output suitable for script parsing")
		("force,f",
			"force open even if the file is not in the given format")
		("list-formats,i",
			"list available input/output file formats")
		("no-pitchbends,b",
			"don't use pitchbends with -c")
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
	bool bForceOpen = false; // open anyway even if archive not in given format?
	bool bUsePitchbends = true; // pass pitchbend events with -c
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
					return 1;
				}
				assert(i->value.size() > 0);  // can't have no values with no name!
				strFilename = i->value[0];
			} else if (i->string_key.compare("help") == 0) {
				std::cout <<
					"Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>\n"
					"This program comes with ABSOLUTELY NO WARRANTY.  This is free software,\n"
					"and you are welcome to change and redistribute it under certain conditions;\n"
					"see <http://www.gnu.org/licenses/> for details.\n"
					"\n"
					"Utility to manipulate archive files used by games to store data files.\n"
					"Build date " __DATE__ " " __TIME__ << "\n"
					"\n"
					"Usage: gamemus [options] <infile> [actions...]\n" << poVisible << "\n"
					"--convert requires a filetype prefix, e.g. raw-rdos:out.raw.  Also works\n"
					"with --newinst if the file type cannot be autodetected."
					<< std::endl;
				return RET_OK;
			} else if (i->string_key.compare("list-formats") == 0) {
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
			}
		}

		if (strFilename.empty()) {
			std::cerr << "Error: no filename given.  Use --help for help." << std::endl;
			return RET_BADARGS;
		}
		std::cout << "Opening " << strFilename << " as type "
			<< (strType.empty() ? "<autodetect>" : strType) << std::endl;

		gm::MusicReaderPtr pMusicIn;
		try {
			pMusicIn = openMusicFile(strFilename, "-t/--type", strType, pManager, bForceOpen);
		} catch (int ret) {
			return ret;
		}

		// Default to using the instruments from the source file
		gm::PatchBankPtr instruments = pMusicIn->getPatchBank();

		int iRet = RET_OK;

		// Run through the actions on the command line
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.compare("list") == 0) {
				// We don't need the patch bank, but without it the instruments won't
				// have been loaded and the instrument field in the note-on events will
				// contain invalid data.
				pMusicIn->getPatchBank();

				int i = 0;
				for (; ; i++) {
					gm::EventPtr next = pMusicIn->readNextEvent();
					if (!next) break; // end of song
					std::cout << i << ": " << next->getContent() << "\n";
				}
				std::cout << i << " events listed." << std::endl;

			} else if (i->string_key.compare("convert") == 0) {
				std::string strOutFile;
				std::string strOutType;
				if (!split(i->value[0], ':', &strOutType, &strOutFile)) {
					std::cerr << "-c/--convert requires a type and a filename, "
						"e.g. -c raw-rdos:out.raw" << std::endl;
					return RET_BADARGS;
				}

				if (!instruments) {
					std::cerr << "Unable to convert, the source file had no "
						"instruments!  Please supply some with -n." << std::endl;
					return RET_SHOWSTOPPER;
				}

				boost::shared_ptr<std::ofstream> psMusicOut(new std::ofstream());
				psMusicOut->exceptions(std::ios::badbit | std::ios::failbit);
				try {
					psMusicOut->open(strOutFile.c_str(), std::ios::trunc | std::ios::out | std::ios::binary);
				} catch (std::ios::failure& e) {
					std::cerr << "Error creating output file \"" << strOutFile << "\"" << std::endl;
					#ifdef DEBUG
						std::cerr << "e.what(): " << e.what() << std::endl;
					#endif
					return RET_SHOWSTOPPER;
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
				gm::MP_SUPPDATA suppData;
				boost::shared_ptr<gm::MusicWriter> pMusicOut(pMusicOutType->create(psMusicOut, suppData));
				assert(pMusicOut);

				if (!bUsePitchbends) pMusicOut->setFlags(gm::MusicWriter::IntegerNotesOnly);

				try {
					pMusicOut->setPatchBank(instruments);
				} catch (EBadPatchType) {
					std::cerr << "ERROR: Output format " << strOutType << " does not "
						"support the instruments used in the input file." << std::endl;
					// TODO: delete output file
					return RET_UNCOMMON_FAILURE;
				}
				// Write the output file's header
				pMusicOut->start();

				try {
					for (;;) {
						gm::EventPtr next = pMusicIn->readNextEvent();
						if (!next) break; // end of song
						next->processEvent(pMusicOut.get());
					}
				} catch (...) {
					// Write the output file's footer to prevent an assertion failure
					pMusicOut->finish();
					throw;
				}

				// Write the output file's footer
				pMusicOut->finish();

				std::cout << "Wrote " << strOutFile << " as " << strOutType << std::endl;

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

				gm::MusicReaderPtr pInst;
				try {
					pInst = openMusicFile(strInstFile, "-n/--newinst", strInstType, pManager, bForceOpen);
				} catch (int ret) {
					return ret;
				}

				gm::PatchBankPtr newinst = pInst->getPatchBank();
				int newCount = newinst->getPatchCount();
				int oldCount = instruments->getPatchCount();
				if (newCount < oldCount) {
					std::cout << "Warning: " << strInstFile << " has less instruments "
						"than the original song! (" << newCount << " vs " << oldCount
						<< ")" << std::endl;
					// Recycle the new instruments until there are the same as there were
					// originally.
					newinst->setPatchCount(oldCount);
					for (int i = newCount; i < oldCount; i++) {
						std::cout << " > Reusing new instrument #"
							<< ((i - newCount) % newCount) + 1 << " as #" << i + 1 << "/"
							<< oldCount << std::endl;
						newinst->setPatch(i, newinst->getPatch((i - newCount) % newCount));
					}
				}
				instruments = newinst;

				std::cout << "Loaded replacement instruments from " << strInstFile
					<< std::endl;

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
	} catch (const std::ios::failure& e) {
		std::cerr << PROGNAME ": I/O error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	} catch (const std::exception& e) {
		std::cerr << PROGNAME ": Unexpected error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	}

	return 0;
}
