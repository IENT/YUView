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

#include "tile_info.h"

#include "sequence_header_obu.h"
#include "typedef.h"

namespace
{

unsigned tile_log2(unsigned blkSize, unsigned target)
{
  unsigned k;
  for (k = 0; (blkSize << k) < target; k++)
  {
  }
  return k;
}

} // namespace

namespace parser::av1
{

using namespace reader;

void tile_info::parse(reader::SubByteReaderLogging &       reader,
                      std::shared_ptr<sequence_header_obu> seqHeader,
                      unsigned                             MiCols,
                      unsigned                             MiRows)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "tile_info()");

  this->sbCols  = seqHeader->use_128x128_superblock ? ((MiCols + 31) >> 5) : ((MiCols + 15) >> 4);
  this->sbRows  = seqHeader->use_128x128_superblock ? ((MiRows + 31) >> 5) : ((MiRows + 15) >> 4);
  this->sbShift = seqHeader->use_128x128_superblock ? 5 : 4;
  auto sbSize   = this->sbShift + 2;
  this->maxTileWidthSb  = MAX_TILE_WIDTH >> sbSize;
  this->maxTileAreaSb   = MAX_TILE_AREA >> (2 * sbSize);
  this->minLog2TileCols = tile_log2(this->maxTileWidthSb, sbCols);
  this->maxLog2TileCols = tile_log2(1, std::min(this->sbCols, MAX_TILE_COLS));
  this->maxLog2TileRows = tile_log2(1, std::min(this->sbRows, MAX_TILE_ROWS));
  this->minLog2Tiles =
      std::max(this->minLog2TileCols, tile_log2(this->maxTileAreaSb, sbRows * sbCols));

  this->uniform_tile_spacing_flag = reader.readFlag("uniform_tile_spacing_flag");
  if (this->uniform_tile_spacing_flag)
  {
    this->TileColsLog2 = this->minLog2TileCols;
    while (this->TileColsLog2 < this->maxLog2TileCols)
    {
      this->increment_tile_cols_log2 = reader.readFlag("increment_tile_cols_log2");
      if (this->increment_tile_cols_log2)
        this->TileColsLog2++;
      else
        break;
    }
    this->tileWidthSb = (sbCols + (1 << TileColsLog2) - 1) >> TileColsLog2;
    this->TileCols    = 0;
    for (unsigned startSb = 0; startSb < sbCols; startSb += tileWidthSb)
    {
      this->MiColStarts.push_back(startSb << sbShift);
      this->TileCols++;
    }
    this->MiColStarts.push_back(MiCols);

    this->minLog2TileRows = std::max(this->minLog2Tiles - this->TileColsLog2, 0u);
    this->TileRowsLog2    = this->minLog2TileRows;
    while (this->TileRowsLog2 < this->maxLog2TileRows)
    {
      if (reader.readFlag("increment_tile_rows_log2"))
        this->TileRowsLog2++;
      else
        break;
    }
    this->tileHeightSb = (this->sbRows + (1 << this->TileRowsLog2) - 1) >> this->TileRowsLog2;
    this->TileRows     = 0;
    for (unsigned startSb = 0; startSb < sbRows; startSb += this->tileHeightSb)
    {
      this->MiRowStarts.push_back(startSb << sbShift);
      this->TileRows++;
    }
    this->MiRowStarts.push_back(MiRows);
  }
  else
  {
    this->widestTileSb = 0;
    this->TileCols     = 0;
    for (unsigned startSb = 0; startSb < sbCols;)
    {
      this->MiColStarts.push_back(startSb << sbShift);
      auto maxWidth = std::min(sbCols - startSb, this->maxTileWidthSb);
      this->width_in_sbs_minus_1 = reader.readNS("width_in_sbs_minus_1", maxWidth);
      auto sizeSb   = this->width_in_sbs_minus_1 + 1;
      this->widestTileSb = std::max(sizeSb, this->widestTileSb);
      startSb += sizeSb;
      this->TileCols++;
    }
    this->MiColStarts.push_back(MiCols);
    this->TileColsLog2 = tile_log2(1, TileCols);

    if (this->minLog2Tiles > 0)
      this->maxTileAreaSb = (sbRows * sbCols) >> (this->minLog2Tiles + 1);
    else
      this->maxTileAreaSb = sbRows * sbCols;
    if (this->widestTileSb == 0)
      throw std::logic_error("widestTileSb is 0. This is not possible.");
    this->maxTileHeightSb = std::max(this->maxTileAreaSb / this->widestTileSb, 1u);

    this->TileRows = 0;
    for (unsigned startSb = 0; startSb < sbRows;)
    {
      MiRowStarts.push_back(startSb << sbShift);
      auto maxHeight = std::min(sbRows - startSb, this->maxTileHeightSb);
      this->height_in_sbs_minus_1 = reader.readNS("height_in_sbs_minus_1", maxHeight);
      auto sizeSb = height_in_sbs_minus_1 + 1;
      startSb += sizeSb;
      this->TileRows++;
    }
    this->MiRowStarts.push_back(MiRows);
    this->TileRowsLog2 = tile_log2(1, TileRows);
  }
  if (this->TileColsLog2 > 0 || this->TileRowsLog2 > 0)
  {
    this->context_update_tile_id = reader.readNS("context_update_tile_id", this->TileRowsLog2 + this->TileColsLog2);
    this->tile_size_bytes_minus_1 = reader.readNS("tile_size_bytes_minus_1", 2);
    this->TileSizeBytes = tile_size_bytes_minus_1 + 1;
  }
  else
    this->context_update_tile_id = 0;

  reader.logCalculatedValue("TileColsLog2", TileColsLog2);
  reader.logCalculatedValue("TileRowsLog2", TileRowsLog2);
}

} // namespace parser::av1