/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AnnexBItuTT35.h"

#include "Subtitle608.h"
#include "parser/common/ReaderHelperNew.h"
#include "parser/common/functions.h"

namespace
{

using namespace parser::reader;

void parse_ATSC1_data(ReaderHelperNew &reader)
{
  ReaderHelperNewSubLevel s(reader, "ATSC1_data");

  unsigned user_data_type_code;
  {
    Options opt;
    opt.meaningString   = "SCTE/ATSC Reserved";
    opt.meaningMap[3]   = "cc_data() / DTV CC";
    opt.meaningMap[6]   = "bar_data()";
    user_data_type_code = reader.readBits("user_data_type_code", 8, opt);
  }

  if (user_data_type_code == 0x03)
  {
    // cc_data() - CEA-708
    reader.readFlag("process_em_data_flag");
    reader.readFlag("process_cc_data_flag");
    reader.readFlag("additional_data_flag");
    unsigned cc_count = reader.readBits("cc_count", 5);
    reader.readBits("em_data", 8);

    if (reader.nrBytesLeft() > cc_count * 3 + 1)
    {
      // Possibly, the count is wrong.
      if ((reader.nrBytesLeft() - 1) % 3 == 0)
        cc_count = unsigned(reader.nrBytesLeft() - 1) / 3;
    }

    // Now should follow (cc_count * 24 bits of cc_data_pkts)
    // Just display the raw bytes of the payload
    for (unsigned int i = 0; i < cc_count; i++)
    {
      parser::subtitle::sub_608::parse608DataPayloadCCDataPacket(reader);
    }

    reader.readBits("marker_bits", 8);
    // The ATSC marker_bits indicator should be 255

    // ATSC_reserved_user_data
    int idx = 0;
    while (reader.canReadBits(8))
    {
      reader.readBits(parser::formatArray("ATSC_reserved_user_data", idx), 8);
      idx++;
    }
  }
}

} // namespace

