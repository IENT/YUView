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

#define READBITS(into,numBits) do { if (!reader.readBits(numBits, into, #into)) return false; } while(0)
#define READBITS_M(into,numBits,meanings) do { if (!reader.readBits(numBits, into, #into, meanings)) return false; } while(0)
#define READBITS_M_E(into,numBits,meanings,type) do { unsigned int val; if (!reader.readBits(numBits, val, #into, meanings)) return false; into = (type)val; } while (0)
#define READBITS_A(into,numBits,idx) do { if (!reader.readBits(numBits, into, #into, idx)) return false; } while(0)
#define READBITS_A_M(into,numBits,idx,meanings) do { if (!reader.readBits(numBits, into, #into, idx, meanings)) return false; } while(0)
#define READZEROBITS(numBits,name) do { if (!reader.readZeroBits(numBits, name)) return false; } while(0)
#define IGNOREBITS(numBits) do { if (!reader.ignoreBits(numBits)) return false; } while(0)

#define READFLAG(into) do { if (!reader.readFlag(into, #into)) return false; } while(0)
#define READFLAG_M(into,meanings) do { if (!reader.readFlag(into, #into, meanings)) return false; } while(0)
#define READFLAG_A(into,idx) do { if (!reader.readFlag(into, #into, idx)) return false; } while(0)
#define READFLAG_A_M(into,idx,meanings) do { if (!reader.readFlag(into, #into, idx, meanings)) return false; } while(0)

#define READUEV(into) do { if (!reader.readUEV(into, #into)) return false; } while(0)
#define READUEV_M(into,meanings) do { if (!reader.readUEV(into, #into, meanings)) return false; } while(0)
#define READUEV_A(into,idx) do { if (!reader.readUEV(into, #into, idx)) return false; } while(0)
#define READUEV_A_M(into,idx,meanings) do { if (!reader.readUEV(into, #into, idx, meanings)) return false; } while(0)

#define READSEV(into) do { if (!reader.readSEV(into, #into)) return false; } while(0)
#define READSEV_A(into,idx) do { if (!reader.readSEV(into, #into, idx)) return false; } while(0)
#define READUEV_APP(into) do { if (!reader.readSEV(into, #into, -1)) return false; } while(0)

#define READLEB128(into) do { if (!reader.readLeb128(into, #into)) return false; } while(0)
#define READUVLC(into) do { if (!reader.readUVLC(into, #into)) return false; } while (0)
#define READNS(into,maxValue) do { if (!reader.readNS(into, #into, maxValue)) return false; } while (0) 
#define READSU(into,numBits) do { if (!reader.readSU(into, #into, numBits)) return false; } while (0)

#define LOGVAL(val) reader.logValue(val, #val)
#define LOGVAL_M(val,meaning) reader.logValue(val, #val, meaning)
#define LOGSTRVAL(name,val) reader.logValue(val, name)
#define LOGPARAM(name,val,coding,code,meaning) reader.logValue(val, name, coding, code, meaning)
#define LOGINFO(info) reader.logInfo(info)