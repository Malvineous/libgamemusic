/**
 * @file  common-attributes.hpp
 * @brief Attribute-related common code shared between Camoto example programs.
 *
 * Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>
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

/// List all the attributes in the object.
void listAttributes(const camoto::HasAttributes *obj, bool bScript)
{
	auto attributes = obj->attributes();
	std::cout << (bScript ? "attribute_count=" : "Number of attributes: ")
		<< attributes.size() << "\n";
	int attrNum = 0;
	for (auto& a : attributes) {

		if (bScript) std::cout << "attribute" << attrNum << "_name=";
		else std::cout << "Attribute " << attrNum+1 << ": ";
		std::cout << a.name << "\n";

		if (bScript) std::cout << "attribute" << attrNum << "_desc=";
		else std::cout << "  Description: ";
		std::cout << a.desc << "\n";

		if (bScript) std::cout << "attribute" << attrNum << "_type=";
		else std::cout << "  Type: ";
		switch (a.type) {

			case camoto::Attribute::Type::Integer: {
				std::cout << (bScript ? "int" : "Integer value") << "\n";

				if (bScript) std::cout << "attribute" << attrNum << "_value=";
				else std::cout << "  Current value: ";
				std::cout << a.integerValue << "\n";

				if (bScript) {
					std::cout << "attribute" << attrNum << "_min=" << a.integerMinValue
						<< "\nattribute" << attrNum << "_max=" << a.integerMaxValue;
				} else {
					std::cout << "  Range: ";
					if ((a.integerMinValue == 0) && (a.integerMaxValue == 0)) {
						std::cout << "[unlimited]";
					} else {
						std::cout << a.integerMinValue << " to " << a.integerMaxValue;
					}
				}
				std::cout << "\n";
				break;
			}

			case camoto::Attribute::Type::Enum: {
				std::cout << (bScript ? "enum" : "Item from list") << "\n";

				if (bScript) std::cout << "attribute" << attrNum << "_value=";
				else std::cout << "  Current value: ";
				if (a.enumValue >= a.enumValueNames.size()) {
					if (bScript) {
						std::cout << "error";
					} else {
						std::cout << "[out of range: " << a.enumValue << "]";
					}
				} else {
					if (bScript) std::cout << a.enumValue;
					else std::cout << "[" << a.enumValue << "] "
						<< a.enumValueNames[a.enumValue];
				}
				std::cout << "\n";

				if (bScript) std::cout << "attribute" << attrNum
					<< "_choice_count=" << a.enumValueNames.size() << "\n";

				int option = 0;
				for (auto& j : a.enumValueNames) {
					if (bScript) {
						std::cout << "attribute" << attrNum << "_choice" << option
							<< "=";
					} else {
						std::cout << "  Allowed value " << option << ": ";
					}
					std::cout << j << "\n";
					option++;
				}
				break;
			}

			case camoto::Attribute::Type::Filename: {
				std::cout << (bScript ? "filename" : "Filename") << "\n";

				if (bScript) std::cout << "attribute" << attrNum << "_value=";
				else std::cout << "  Current value: ";
				std::cout << a.filenameValue << "\n";

				if (bScript) std::cout << "attribute" << attrNum
					<< "_filespec=";
				else std::cout << "  Valid files: ";
				bool first = true;
				for (auto& fs : a.filenameSpec) {
					if (!first) std::cout << (bScript ? ":" : "; ");
					std::cout << fs;
					first = false;
				}
				std::cout << "\n";
				break;
			}

			case camoto::Attribute::Type::Text:
				std::cout << (bScript ? "text" : "Text") << "\n";

				if (bScript) std::cout << "attribute" << attrNum << "_value=";
				else std::cout << "  Current value: ";
				std::cout << a.textValue << "\n";

				if (bScript) std::cout << "attribute" << attrNum
					<< "_maxlen=";
				else std::cout << "  Maximum length: ";
				std::cout << a.textMaxLength << "\n";
				break;

			case camoto::Attribute::Type::Image:
				std::cout << (bScript ? "image" : "Image") << "\n";

				if (bScript) std::cout << "attribute" << attrNum << "_value=";
				else std::cout << "  Current value: ";
				std::cout << a.imageIndex << "\n";
				break;

			default:
				std::cout << (bScript ? "unknown" : "Unknown type (fix this!)\n");
				break;
		}
		attrNum++;
	}
	return;
}

/// Set an attribute
void setAttribute(camoto::HasAttributes *obj, bool bScript,
	unsigned int index, const std::string& value)
{
	auto attributes = obj->attributes();
	if (index >= attributes.size()) {
		throw camoto::error(createString("Index " << index << " is out of range ("
			<< attributes.size() << " attributes available)."));
	}
	auto& a = attributes[index];
	switch (a.type) {
		case camoto::Attribute::Type::Integer:
		case camoto::Attribute::Type::Enum:
		case camoto::Attribute::Type::Image: {
			long val = strtol(value.c_str(), nullptr, 0);
			obj->attribute(index, val);
			break;
		}

		case camoto::Attribute::Type::Filename:
		case camoto::Attribute::Type::Text: {
			obj->attribute(index, value);
			break;
		}

		default:
			throw camoto::error(createString("Attribute " << index
				<< " has an unknown type (fix this!)"));
	}
	return;
}