namespace parser::subtitle::itutt35
{

void parse_user_data_registered_itu_t_t35(ByteVector &data, TreeItem *root)
{
  ReaderHelperNew reader(data, root, "user_data_registered_itu_t_t35()");
  // For all SEI messages, the emulation prevention is already removed one level up
  reader.disableEmulationPrevention();

  if (data.size() < 2)
    throw std::logic_error("Invalid length of user_data_registered_itu_t_t35 SEI.");

  unsigned itu_t_t35_country_code;
  {
  auto itu_t_t35_country_code_meaning =
      std::vector<std::string>({"Japan",
                                "Albania",
                                "Algeria",
                                "American Samoa",
                                "Germany (Federal Republic of)",
                                "Anguilla",
                                "Antigua and Barbuda",
                                "Argentina",
                                "Ascension (see S. Helena)",
                                "Australia",
                                "Austria",
                                "Bahamas",
                                "Bahrain",
                                "Bangladesh",
                                "Barbados",
                                "Belgium",
                                "Belize",
                                "Benin (Republic of)",
                                "Bermudas",
                                "Bhutan (Kingdom of)",
                                "Bolivia",
                                "Botswana",
                                "Brazil",
                                "British Antarctic Territory",
                                "British Indian Ocean Territory",
                                "British Virgin Islands",
                                "Brunei Darussalam",
                                "Bulgaria",
                                "Myanmar (Union of)",
                                "Burundi",
                                "Byelorussia",
                                "Cameroon",
                                "Canada",
                                "Cape Verde",
                                "Cayman Islands",
                                "Central African Republic",
                                "Chad",
                                "Chile",
                                "China",
                                "Colombia",
                                "Comoros",
                                "Congo",
                                "Cook Islands",
                                "Costa Rica",
                                "Cuba",
                                "Cyprus",
                                "Czech and Slovak Federal Republic",
                                "Cambodia",
                                "Democratic People's Republic of Korea",
                                "Denmark",
                                "Djibouti",
                                "Dominican Republic",
                                "Dominica",
                                "Ecuador",
                                "Egypt",
                                "El Salvador",
                                "Equatorial Guinea",
                                "Ethiopia",
                                "Falkland Islands",
                                "Fiji",
                                "Finland",
                                "France",
                                "French Polynesia",
                                "French Southern and Antarctic Lands",
                                "Gabon",
                                "Gambia",
                                "Germany (Federal Republic of)",
                                "Angola",
                                "Ghana",
                                "Gibraltar",
                                "Greece",
                                "Grenada",
                                "Guam",
                                "Guatemala",
                                "Guernsey",
                                "Guinea",
                                "Guinea-Bissau",
                                "Guayana",
                                "Haiti",
                                "Honduras",
                                "Hongkong",
                                "Hungary (Republic of)",
                                "Iceland",
                                "India",
                                "Indonesia",
                                "Iran (Islamic Republic of)",
                                "Iraq",
                                "Ireland",
                                "Israel",
                                "Italy",
                                "C�te d'Ivoire",
                                "Jamaica",
                                "Afghanistan",
                                "Jersey",
                                "Jordan",
                                "Kenya",
                                "Kiribati",
                                "Korea (Republic of)",
                                "Kuwait",
                                "Lao (People's Democratic Republic)",
                                "Lebanon",
                                "Lesotho",
                                "Liberia",
                                "Libya",
                                "Liechtenstein",
                                "Luxembourg",
                                "Macau",
                                "Madagascar",
                                "Malaysia",
                                "Malawi",
                                "Maldives",
                                "Mali",
                                "Malta",
                                "Mauritania",
                                "Mauritius",
                                "Mexico",
                                "Monaco",
                                "Mongolia",
                                "Montserrat",
                                "Morocco",
                                "Mozambique",
                                "Nauru",
                                "Nepal",
                                "Netherlands",
                                "Netherlands Antilles",
                                "New Caledonia",
                                "New Zealand",
                                "Nicaragua",
                                "Niger",
                                "Nigeria",
                                "Norway",
                                "Oman",
                                "Pakistan",
                                "Panama",
                                "Papua New Guinea",
                                "Paraguay",
                                "Peru",
                                "Philippines",
                                "Poland (Republic of)",
                                "Portugal",
                                "Puerto Rico",
                                "Qatar",
                                "Romania",
                                "Rwanda",
                                "Saint Kitts and Nevis",
                                "Saint Croix",
                                "Saint Helena and Ascension",
                                "Saint Lucia",
                                "San Marino",
                                "Saint Thomas",
                                "Sao Tom� and Principe",
                                "Saint Vincent and the Grenadines",
                                "Saudi Arabia",
                                "Senegal",
                                "Seychelles",
                                "Sierra Leone",
                                "Singapore",
                                "Solomon Islands",
                                "Somalia",
                                "South Africa",
                                "Spain",
                                "Sri Lanka",
                                "Sudan",
                                "Suriname",
                                "Swaziland",
                                "Sweden",
                                "Switzerland",
                                "Syria",
                                "Tanzania",
                                "Thailand",
                                "Togo",
                                "Tonga",
                                "Trinidad and Tobago",
                                "Tunisia",
                                "Turkey",
                                "Turks and Caicos Islands",
                                "Tuvalu",
                                "Uganda",
                                "Ukraine",
                                "United Arab Emirates",
                                "United Kingdom",
                                "United States (ANSI-SCTE 128-1)",
                                "Burkina Faso",
                                "Uruguay",
                                "U.S.S.R.",
                                "Vanuatu",
                                "Vatican City State",
                                "Venezuela",
                                "Viet Nam",
                                "Wallis and Futuna",
                                "Western Samoa",
                                "Yemen (Republic of)",
                                "Yemen (Republic of)",
                                "Yugoslavia",
                                "Zaire",
                                "Zambia",
                                "Zimbabwe",
                                "Unspecified"});
    
    Options opt;
    for (unsigned i = 0; i < itu_t_t35_country_code_meaning.size(); i++)
      opt.meaningMap[i] = itu_t_t35_country_code_meaning.at(i);
    itu_t_t35_country_code = reader.readBits("itu_t_t35_country_code", 8, opt);
  }

  unsigned i = 1;
  if (itu_t_t35_country_code == 0xff)
  {
    reader.readBits("itu_t_t35_country_code_extension_byte", 8);
    i = 2;
  }
  if (itu_t_t35_country_code == 0xB5)
  {
    // This is possibly an ANSI payload (see ANSI-SCTE 128-1 2013)
    {
      // A fixed 16-bit field registered by the ATSC. The
                                                 // value shall be 0x0031 (49).
      Options opt;
      opt.meaningMap[49] = "ANSI-SCTE 128-1 2013";
      reader.readBits("itu_t_t35_provider_code",
               16,
               opt); 
    }
    i += 2;

    unsigned user_identifier;
    {
      Options opt;
      opt.meaningMap[1195456820] = "ATSC1_data()"; // 0x47413934 ("GA94")
      opt.meaningMap[1146373937] = "afd_data()";   // 0x44544731 ("DTG1")
      opt.meaningString = "SCTE/ATSC Reserved";
      user_identifier = reader.readBits("user_identifier", 32, opt);
    }
    i += 4;

    if (user_identifier == 1195456820)
      parse_ATSC1_data(reader);
    else
    {
      // Display the raw bytes of the payload
      int idx = 0;
      while (i < data.size())
      {
        reader.readBits(parser::formatArray("itu_t_t35_payload_byte_array", idx), 8);
        i++;
      }
    }
  }
  else
  {
    // Just display the raw bytes of the payload
    int idx = 0;
    while (i < data.size())
    {
      reader.readBits(parser::formatArray("itu_t_t35_payload_byte_array", idx), 8);
      i++;
    }
  }
}

} // namespace parser::subtitle::itutt35