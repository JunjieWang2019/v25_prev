/* The copyright in this software is being made available under the BSD
 * Licence, included below.  This software may be subject to other third
 * party and contributor rights, including patent rights, and no such
 * rights are granted under this licence.
 *
 * Copyright (c) 2017-2018, ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the ISO/IEC nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "geometry_octree.h"

// todo: break dependency
#include "PCCTMC3Decoder.h"

#include "OctreeNeighMap.h"
#include "io_hls.h"
#include "tables.h"

namespace pcc {

//============================================================================

class GeometryOctreeDecoder {
public:
  GeometryOctreeDecoder(o3dgc::Arithmetic_Codec* arithmeticDecoder);

  int decodePositionLeafNumPoints();

  int decodeOccupancyNeighZ();

  int decodeOccupancyNeighNZ(int neighPattern10);

  uint32_t decodeGeometryOccupancy(const PCCOctree3Node& node0);

  PCCVector3<uint32_t> decodePointPosition(int nodeSizeLog2);

  template<class OutputIt>
  int decodeDirectPosition(
    int nodeSizeLog2, const PCCOctree3Node& node, OutputIt outputPoints);

private:
  o3dgc::Arithmetic_Codec* _arithmeticDecoder;
  o3dgc::Static_Bit_Model _ctxEquiProb;
  o3dgc::Adaptive_Bit_Model _ctxSingleChild;
  o3dgc::Adaptive_Bit_Model _ctxSinglePointPerBlock;
  o3dgc::Adaptive_Bit_Model _ctxPointCountPerBlock;
  o3dgc::Adaptive_Bit_Model _ctxBlockSkipTh;
  o3dgc::Adaptive_Bit_Model _ctxNumIdcmPointsEq1;
  CtxModelOctreeOccupancy _ctxOccupancy;
};

//============================================================================

GeometryOctreeDecoder::GeometryOctreeDecoder(
  o3dgc::Arithmetic_Codec* arithmeticDecoder)
  : _arithmeticDecoder(arithmeticDecoder)
{}

//============================================================================
// Decode the number of points in a leaf node of the octree.

int
GeometryOctreeDecoder::decodePositionLeafNumPoints()
{
  const bool isSinglePoint =
    _arithmeticDecoder->decode(_ctxSinglePointPerBlock) != 0;

  int count = 1;
  if (!isSinglePoint) {
    count += _arithmeticDecoder->ExpGolombDecode(
      0, _ctxEquiProb, _ctxPointCountPerBlock);
  }

  return count;
}

//---------------------------------------------------------------------------
// decode occupancy bits (neighPattern10 == 0 case)

int
GeometryOctreeDecoder::decodeOccupancyNeighZ()
{
  int numOccupiedAcc = 0;
  int bit;
  int occupancy = 0;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b0[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 1;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b1[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 7;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b2[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 5;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b3[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 3;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b4[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 2;

  bit = _arithmeticDecoder->decode(_ctxOccupancy.b5[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 4;

  // NB: There must be at least two occupied child nodes
  //  -- avoid coding the occupancyB if it is implied.
  bit = 1;
  if (numOccupiedAcc >= 1)
    bit = _arithmeticDecoder->decode(_ctxOccupancy.b6[numOccupiedAcc]);
  numOccupiedAcc += bit;
  occupancy |= bit << 6;

  bit = 1;
  if (numOccupiedAcc >= 2)
    bit = _arithmeticDecoder->decode(_ctxOccupancy.b7[numOccupiedAcc]);
  occupancy |= bit << 0;

  return occupancy;
}

//---------------------------------------------------------------------------
// decode occupancy bits (neighPattern10 != 0 case)

int
GeometryOctreeDecoder::decodeOccupancyNeighNZ(int neighPattern10)
{
  int occupancy = 0;
  int partialOccupancy = 0;
  int idx;
  int bit;

  idx = neighPattern10;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b0[idx]);
  partialOccupancy |= bit << 0;
  occupancy |= bit << 1;

  idx = (neighPattern10 << 1) + partialOccupancy;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b1[idx]);
  partialOccupancy |= bit << 1;
  occupancy |= bit << 7;

  idx = (neighPattern10 << 2) + partialOccupancy;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b2[idx]);
  partialOccupancy |= bit << 2;
  occupancy |= bit << 5;

  idx = (neighPattern10 << 3) + partialOccupancy;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b3[idx]);
  partialOccupancy |= bit << 3;
  occupancy |= bit << 3;

  // todo(df): merge constants into lut.
  idx = ((neighPattern10 - 1) << 4) + partialOccupancy;
  idx = kOccMapBit4CtxIdx[idx] - 1 + 5;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b4[idx]);
  partialOccupancy |= bit << 4;
  occupancy |= bit << 2;

  idx = ((neighPattern10 - 1) << 5) + partialOccupancy;
  idx = kOccMapBit5CtxIdx[idx] - 1 + 6;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b5[idx]);
  partialOccupancy |= bit << 5;
  occupancy |= bit << 4;

  int neighPattern7 = kNeighPattern10to7[neighPattern10];
  idx = ((neighPattern7 - 1) << 6) + partialOccupancy;
  idx = kOccMapBit6CtxIdx[idx] - 1 + 7;
  bit = _arithmeticDecoder->decode(_ctxOccupancy.b6[idx]);
  partialOccupancy += bit << 6;
  occupancy |= bit << 6;

  int neighPattern5 = kNeighPattern7to5[neighPattern7];
  idx = ((neighPattern5 - 1) << 7) + partialOccupancy;
  idx = kOccMapBit7CtxIdx[idx] - 1 + 8;
  // NB: if firt 7 bits are 0, then the last is implicitly 1.
  bit = 1;
  if (partialOccupancy)
    bit = _arithmeticDecoder->decode(_ctxOccupancy.b7[idx]);
  occupancy |= bit << 0;

  return occupancy;
}

//-------------------------------------------------------------------------
// decode node occupancy bits
//

uint32_t
GeometryOctreeDecoder::decodeGeometryOccupancy(const PCCOctree3Node& node0)
{
  // neighbouring configuration with reduction from 64 to 10
  int neighPattern = node0.neighPattern;
  int neighPattern10 = kNeighPattern64to10[neighPattern];

  // decode occupancy pattern
  uint32_t occupancy;
  if (neighPattern10 == 0) {
    // neighbour empty and only one point => decode index, not pattern
    if (_arithmeticDecoder->decode(_ctxSingleChild)) {
      uint32_t cnt = _arithmeticDecoder->decode(_ctxEquiProb);
      cnt |= _arithmeticDecoder->decode(_ctxEquiProb) << 1;
      cnt |= _arithmeticDecoder->decode(_ctxEquiProb) << 2;
      occupancy = 1 << cnt;
    } else {
      occupancy = decodeOccupancyNeighZ();
    }
  } else {
    occupancy = decodeOccupancyNeighNZ(neighPattern10);
    occupancy = mapGeometryOccupancyInv(occupancy, neighPattern);
  }
  return occupancy;
}

//-------------------------------------------------------------------------
// Decode a position of a point in a given volume.

PCCVector3<uint32_t>
GeometryOctreeDecoder::decodePointPosition(int nodeSizeLog2)
{
  PCCVector3<uint32_t> delta{};
  for (int i = nodeSizeLog2; i > 0; i--) {
    delta <<= 1;
    delta[0] |= _arithmeticDecoder->decode(_ctxEquiProb);
    delta[1] |= _arithmeticDecoder->decode(_ctxEquiProb);
    delta[2] |= _arithmeticDecoder->decode(_ctxEquiProb);
  }

  return delta;
}

//-------------------------------------------------------------------------
// Direct coding of position of points in node (early tree termination).
// Decoded points are written to @outputPoints
// Returns the number of points emitted.

template<class OutputIt>
int
GeometryOctreeDecoder::decodeDirectPosition(
  int nodeSizeLog2, const PCCOctree3Node& node, OutputIt outputPoints)
{
  bool isDirectMode = _arithmeticDecoder->decode(_ctxBlockSkipTh);
  if (!isDirectMode) {
    return 0;
  }

  int numPoints = 1;
  if (_arithmeticDecoder->decode(_ctxNumIdcmPointsEq1))
    numPoints++;

  for (int i = 0; i < numPoints; i++) {
    // convert node-relative position to world position
    PCCVector3<uint32_t> pos = node.pos + decodePointPosition(nodeSizeLog2);
    *(outputPoints++) = {double(pos[0]), double(pos[1]), double(pos[2])};
  }

  return numPoints;
}

//-------------------------------------------------------------------------

void
PCCTMC3Decoder3::decodePositions(
  const PayloadBuffer& buf, PCCPointSet3& pointCloud)
{
  assert(buf.type == PayloadType::kGeometryBrick);

  int gbhSize;
  GeometryBrickHeader gbh = parseGbh(*_gps, buf, &gbhSize);
  minPositions.x() = gbh.geomBoxOrigin.x();
  minPositions.y() = gbh.geomBoxOrigin.y();
  minPositions.z() = gbh.geomBoxOrigin.z();

  pointCloud.resize(gbh.geom_num_points);

  o3dgc::Arithmetic_Codec arithmeticDecoder(
    int(buf.size()) - gbhSize,
    reinterpret_cast<uint8_t*>(const_cast<char*>(buf.data() + gbhSize)));
  arithmeticDecoder.start_decoder();
  GeometryOctreeDecoder decoder(&arithmeticDecoder);

  // init main fifo
  //  -- worst case size is the last level containing every input poit
  //     and each point being isolated in the previous level.
  pcc::ringbuf<PCCOctree3Node> fifo(pointCloud.getPointCount() + 1);

  // the current node dimension (log2)
  int nodeSizeLog2 = gbh.geom_max_node_size_log2;

  // push the first node
  fifo.emplace_back();
  PCCOctree3Node& node00 = fifo.back();
  node00.start = uint32_t(0);
  node00.end = uint32_t(pointCloud.getPointCount());
  node00.pos = uint32_t(0);
  node00.neighPattern = 0;
  node00.numSiblingsPlus1 = 8;
  node00.siblingOccupancy = 0;

  size_t processedPointCount = 0;
  std::vector<uint32_t> values;

  auto fifoCurrLvlEnd = fifo.end();

  // this counter represents fifo.end() - fifoCurrLvlEnd().
  // ie, the number of nodes added to the next level of the tree
  int numNodesNextLvl = 0;

  PCCVector3<uint32_t> occupancyAtlasOrigin(0xffffffff);
  MortonMap3D occupancyAtlas;
  if (_gps->neighbour_avail_boundary_log2) {
    occupancyAtlas.resize(_gps->neighbour_avail_boundary_log2);
    occupancyAtlas.clear();
  }

  for (; !fifo.empty(); fifo.pop_front()) {
    if (fifo.begin() == fifoCurrLvlEnd) {
      // transition to the next level
      fifoCurrLvlEnd = fifo.end();
      nodeSizeLog2--;
      numNodesNextLvl = 0;
      occupancyAtlasOrigin = 0xffffffff;
    }
    PCCOctree3Node& node0 = fifo.front();

    if (_gps->neighbour_avail_boundary_log2) {
      updateGeometryOccupancyAtlas(
        node0.pos, nodeSizeLog2, fifo, fifoCurrLvlEnd, &occupancyAtlas,
        &occupancyAtlasOrigin);

      node0.neighPattern =
        makeGeometryNeighPattern(node0.pos, nodeSizeLog2, occupancyAtlas);
    }

    // decode occupancy pattern
    uint8_t occupancy = decoder.decodeGeometryOccupancy(node0);

    assert(occupancy > 0);

    // population count of occupancy for IDCM
    int numOccupied = popcnt(occupancy);

    // split the current node
    for (int i = 0; i < 8; i++) {
      uint32_t mask = 1 << i;
      if (!(occupancy & mask)) {
        // child is empty: skip
        continue;
      }

      int x = !!(i & 4);
      int y = !!(i & 2);
      int z = !!(i & 1);

      int childSizeLog2 = nodeSizeLog2 - 1;

      // point counts for leaf nodes are coded immediately upon
      // encountering the leaf node.
      if (childSizeLog2 == 0) {
        int numPoints = 1;

        if (!_gps->geom_unique_points_flag) {
          numPoints = decoder.decodePositionLeafNumPoints();
        }

        const PCCVector3D point(
          node0.pos[0] + (x << childSizeLog2),
          node0.pos[1] + (y << childSizeLog2),
          node0.pos[2] + (z << childSizeLog2));

        for (int i = 0; i < numPoints; ++i)
          pointCloud[processedPointCount++] = point;

        // do not recurse into leaf nodes
        continue;
      }

      // create & enqueue new child.
      fifo.emplace_back();
      auto& child = fifo.back();

      child.pos[0] = node0.pos[0] + (x << childSizeLog2);
      child.pos[1] = node0.pos[1] + (y << childSizeLog2);
      child.pos[2] = node0.pos[2] + (z << childSizeLog2);
      child.numSiblingsPlus1 = numOccupied;
      child.siblingOccupancy = occupancy;

      bool idcmEnabled = _gps->inferred_direct_coding_mode_enabled_flag;
      if (isDirectModeEligible(idcmEnabled, nodeSizeLog2, node0, child)) {
        int numPoints = decoder.decodeDirectPosition(
          childSizeLog2, child, &pointCloud[processedPointCount]);
        processedPointCount += numPoints;

        if (numPoints > 0) {
          // node fully decoded, do not split: discard child
          fifo.pop_back();

          // NB: no further siblings to decode by definition of IDCM
          assert(child.numSiblingsPlus1 == 1);
          break;
        }
      }

      numNodesNextLvl++;

      if (!_gps->neighbour_avail_boundary_log2) {
        updateGeometryNeighState(
          _gps->neighbour_context_restriction_flag, fifo.end(),
          numNodesNextLvl, childSizeLog2, child, i, node0.neighPattern,
          occupancy);
      }
    }
  }

  arithmeticDecoder.stop_decoder();
}

//============================================================================

}  // namespace pcc
