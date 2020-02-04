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

#pragma once

#include "parserAnnexB.h"
#include "parserCommon.h"
#include "parserCommonMacros.h"
#include "parserSubtitle608.h"

template <class T>
class user_data_registered_itu_t_t35_sei : public T
{
  static_assert(std::is_base_of<parserAnnexB::nal_unit, T>::value, "T must derive from parserAnnexB::nal_unit");
    
public:
  user_data_registered_itu_t_t35_sei(QSharedPointer<T> sei_src) : T(sei_src) {};
  parserAnnexB::sei_parsing_return_t parse_user_data_registered_itu_t_t35(QByteArray &data, parserCommon::TreeItem *root) { return parse_internal(data, root) ? parserAnnexB::SEI_PARSING_OK : parserAnnexB::SEI_PARSING_ERROR; }

  unsigned int itu_t_t35_country_code;
  unsigned int itu_t_t35_country_code_extension_byte;
  QByteArray itu_t_t35_payload_byte_array;
  // ANSI-SCTE 128-1 2013
  unsigned int itu_t_t35_provider_code;
  unsigned int user_identifier;
  // ATSC1 data
  unsigned int user_data_type_code;
  bool process_em_data_flag;
  bool process_cc_data_flag;
  bool additional_data_flag;
  unsigned int cc_count;
  unsigned int em_data;
  QList<unsigned int> cc_packet_data;
  unsigned int marker_bits;
  QList<unsigned int> ATSC_reserved_user_data;

private:
  bool parse_internal(QByteArray &data, parserCommon::TreeItem *root)
  {
    parserCommon::reader_helper reader(data, root, "user_data_registered_itu_t_t35()");

    if (data.length() < 2)
      return reader.addErrorMessageChildItem("Invalid length of user_data_registered_itu_t_t35 SEI.");

    QStringList itu_t_t35_country_code_meaning = QStringList() << "Japan" << "Albania" << "Algeria" << "American Samoa" << "Germany (Federal Republic of)" << "Anguilla" << "Antigua and Barbuda" << "Argentina" << "Ascension (see S. Helena)" << "Australia" << "Austria" << "Bahamas" << "Bahrain" << "Bangladesh" << "Barbados" << "Belgium" << "Belize" << "Benin (Republic of)" << "Bermudas" << "Bhutan (Kingdom of)" << "Bolivia" << "Botswana" << "Brazil" << "British Antarctic Territory" << "British Indian Ocean Territory" << "British Virgin Islands" << "Brunei Darussalam" << "Bulgaria" << "Myanmar (Union of)" << "Burundi" << "Byelorussia" << "Cameroon" << "Canada" << "Cape Verde" << "Cayman Islands" << "Central African Republic" << "Chad" << "Chile" << "China" << "Colombia" << "Comoros" << "Congo" << "Cook Islands" << "Costa Rica" << "Cuba" << "Cyprus" << "Czech and Slovak Federal Republic" << "Cambodia" << "Democratic People's Republic of Korea" << "Denmark" << "Djibouti" << "Dominican Republic" << "Dominica" << "Ecuador" << "Egypt" << "El Salvador" << "Equatorial Guinea" << "Ethiopia" << "Falkland Islands" << "Fiji" << "Finland" << "France" << "French Polynesia" << "French Southern and Antarctic Lands" << "Gabon" << "Gambia" << "Germany (Federal Republic of)" << "Angola" << "Ghana" << "Gibraltar" << "Greece" << "Grenada" << "Guam" << "Guatemala" << "Guernsey" << "Guinea" << "Guinea-Bissau" << "Guayana" << "Haiti" << "Honduras" << "Hongkong" << "Hungary (Republic of)" << "Iceland" << "India" << "Indonesia" << "Iran (Islamic Republic of)" << "Iraq" << "Ireland" << "Israel" << "Italy" << "C�te d'Ivoire" << "Jamaica" << "Afghanistan" << "Jersey" << "Jordan" << "Kenya" << "Kiribati" << "Korea (Republic of)" << "Kuwait" << "Lao (People's Democratic Republic)" << "Lebanon" << "Lesotho" << "Liberia" << "Libya" << "Liechtenstein" << "Luxembourg" << "Macau" << "Madagascar" << "Malaysia" << "Malawi" << "Maldives" << "Mali" << "Malta" << "Mauritania" << "Mauritius" << "Mexico" << "Monaco" << "Mongolia" << "Montserrat" << "Morocco" << "Mozambique" << "Nauru" << "Nepal" << "Netherlands" << "Netherlands Antilles" << "New Caledonia" << "New Zealand" << "Nicaragua" << "Niger" << "Nigeria" << "Norway" << "Oman" << "Pakistan" << "Panama" << "Papua New Guinea" << "Paraguay" << "Peru" << "Philippines" << "Poland (Republic of)" << "Portugal" << "Puerto Rico" << "Qatar" << "Romania" << "Rwanda" << "Saint Kitts and Nevis" << "Saint Croix" << "Saint Helena and Ascension" << "Saint Lucia" << "San Marino" << "Saint Thomas" << "Sao Tom� and Principe" << "Saint Vincent and the Grenadines" << "Saudi Arabia" << "Senegal" << "Seychelles" << "Sierra Leone" << "Singapore" << "Solomon Islands" << "Somalia" << "South Africa" << "Spain" << "Sri Lanka" << "Sudan" << "Suriname" << "Swaziland" << "Sweden" << "Switzerland" << "Syria" << "Tanzania" << "Thailand" << "Togo" << "Tonga" << "Trinidad and Tobago" << "Tunisia" << "Turkey" << "Turks and Caicos Islands" << "Tuvalu" << "Uganda" << "Ukraine" << "United Arab Emirates" << "United Kingdom" << "United States (ANSI-SCTE 128-1)" << "Burkina Faso" << "Uruguay" << "U.S.S.R." << "Vanuatu" << "Vatican City State" << "Venezuela" << "Viet Nam" << "Wallis and Futuna" << "Western Samoa" << "Yemen (Republic of)" << "Yemen (Republic of)" << "Yugoslavia" << "Zaire" << "Zambia" << "Zimbabwe" << "Unspecified";
    READBITS_M(itu_t_t35_country_code, 8, itu_t_t35_country_code_meaning);
    int i = 1;
    if (itu_t_t35_country_code == 0xff)
    {
      READBITS(itu_t_t35_country_code_extension_byte, 8);
      i = 2;
    }
    if (itu_t_t35_country_code == 0xB5)
    {
      // This is possibly an ANSI payload (see ANSI-SCTE 128-1 2013)
      QMap<int, QString> itu_t_t35_provider_code_meaning;
      itu_t_t35_provider_code_meaning.insert(49, "ANSI-SCTE 128-1 2013");
      READBITS_M(itu_t_t35_provider_code, 16, itu_t_t35_provider_code_meaning);  // A fixed 16-bit field registered by the ATSC. The value shall be 0x0031 (49).
      i += 2;

      QMap<int, QString> user_identifier_meaning;
      user_identifier_meaning.insert(1195456820, "ATSC1_data()"); // 0x47413934 ("GA94")
      user_identifier_meaning.insert(1146373937, "afd_data()");   // 0x44544731 ("DTG1")
      user_identifier_meaning.insert(-1, "SCTE/ATSC Reserved");
      READBITS_M(user_identifier, 32, user_identifier_meaning);
      i += 4;

      if (user_identifier == 1195456820)
      {
        if (!parse_ATSC1_data(reader))
          return false;
      }
      else
      {
        // Display the raw bytes of the payload
        int idx = 0;
        while (i < data.length())
        {
          READBITS_A(itu_t_t35_payload_byte_array, 8, idx);
          i++;
        }
      }
    }
    else
    {
      // Just display the raw bytes of the payload
      int idx = 0;
      while (i < data.length())
      {
        READBITS_A(itu_t_t35_payload_byte_array, 8, idx);
        i++;
      }
    }

    return true;
  }
  bool parse_ATSC1_data(parserCommon::reader_helper &reader)
  {
    parserCommon::reader_sub_level s(reader, "ATSC1_data");

    QMap<int, QString> user_data_type_code_meaning;
    user_data_type_code_meaning.insert(3, "cc_data() / DTV CC");
    user_data_type_code_meaning.insert(6, "bar_data()");
    user_data_type_code_meaning.insert(-1, "SCTE/ATSC Reserved");
    READBITS_M(user_data_type_code, 8, user_data_type_code_meaning);

    if (user_data_type_code == 0x03)
    {
      // cc_data() - CEA-708
      READFLAG(process_em_data_flag);
      READFLAG(process_cc_data_flag);
      READFLAG(additional_data_flag);
      READBITS(cc_count, 5);
      READBITS(em_data, 8);

      if (reader.nrBytesLeft() > cc_count * 3 + 1)
      {
        // Possibly, the count is wrong.
        if ((reader.nrBytesLeft() - 1) % 3 == 0)
          cc_count = (reader.nrBytesLeft() - 1) / 3;
      }
      
      // Now should follow (cc_count * 24 bits of cc_data_pkts)
      // Just display the raw bytes of the payload
      for (unsigned int i = 0; i < cc_count; i++)
      {
        unsigned int ccData;
        subtitle_608::parse608DataPayloadCCDataPacket(reader, ccData);
      }

      READBITS(marker_bits, 8);
      // The ATSC marker_bits indicator should be 255
      
      // ATSC_reserved_user_data
      int idx = 0;
      while (reader.testReadingBits(8))
      {
        READBITS_A(ATSC_reserved_user_data, 8, idx);
        idx++;
      }
    }

    return true;
  }
};
