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

#include "geometry.h"

#include "DualLutCoder.h"
#include "OctreeNeighMap.h"
#include "geometry_octree.h"
#include "geometry_intra_pred.h"
#include "io_hls.h"
#include "tables.h"
#include "quantization.h"
#include "TMC3.h"
#include "motionWip.h"
#include <set>
#include <random>

namespace pcc {

//============================================================================

enum class DirectMode
{
  kUnavailable,
  kAllPointSame,
  kTwoPoints
};

//============================================================================

class GeometryOctreeEncoder : protected GeometryOctreeContexts {
public:
  GeometryOctreeEncoder(
    const GeometryParameterSet& gps,
    const GeometryBrickHeader& gbh,
    const GeometryOctreeContexts& ctxtMem,
    EntropyEncoder* arithmeticEncoder);

  GeometryOctreeEncoder(const GeometryOctreeEncoder&) = default;
  GeometryOctreeEncoder(GeometryOctreeEncoder&&) = default;
  GeometryOctreeEncoder& operator=(const GeometryOctreeEncoder&) = default;
  GeometryOctreeEncoder& operator=(GeometryOctreeEncoder&&) = default;

  void beginOctreeLevel(const Vec3<int>& planarDepth);

  void resetMap();
  void clearMap();

  void encodePositionLeafNumPoints(int count);

  int encodePlanarMode(
    OctreeNodePlanar& planar,
    int plane,
    int dist,
    int adjPlanes,
    int planeId,
    int contextAngle,
    bool* twoPlanesIsPlanar,
    bool* multiPlanarEligible,
    OctreeNodePlanar& planarRef);

  void derivePlanarPCMContextBuffer(
    OctreeNodePlanar& planar,
    OctreeNodePlanar& planarRef,
    OctreePlanarBuffer& planeBuffer,
    int xx,
    int yy,
    int zz,
    OctreePlanarBuffer::Row* planeBuffer0,
    OctreePlanarBuffer::Row* planeBuffer1,
    OctreePlanarBuffer::Row* planeBuffer2);

  void determinePlanarMode(
    const bool& adjacent_child_contextualization_enabled_flag,
    int planeId,
    OctreeNodePlanar& planar,
    OctreePlanarBuffer::Row* planeBuffer,
    int coord1,
    int coord2,
    int coord3,
    int posInParent,
    const GeometryNeighPattern& gnp,
    uint8_t siblingOccupancy,
    int planarRate[3],
    int contextAngle,
    bool* twoPlanesIsPlanar,
    bool* multiPlanarEligible,
    OctreeNodePlanar& planarRef);

  void determinePlanarMode(
    bool adjacent_child_contextualization_enabled_flag,
    int occupancy,
    const bool planarEligible[3],
    int posInParent,
    const GeometryNeighPattern& gnp,
    PCCOctree3Node& child,
    OctreeNodePlanar& planar,
    int contextAngle,
    int contextAnglePhiX,
    int contextAnglePhiY,
    OctreeNodePlanar& planarRef);

  void encodeOccupancyNeighZsimple(
    int mappedOccupancy,
    int mappedPlanarMaskX,
    bool planarPossibleX,
    int mappedPlanarMaskY,
    bool planarPossibleY,
    int mappedPlanarMaskZ,
    bool planarPossibleZ,
    int preOcc);

  void encodeOccupancyFullNeihbourgsNZ(
    int neighPattern,
    int occupancy,
    int Word4[8],
    int Word7Adj[8],
    bool Sparse[8],
    int planarMaskX,
    int planarMaskY,
    int planarMaskZ,
    bool planarPossibleX,
    bool planarPossibleY,
    bool planarPossibleZ,
    int predOcc);

  void encodeOccupancyFullNeihbourgs(
    int neighPattern,
    int occupancy,
    int planarMaskX,
    int planarMaskY,
    int planarMaskZ,
    bool planarPossibleX,
    bool planarPossibleY,
    bool planarPossibleZ,
    const MortonMap3D& occupancyAtlas,
    Vec3<int32_t> pos,
    const int atlasShift,
    bool flagWord4,
    bool adjacent_child_contextualization_enabled_flag,
    int predOccupancy);

  void encodeOrdered2ptPrefix(
    const point_t points[2], Vec3<bool> directIdcm, Vec3<int>& nodeSizeLog2);

  void encodePointPosition(
    const Vec3<int>& nodeSizeLog2AfterPlanar, const Vec3<int32_t>& pos);

  void encodePointPositionAngular(
    const OctreeAngPosScaler& quant,
    const OctreeNodePlanar& planar,
    const Vec3<int>& nodeSizeLog2Rem,
    Vec3<int> posXyz,
    const Vec3<int>& pos,
    int nodeLaserIdx,
    const Vec3<int>& angularOrigin,
    const int* zLaser,
    const int* thetaLaser,
    int numLasers);

  inline void encodePointPositionZAngular(
    const OctreeAngPosScaler& quant,
    const Vec3<int>& nodeSizeLog2Rem,
    const int* zLaser,
    const int* thetaLaser,
    int laserIdx,
    Vec3<int>& posXyz,
    const int& posZ);

  inline void encodePointPositionZAngularExtension(
    const Vec3<int>& angularOrigin,
    const Vec3<int>& nodePos,
    const int* zLaser,
    const int* thetaLaser,
    int laserIdx,
    int maskz,
    Vec3<int>& posXyz);

  void encodeNodeQpOffetsPresent(bool);
  void encodeQpOffset(int dqp);

  void encodeIsIdcm(DirectMode mode);

  void encodeDirectPosition(
    bool geom_unique_points_flag,
    bool joint_2pt_idcm_enabled_flag,
    bool geom_angular_mode_enabled_flag,
    DirectMode mode,
    const Vec3<uint32_t>& quantMasks,
    const Vec3<int>& nodeSizeLog2,
    int shiftBits,
    const PCCOctree3Node& node,
    const OctreeNodePlanar& planar,
    PCCPointSet3& pointCloud,
    const Vec3<int>& headPos,
    const int* zLaser,
    const int* thetaLaser,
    int numLasers);

  void encodeThetaRes(int ThetaRes, int prevThetaRes);
  void encodeZRes(int zRes);

  const GeometryOctreeContexts& getCtx() const { return *this; }

public:
  // selects between the bitwise and bytewise occupancy coders
  bool _useBitwiseOccupancyCoder;

  const uint8_t* _neighPattern64toR1;

  EntropyEncoder* _arithmeticEncoder;

  // Planar state
  OctreePlanarState _planar;

  // Azimuthal buffer
  std::vector<int> _phiBuffer;

  // last previously coded laser index residual for a given node laser index
  std::vector<int> _prevLaserIndexResidual;

  // azimuthal elementary shifts
  AzimuthalPhiZi _phiZi;

  // Octree extensions
  bool _angularExtension;
};

//============================================================================

GeometryOctreeEncoder::GeometryOctreeEncoder(
  const GeometryParameterSet& gps,
  const GeometryBrickHeader& gbh,
  const GeometryOctreeContexts& ctxtMem,
  EntropyEncoder* arithmeticEncoder)
  : GeometryOctreeContexts(ctxtMem)
  , _useBitwiseOccupancyCoder(gps.bitwise_occupancy_coding_flag)
  , _neighPattern64toR1(neighPattern64toR1(gps))
  , _arithmeticEncoder(arithmeticEncoder)
  , _planar(gps)
  , _phiBuffer(gps.numLasers(), 0x80000000)
  , _prevLaserIndexResidual(gps.numLasers(), 0x00000000)
  , _phiZi(gps.numLasers(), gps.angularNumPhiPerTurn)
  , _angularExtension(gps.octree_angular_extension_flag)
{
  if (!_useBitwiseOccupancyCoder && !gbh.entropy_continuation_flag) {
    for (int i = 0; i < 10; i++)
      _bytewiseOccupancyCoder[i].init(kDualLutOccupancyCoderInit[i]);
  }
}

//============================================================================

void
GeometryOctreeEncoder::beginOctreeLevel(const Vec3<int>& planarDepth)
{
  for (int i = 0; i < 10; i++) {
    _bytewiseOccupancyCoder[i].resetLut();
  }

  _planar.initPlanes(planarDepth);
}

void
GeometryOctreeEncoder::resetMap()
{
  for (int i = 0; i < 4; i++) {
    _MapOccupancy[i][0].reset(6 + 3, 12 - 3);
    _MapOccupancy[i][1].reset(6 + 3, 12 - 3);
    _MapOccupancy[i][2].reset(6 + 3, 12 - 3);
    _MapOccupancy[i][3].reset(6 + 3, 10 - 3);
    _MapOccupancy[i][4].reset(6 + 3, 12 - 3);
    _MapOccupancy[i][5].reset(6 + 3, 11 - 3);
    _MapOccupancy[i][6].reset(6 + 3, 11 - 3);
    _MapOccupancy[i][7].reset(6 + 3, 10 - 3);

    _MapOccupancySparse[i][0].reset(6 + 5, 9 - 5);
    _MapOccupancySparse[i][1].reset(6 + 5, 7 - 5);
    _MapOccupancySparse[i][2].reset(6 + 5, 8 - 5);
    _MapOccupancySparse[i][3].reset(6 + 5, 11 - 5);
    _MapOccupancySparse[i][4].reset(6 + 5, 10 - 5);
    _MapOccupancySparse[i][5].reset(6 + 5, 11 - 5);
    _MapOccupancySparse[i][6].reset(6 + 5, 12 - 5);
  }
}

//============================================================================
void
GeometryOctreeEncoder::clearMap()
{
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 8; i++) {
      _MapOccupancy[j][i].clear();
      _MapOccupancySparse[j][i].clear();
    }
}

//============================================================================
// Encode the number of points in a leaf node of the octree.

void
GeometryOctreeEncoder::encodePositionLeafNumPoints(int count)
{
  int dupPointCnt = count - 1;
  _arithmeticEncoder->encode(dupPointCnt > 0, _ctxDupPointCntGt0);
  if (dupPointCnt <= 0)
    return;

  _arithmeticEncoder->encodeExpGolomb(dupPointCnt - 1, 0, _ctxDupPointCntEgl);
  return;
}

//============================================================================

int
GeometryOctreeEncoder::encodePlanarMode(
  OctreeNodePlanar& node,
  int plane,
  int dist,
  int adjPlanes,
  int planeId,
  int contextAngle,
  bool* multiPlanarFlag,
  bool* multiPlanarEligible,
  OctreeNodePlanar& planarRef
)
{
  const int mask0 = (1 << planeId);
  const int mask1[3] = {6, 5, 3};

  bool isPlanar = node.planarMode & mask0;
  int planeBit = (node.planePosBits & mask0) == 0 ? 0 : 1;

  bool isPlanarRef = planarRef.planarMode & mask0;
  int planeBitRef = (planarRef.planePosBits & mask0) == 0 ? 0 : 1;

  int ctxIdx_Planar_flag = planeId;
  if (isPlanarRef)
    ctxIdx_Planar_flag += 3 * (planeBitRef + 1);

  if (!node.isPCM) {
    if (_planar._geom_multiple_planar_mode_enable_flag) {
      static const int planeId2Index[3][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}};
      bool multiPlanarFlagFalse = true;
      for (int i = 0; i < 3; i++) {
        multiPlanarFlagFalse &= !(multiPlanarFlag[planeId2Index[planeId][i]]);
      }
      bool inferredPlanarFalse = multiPlanarFlagFalse;

      if (multiPlanarFlagFalse) {
        if (planeId == 2) {
          if (multiPlanarEligible[0])
            inferredPlanarFalse =
              !((node.planarMode & 2) && (node.planarMode & 1));
          else if (multiPlanarEligible[2])
            inferredPlanarFalse = !(node.planarMode & 1);
          else if (multiPlanarEligible[3])
            inferredPlanarFalse = !(node.planarMode & 2);
        } else if (planeId == 1) {
          if (multiPlanarEligible[1])
            inferredPlanarFalse = !(node.planarMode & 1);
        }
      }

      if (inferredPlanarFalse)
        _arithmeticEncoder->encode(
          isPlanar, _ctxPlanarMode[ctxIdx_Planar_flag]);
    } else {
      _arithmeticEncoder->encode(isPlanar, _ctxPlanarMode[ctxIdx_Planar_flag]);
    }
  }

  if (!isPlanar) {
    node.planarPossible &= mask1[planeId];
    return -1;
  }

  // encode the plane index

  if (node.isPCM)
    return planeBit;

  // Not PCM and signall the plane bit

  if (planeId == node.lastDirIdx && node.isPreDirMatch && node.allowPCM) {
    if (
      isPlanarRef)  // isPlanarRef = isPlanar = 1; and it is not PCM --> Not need to signal the position bit
    {
      if (planeBitRef == planeBit)
        assert(false);
      return planeBit;
    }
  }

  if (contextAngle == -1) {  // angular mode off
    static const int kAdjPlaneCtx[4] = {0, 1, 2, 0};
    int planePosCtx = kAdjPlaneCtx[adjPlanes];
    if (plane < 0) {
      int planePostCtxTmp = planePosCtx;
      if (isPlanarRef) {
        planePostCtxTmp += 3 * (planeBitRef + 1);
      }
      _arithmeticEncoder->encode(
        planeBit, _ctxPlanarPlaneLastIndexZ[planePostCtxTmp]);

    } else {
      int discreteDist = dist > (8 >> OctreePlanarBuffer::shiftAb);
      int lastIndexPlane2d = plane + (discreteDist << 1);
      int refPlane = 0;
      if (isPlanarRef) {
        refPlane = 1 + planeBitRef;
      }
      _arithmeticEncoder->encode(
        planeBit,
        _ctxPlanarPlaneLastIndex[refPlane][planeId][planePosCtx]
                                [lastIndexPlane2d]);
    }
  } else {  // angular mode on
    int refPlane = isPlanarRef ? (1 + planeBitRef) : 0;
    if (planeId == 2) {  // angular
      _arithmeticEncoder->encode(
        planeBit, _ctxPlanarPlaneLastIndexAngular[refPlane][contextAngle]);
    } else {  // azimuthal
      _arithmeticEncoder->encode(
        planeBit, _ctxPlanarPlaneLastIndexAngularPhi[refPlane][contextAngle]);
    }
  }
  return planeBit;
}

//============================================================================

void
GeometryOctreeEncoder::derivePlanarPCMContextBuffer(
  OctreeNodePlanar& planar,
  OctreeNodePlanar& planarRef,
  OctreePlanarBuffer& planeBuffer,
  int xx,
  int yy,
  int zz,
  OctreePlanarBuffer::Row* planeBuffer0,
  OctreePlanarBuffer::Row* planeBuffer1,
  OctreePlanarBuffer::Row* planeBuffer2)
{
  bool matchDir[3] = {false, false, false};
  bool bufferAvai = true;
  int matchedDir = 0;

  int closestPlanarFlag;
  int closestDist = 0;

  planarRef.ctxBufPCM = 4
    * (int(planar.eligible[0]) + int(planar.eligible[1])
       + int(planar.eligible[2]) - 1);
  assert(planarRef.ctxBufPCM >= 0);

  for (int planeId = 0; planeId < 3; planeId++) {
    if (planar.eligible[planeId]) {
      const int mask0 = (1 << planeId);
      const int mask1[3] = {6, 5, 3};

      bool isPlanarRef = planarRef.planarMode & mask0;
      int planeBitRef = (planarRef.planePosBits & mask0) == 0 ? 0 : 1;

      // Get the buffer information
      OctreePlanarBuffer::Row* planeBufferDir = planeBuffer.getBuffer(planeId);

      int coord1 = yy;
      int coord2 = zz;
      int coord3 = xx;
      if (planeId == 1) {
        coord1 = xx;
        coord2 = zz;
        coord3 = yy;
      }
      if (planeId == 2) {
        coord1 = xx;
        coord2 = yy;
        coord3 = zz;
      }

      OctreePlanarBuffer::Elmt* row;
      int rowLen = OctreePlanarBuffer::rowSize;
      int maxCoord;

      if (planeBufferDir) {
        coord1 =
          (coord1 & OctreePlanarBuffer::maskAb) >> OctreePlanarBuffer::shiftAb;
        coord2 =
          (coord2 & OctreePlanarBuffer::maskAb) >> OctreePlanarBuffer::shiftAb;
        coord3 = coord3 & OctreePlanarBuffer::maskC;

        row = planeBufferDir[coord3];

        maxCoord = std::max(coord1, coord2);
        closestDist += std::abs(maxCoord - int(row[rowLen - 1].pos));

        int idxMinDist = rowLen - 1;
        closestPlanarFlag = row[idxMinDist].planeIdx;
        bool closetPL = (closestPlanarFlag > -1) ? true : false;
        int closetPlane = closetPL ? closestPlanarFlag : 0;
        matchDir[planeId] =
          (closetPL == isPlanarRef && closetPlane == planeBitRef);
        matchedDir += int(matchDir[planeId]);
      }
    }
  }
  planarRef.ctxBufPCM += matchedDir;
}

//============================================================================
// determine Planar mode for one direction

void
GeometryOctreeEncoder::determinePlanarMode(
  const bool& adjacent_child_contextualization_enabled_flag,
  int planeId,
  OctreeNodePlanar& planar,
  OctreePlanarBuffer::Row* planeBuffer,
  int coord1,
  int coord2,
  int coord3,
  int posInParent,
  const GeometryNeighPattern& gnp,
  uint8_t siblingOccupancy,
  int planarRate[3],
  int contextAngle,
  bool* multiPlanarFlag,
  bool* multiPlanarEligible,
  OctreeNodePlanar& planarRef)
{
  const int kPlanarChildThreshold = 63;
  const int kAdjNeighIdxFromPlanePos[3][2] = {1, 0, 2, 3, 4, 5};
  const int planeSelector = 1 << planeId;
  static const uint8_t KAdjNeighIdxMask[3][2] = {0x0f, 0xf0, 0x33,
                                                 0xcc, 0x55, 0xaa};
  OctreePlanarBuffer::Elmt* row;
  int rowLen = OctreePlanarBuffer::rowSize;
  int closestPlanarFlag;
  int closestDist;
  int maxCoord;

  if (!planeBuffer) {
    // angular: buffer disabled
    closestPlanarFlag = -1;
    closestDist = 0;
  } else {
    coord1 =
      (coord1 & OctreePlanarBuffer::maskAb) >> OctreePlanarBuffer::shiftAb;
    coord2 =
      (coord2 & OctreePlanarBuffer::maskAb) >> OctreePlanarBuffer::shiftAb;
    coord3 = coord3 & OctreePlanarBuffer::maskC;

    row = planeBuffer[coord3];

    maxCoord = std::max(coord1, coord2);
    closestDist = std::abs(maxCoord - int(row[rowLen - 1].pos));
    int idxMinDist = rowLen - 1;

    // push closest point front
    row[rowLen - 1] = row[idxMinDist];

    closestPlanarFlag = row[idxMinDist].planeIdx;
  }

  // The relative plane position (0|1) along the planeId axis.
  int pos = !(KAdjNeighIdxMask[planeId][0] & (1 << posInParent));

  // Determine which adjacent planes are occupied
  // The low plane is at position axis - 1
  bool lowAdjPlaneOccupied = adjacent_child_contextualization_enabled_flag
    ? KAdjNeighIdxMask[planeId][1] & gnp.adjNeighOcc[planeId]
    : (gnp.neighPattern >> kAdjNeighIdxFromPlanePos[planeId][0]) & 1;

  // The high adjacent plane is at position axis + 1
  bool highAdjPlaneOccupied = !pos
    ? KAdjNeighIdxMask[planeId][1] & siblingOccupancy
    : (gnp.neighPattern >> kAdjNeighIdxFromPlanePos[planeId][1]) & 1;

  int adjPlanes = (highAdjPlaneOccupied << 1) | lowAdjPlaneOccupied;
  int planeBit = encodePlanarMode(
    planar, closestPlanarFlag, closestDist, adjPlanes, planeId, contextAngle,
    multiPlanarFlag, multiPlanarEligible, planarRef);

  bool isPlanar = (planar.planarMode & planeSelector);

  planarRate[planeId] =
    (255 * planarRate[planeId] + (isPlanar ? 256 * 8 : 0) + 128) >> 8;

  if (planeBuffer)
    row[rowLen - 1] = {unsigned(maxCoord), planeBit};

  bool isPlanarRef = (planarRef.planarMode & planeSelector);
  int planeBitRef = (planarRef.planePosBits & planeSelector) == 0 ? 0 : 1;

  if (!((isPlanar == isPlanarRef) && (planeBit == planeBitRef))) {
    planar.isPreDirMatch = false;
  }
}

//============================================================================
// determine Planar mode for all directions

void
GeometryOctreeEncoder::determinePlanarMode(
  bool adjacent_child_contextualization_enabled_flag,
  int occupancy,
  const bool planarEligible[3],
  int posInParent,
  const GeometryNeighPattern& gnp,
  PCCOctree3Node& node,
  OctreeNodePlanar& planar,
  int contextAngle,
  int contextAnglePhiX,
  int contextAnglePhiY,
  OctreeNodePlanar& planarRef)
{
  auto& planeBuffer = _planar._planarBuffer;

  setPlanesFromOccupancy(occupancy, planar);

  uint8_t planarEligibleMask = 0;
  planarEligibleMask |= planarEligible[2] << 2;
  planarEligibleMask |= planarEligible[1] << 1;
  planarEligibleMask |= planarEligible[0] << 0;
  planar.planarMode &= planarEligibleMask;
  planar.planePosBits &= planarEligibleMask;

  int xx = node.pos[0];
  int yy = node.pos[1];
  int zz = node.pos[2];

  planarRef.planarMode &= planarEligibleMask;
  planarRef.planePosBits &= planarEligibleMask;

  bool matchDir[3] = {false, false, false};
  const int mask1[3] = {6, 5, 3};

  if (planar.allowPCM) {
    for (int planeId = 0; planeId < 3; planeId++) {
      const int mask0 = (1 << planeId);
      bool isPlanar = planar.planarMode & mask0;
      int planeBit = (planar.planePosBits & mask0) == 0 ? 0 : 1;

      bool isPlanarRef = planarRef.planarMode & mask0;
      int planeBitRef = (planarRef.planePosBits & mask0) == 0 ? 0 : 1;

      if (planarEligible[planeId]) {
        matchDir[planeId] =
          (isPlanar == isPlanarRef) && (planeBit == planeBitRef);
      } else {
        matchDir[planeId] = true;
      }
    }
  }

  planar.isPCM = planar.allowPCM && matchDir[0] && matchDir[1] && matchDir[2];

  if (planar.allowPCM) {
    derivePlanarPCMContextBuffer(
      planar, planarRef, planeBuffer, xx, yy, zz, planeBuffer.getBuffer(0),
      planeBuffer.getBuffer(1), planeBuffer.getBuffer(2));
  }

  if (!planar.isSignaled && planar.allowPCM) {
    _arithmeticEncoder->encode(
      planar.isPCM,
      _ctxPlanarCopyMode[planarRef.ctxBufPCM][planarRef.planarMode]);
    planar.isSignaled = true;
  }

  bool multiPlanarEligible[4] = {false, false, false, false};  //xyz,xy,xz,yz
  bool multiPlanarFlag[4] = {false, false, false, false};      //xyz,xy,xz,yz

  if (_planar._geom_multiple_planar_mode_enable_flag) {
    if (!planar.isPCM) {
      // determine what planes exist in occupancy
      if (planarEligible[2] && planarEligible[1] && planarEligible[0]) {  //xyz
        multiPlanarEligible[0] = true;
        multiPlanarFlag[0] = !popcntGt1(occupancy);
        _arithmeticEncoder->encode(multiPlanarFlag[0], _ctxMultiPlanarMode);
      } else if (
        (!planarEligible[2]) && planarEligible[1] && planarEligible[0]) {  //xy
        multiPlanarEligible[1] = true;
        multiPlanarFlag[1] =
          (planar.planarMode & 1) && (planar.planarMode & 2);
        _arithmeticEncoder->encode(multiPlanarFlag[1], _ctxMultiPlanarMode);
      } else if (
        planarEligible[2] && (!planarEligible[1]) && planarEligible[0]) {  //xz
        multiPlanarEligible[2] = true;
        multiPlanarFlag[2] =
          (planar.planarMode & 1) && (planar.planarMode & 4);
        _arithmeticEncoder->encode(multiPlanarFlag[2], _ctxMultiPlanarMode);
      } else if (
        planarEligible[2] && planarEligible[1] && (!planarEligible[0])) {  //yz
        multiPlanarEligible[3] = true;
        multiPlanarFlag[3] =
          (planar.planarMode & 2) && (planar.planarMode & 4);
        _arithmeticEncoder->encode(multiPlanarFlag[3], _ctxMultiPlanarMode);
      }
    }
  }


  // planar x
  if (planarEligible[0]) {
    determinePlanarMode(
      adjacent_child_contextualization_enabled_flag, 0, planar,
      planeBuffer.getBuffer(0), yy, zz, xx, posInParent, gnp,
      node.siblingOccupancy, _planar._rate.data(), contextAnglePhiX,
      multiPlanarFlag, multiPlanarEligible, planarRef);
  }
  // planar y
  if (planarEligible[1]) {
    determinePlanarMode(
      adjacent_child_contextualization_enabled_flag, 1, planar,
      planeBuffer.getBuffer(1), xx, zz, yy, posInParent, gnp,
      node.siblingOccupancy, _planar._rate.data(), contextAnglePhiY,
      multiPlanarFlag, multiPlanarEligible, planarRef);
  }
  // planar z
  if (planarEligible[2]) {
    determinePlanarMode(
      adjacent_child_contextualization_enabled_flag, 2, planar,
      planeBuffer.getBuffer(2), xx, yy, zz, posInParent, gnp,
      node.siblingOccupancy, _planar._rate.data(), contextAngle,
      multiPlanarFlag, multiPlanarEligible, planarRef);
  }
}

//-------------------------------------------------------------------------
// encode occupancy bits (neighPattern10 == 0 case)

void
GeometryOctreeEncoder::encodeOccupancyNeighZsimple(
  int mappedOccupancy,
  int mappedPlanarMaskX,
  bool planarPossibleX,
  int mappedPlanarMaskY,
  bool planarPossibleY,
  int mappedPlanarMaskZ,
  bool planarPossibleZ,
  int predOcc)
{
  // NB: if not predicted, miniumum num occupied is 2 due to singleChild
  int minOccupied = predOcc ? 1 : 2;
  int threshold = 8 - minOccupied;

  int numOccupiedAcc = 0;

  int maxPerPlaneX = mappedPlanarMaskX ? 2 : 3;
  int maxPerPlaneY = mappedPlanarMaskY ? 2 : 3;
  int maxPerPlaneZ = mappedPlanarMaskZ ? 2 : 3;
  bool sure_planarityX = mappedPlanarMaskX || !planarPossibleX;
  bool sure_planarityY = mappedPlanarMaskY || !planarPossibleY;
  bool sure_planarityZ = mappedPlanarMaskZ || !planarPossibleZ;

  int maskedOccupancy =
    mappedPlanarMaskX | mappedPlanarMaskY | mappedPlanarMaskZ;
  int MaskConfig = !mappedPlanarMaskX ? 0 : mappedPlanarMaskX == 15 ? 1 : 2;
  MaskConfig += !mappedPlanarMaskY ? 0 : mappedPlanarMaskY == 51 ? 3 : 6;
  MaskConfig += !mappedPlanarMaskZ ? 0 : mappedPlanarMaskZ == 85 ? 9 : 18;
  static const int LUinit[27][6] = {
    {0, 0, 0, 0, 0, 0}, {4, 0, 2, 2, 2, 2}, {0, 4, 2, 2, 2, 2},
    {2, 2, 4, 0, 2, 2}, {4, 2, 4, 2, 3, 3}, {2, 4, 4, 2, 3, 3},
    {2, 2, 0, 4, 2, 2}, {4, 2, 2, 4, 3, 3}, {2, 4, 2, 4, 3, 3},
    {2, 2, 2, 2, 4, 0}, {4, 2, 3, 3, 4, 2}, {2, 4, 3, 3, 4, 2},
    {3, 3, 4, 2, 4, 2}, {4, 3, 4, 3, 4, 3}, {3, 4, 4, 3, 4, 3},
    {3, 3, 2, 4, 4, 2}, {4, 3, 3, 4, 4, 3}, {3, 4, 3, 4, 4, 3},
    {2, 2, 2, 2, 0, 4}, {4, 2, 3, 3, 2, 4}, {2, 4, 3, 3, 2, 4},
    {3, 3, 4, 2, 2, 4}, {4, 3, 4, 3, 3, 4}, {3, 4, 4, 3, 3, 4},
    {3, 3, 2, 4, 2, 4}, {4, 3, 3, 4, 3, 4}, {3, 4, 3, 4, 3, 4}};

  const int* vinit = LUinit[MaskConfig];
  int coded0[6] = {vinit[0], vinit[1], vinit[2],
                   vinit[3], vinit[4], vinit[5]};  // mask x0 x1 y0 y1 z0 z1

  for (int i = 0; i < 8; i++) {
    // masking for planar is here
    if ((maskedOccupancy >> i) & 1)
      continue;

    //  -- avoid coding the occupancyBit if it is implied.
    int mask0X = (0xf0 >> i) & 1;
    bool bitIsOneX = (sure_planarityX && coded0[mask0X] >= maxPerPlaneX)
      || (coded0[0] + coded0[1] >= threshold);
    int mask0Y = 2 + ((0xcc >> i) & 1);
    bool bitIsOneY = (sure_planarityY && coded0[mask0Y] >= maxPerPlaneY)
      || (coded0[0] + coded0[1] >= threshold);
    int mask0Z = 4 + ((0xaa >> i) & 1);
    bool bitIsOneZ = (sure_planarityZ && coded0[mask0Z] >= maxPerPlaneZ)
      || (coded0[0] + coded0[1] >= threshold);

    int bitPred = (predOcc >> i) & 1;
    int interCtx = bitPred;

    int bit = (mappedOccupancy >> i) & 1;
    if (!(bitIsOneX || bitIsOneY || bitIsOneZ)) {
      _arithmeticEncoder->encode(bit, _ctxZ[i][numOccupiedAcc][interCtx]);

      coded0[mask0X] += !bit;
      coded0[mask0Y] += !bit;
      coded0[mask0Z] += !bit;
    }

    numOccupiedAcc += bit;
  }
}

//-------------------------------------------------------------------------
// encode node occupancy bits
//

void
GeometryOctreeEncoder::encodeOccupancyFullNeihbourgsNZ(
  int neighPattern,
  int occupancy,
  int Word4[8],
  int Word7Adj[8],
  bool Sparse[8],
  int planarMaskX,
  int planarMaskY,
  int planarMaskZ,
  bool planarPossibleX,
  bool planarPossibleY,
  bool planarPossibleZ,
  int predOcc)
{
  static const int LUTinitCoded0[27][6] = {
    {0, 0, 0, 0, 0, 0}, {4, 0, 2, 2, 2, 2}, {0, 4, 2, 2, 2, 2},
    {2, 2, 4, 0, 2, 2}, {4, 2, 4, 2, 3, 3}, {2, 4, 4, 2, 3, 3},
    {2, 2, 0, 4, 2, 2}, {4, 2, 2, 4, 3, 3}, {2, 4, 2, 4, 3, 3},
    {2, 2, 2, 2, 4, 0}, {4, 2, 3, 3, 4, 2}, {2, 4, 3, 3, 4, 2},
    {3, 3, 4, 2, 4, 2}, {4, 3, 4, 3, 4, 3}, {3, 4, 4, 3, 4, 3},
    {3, 3, 2, 4, 4, 2}, {4, 3, 3, 4, 4, 3}, {3, 4, 3, 4, 4, 3},
    {2, 2, 2, 2, 0, 4}, {4, 2, 3, 3, 2, 4}, {2, 4, 3, 3, 2, 4},
    {3, 3, 4, 2, 2, 4}, {4, 3, 4, 3, 3, 4}, {3, 4, 4, 3, 3, 4},
    {3, 3, 2, 4, 2, 4}, {4, 3, 3, 4, 3, 4}, {3, 4, 3, 4, 3, 4}};
  static const int LUTw[16] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 6};
  static const int LUTmask[16] = {2, 1, 8, 4, 2, 1, 2, 1,
                                  4, 2, 2, 1, 1, 4, 1, 1};

  bool sure_planarityX = planarMaskX || !planarPossibleX;
  bool sure_planarityY = planarMaskY || !planarPossibleY;
  bool sure_planarityZ = planarMaskZ || !planarPossibleZ;

  int MaskConfig = !planarMaskX ? 0 : planarMaskX == 15 ? 1 : 2;
  MaskConfig += !planarMaskY ? 0 : planarMaskY == 51 ? 3 : 6;
  MaskConfig += !planarMaskZ ? 0 : planarMaskZ == 85 ? 9 : 18;

  const int* vinit = LUTinitCoded0[MaskConfig];
  int coded0[6] = {vinit[0], vinit[1], vinit[2],
                   vinit[3], vinit[4], vinit[5]};  // mask x0 x1 y0 y1 z0 z1

  // loop on occupancy bits from occupancy map
  uint32_t partialOccupancy = 0;
  int maskedOccupancy = planarMaskX | planarMaskY | planarMaskZ;
  for (int i = 0; i < 8; i++) {
    if (
      (maskedOccupancy >> i)
      & 1) {  // bit is 0 because masked by QTBT or planar
      partialOccupancy <<= 1;
      continue;
    }

    int mask0X = (0xf0 >> i) & 1;
    bool bitIsOneX =
      (sure_planarityX && coded0[mask0X] >= 3) || (coded0[0] + coded0[1] >= 7);

    int mask0Y = 2 + ((0xcc >> i) & 1);
    bool bitIsOneY =
      (sure_planarityY && coded0[mask0Y] >= 3) || (coded0[2] + coded0[3] >= 7);

    int mask0Z = 4 + ((0xaa >> i) & 1);
    bool bitIsOneZ =
      (sure_planarityZ && coded0[mask0Z] >= 3) || (coded0[4] + coded0[5] >= 7);

    if (bitIsOneX || bitIsOneY || bitIsOneZ) {  // bit is 1
      partialOccupancy <<= 1;
      partialOccupancy |= 1;
      continue;
    }

    // OBUF contexts
    int comp = i << 1;
    int ctxComp = !(Word4[LUTw[comp]] & LUTmask[comp++]) << 1;
    ctxComp |= !(Word4[LUTw[comp]] & LUTmask[comp++]);
    int ctx2 = (Word4[i] << 2) | ctxComp;

    // encode
    int bit = (occupancy >> i) & 1;

    int bitPred = (predOcc >> i) & 1;
    int interCtx = bitPred;

    if (Sparse[i]) {
      ctx2 |= (Word7Adj[i] & 31) << 6;
      int ctx1 = ((Word7Adj[i] >> 5) << i) | partialOccupancy;
      _arithmeticEncoder->encode(
        bit,
        _ctxMapOccupancy[_MapOccupancySparse[interCtx][i].getEvolve(
          bit, ctx2, ctx1)]);
    } else {
      ctx2 |= (Word7Adj[i] & 7) << 6;
      int ctx1 = ((Word7Adj[i] >> 3) << i) | partialOccupancy;
      _arithmeticEncoder->encode(
        bit,
        _ctxMapOccupancy[_MapOccupancy[interCtx][i].getEvolve(
          bit, ctx2, ctx1)]);
    }

    // update partial occupancy of current node
    coded0[mask0X] += !bit;
    coded0[mask0Y] += !bit;
    coded0[mask0Z] += !bit;
    partialOccupancy <<= 1;
    partialOccupancy |= bit;
  }
}

//-------------------------------------------------------------------------
// encode node occupancy bits
//
void
GeometryOctreeEncoder::encodeOccupancyFullNeihbourgs(
  int neighPattern,
  int occupancy,
  int planarMaskX,
  int planarMaskY,
  int planarMaskZ,
  bool planarPossibleX,
  bool planarPossibleY,
  bool planarPossibleZ,
  const MortonMap3D& occupancyAtlas,
  Vec3<int32_t> pos,
  const int atlasShift,
  bool flagWord4,
  bool adjacent_child_contextualization_enabled_flag,
  int predOcc)
{
  // 3 planars => single child and we know its position
  if (planarMaskX && planarMaskY && planarMaskZ)
    return;
  if (
    neighPattern == 0
    && (!predOcc || (planarMaskX | planarMaskY | planarMaskZ))) {
    bool singleChild = !popcntGt1(occupancy);
    if (planarPossibleX && planarPossibleY && planarPossibleZ) {
      _arithmeticEncoder->encode(singleChild, _ctxSingleChild);
    }

    if (singleChild) {
      // no siblings => encode index = (z,y,x) not 8bit pattern
      // if mask is not zero, then planar, then child z known from plane index
      if (!planarMaskZ)
        _arithmeticEncoder->encode(!!(occupancy & 0xaa));

      if (!planarMaskY)
        _arithmeticEncoder->encode(!!(occupancy & 0xcc));

      if (!planarMaskX)
        _arithmeticEncoder->encode(!!(occupancy & 0xf0));

      return;
    }

    // at least two child nodes occupied and two planars => we know the occupancy
    if (planarMaskX && planarMaskY)
      return;
    if (planarMaskY && planarMaskZ)
      return;
    if (planarMaskX && planarMaskZ)
      return;

    encodeOccupancyNeighZsimple(
      occupancy, planarMaskX, planarPossibleX, planarMaskY, planarPossibleY,
      planarMaskZ, planarPossibleZ, predOcc);
  }
  //------ NZ occupancy encoding from here ----------------
  else {
    int Word4[8] = {0, 0, 0, 0,
                    0, 0, 0, 0};  // occupancy pattern for 3 edges + 1 vertex
    int Word7Adj[8] = {
      0, 0, 0, 0, 0,
      0, 0, 0};  // 7 bits: 0=FaceL 1=FaceF 2=FaceB  / 3=EdgeLF 4=EdgeLB 5=Edge FB / 6=VertexLFB
    bool Sparse[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (flagWord4) {
      construct26NeighbourWord(occupancyAtlas, pos, atlasShift, Word4);
      if (adjacent_child_contextualization_enabled_flag)
        makeGeometryAdvancedNeighPattern(
          neighPattern, pos, atlasShift, occupancyAtlas, Word7Adj, Sparse);
    }
    encodeOccupancyFullNeihbourgsNZ(
      neighPattern, occupancy, Word4, Word7Adj, Sparse, planarMaskX,
      planarMaskY, planarMaskZ, planarPossibleX, planarPossibleY,
      planarPossibleZ, predOcc);
  }
}

//-------------------------------------------------------------------------
// Encode part of the position of two unordred points  point in a given volume.
void
GeometryOctreeEncoder::encodeOrdered2ptPrefix(
  const point_t points[2], Vec3<bool> directIdcm, Vec3<int>& nodeSizeLog2)
{
  if (nodeSizeLog2[0] >= 1 && directIdcm[0]) {
    bool sameBit = true;
    int ctxIdx = 0;
    while (nodeSizeLog2[0] && sameBit) {
      nodeSizeLog2[0]--;
      int mask = 1 << nodeSizeLog2[0];
      auto bit0 = !!(points[0][0] & mask);
      auto bit1 = !!(points[1][0] & mask);
      sameBit = bit0 == bit1;

      _arithmeticEncoder->encode(sameBit, _ctxSameBitHighx[ctxIdx]);
      ctxIdx = std::min(4, ctxIdx + 1);
      if (sameBit)
        _arithmeticEncoder->encode(bit0);
    }
  }

  if (nodeSizeLog2[1] >= 1 && directIdcm[1]) {
    bool sameX = !directIdcm[0] || points[0][0] == points[1][0];
    bool sameBit = true;
    int ctxIdx = 0;
    while (nodeSizeLog2[1] && sameBit) {
      nodeSizeLog2[1]--;
      int mask = 1 << nodeSizeLog2[1];
      auto bit0 = !!(points[0][1] & mask);
      auto bit1 = !!(points[1][1] & mask);
      sameBit = bit0 == bit1;

      _arithmeticEncoder->encode(sameBit, _ctxSameBitHighy[ctxIdx]);
      ctxIdx = std::min(4, ctxIdx + 1);
      if (!(sameX && !sameBit))
        _arithmeticEncoder->encode(bit0);
    }
  }

  if (nodeSizeLog2[2] >= 1 && directIdcm[2]) {
    bool sameBit = true;
    bool sameXy = (!directIdcm[0] || points[0][0] == points[1][0])
      && (!directIdcm[1] || points[0][1] == points[1][1]);
    int ctxIdx = 0;
    while (nodeSizeLog2[2] && sameBit) {
      nodeSizeLog2[2]--;
      int mask = 1 << nodeSizeLog2[2];
      auto bit0 = !!(points[0][2] & mask);
      auto bit1 = !!(points[1][2] & mask);
      sameBit = bit0 == bit1;

      _arithmeticEncoder->encode(sameBit, _ctxSameBitHighz[ctxIdx]);
      ctxIdx = std::min(4, ctxIdx + 1);
      if (!(sameXy && !sameBit))
        _arithmeticEncoder->encode(bit0);
    }
  }
}

//-------------------------------------------------------------------------
// Encode a position of a point in a given volume.
void
GeometryOctreeEncoder::encodePointPosition(
  const Vec3<int>& nodeSizeLog2AfterPlanar, const Vec3<int32_t>& pos)
{
  for (int k = 0; k < 3; k++) {
    if (nodeSizeLog2AfterPlanar[k] <= 0)
      continue;

    for (int mask = 1 << (nodeSizeLog2AfterPlanar[k] - 1); mask; mask >>= 1) {
      _arithmeticEncoder->encode(!!(pos[k] & mask));
    }
  }
}

//-------------------------------------------------------------------------
// Encode a position of a point in a given volume, using elevation angle prior.
//
// pos is the point position to be coded.
// posXyz is initially the node position relative to the angular origin,
// and updated as bits are coded.

void
GeometryOctreeEncoder::encodePointPositionAngular(
  const OctreeAngPosScaler& quant,
  const OctreeNodePlanar& planar,
  const Vec3<int>& nodeSizeLog2Rem,
  Vec3<int> posXyz,
  const Vec3<int>& pos,
  int nodeLaserIdx,
  const Vec3<int>& angularOrigin,
  const int* zLaser,
  const int* thetaLaser,
  int numLasers)
{
  // -- PHI --
  // code x or y directly and compute phi of node
  bool directAxis = std::abs(posXyz[0]) <= std::abs(posXyz[1]);

  for (int mask = (1 << nodeSizeLog2Rem[directAxis]) >> 1; mask; mask >>= 1)
    _arithmeticEncoder->encode(!!(pos[directAxis] & mask));

  // update the known position to take into account all coded bits
  for (int k = 0; k < 3; k++)
    if (k != directAxis)
      if (planar.planePosBits & (1 << k))
        posXyz[k] += quant.scaleEns(k, 1 << nodeSizeLog2Rem[k]);

  posXyz[directAxis] =
    quant.scaleEns(directAxis, pos[directAxis]) - angularOrigin[directAxis];

  // Laser
  int laserIdx;

  if (_angularExtension)
    laserIdx = findLaserPrecise(
      quant.scaleEns(pos) - angularOrigin, thetaLaser, zLaser, numLasers);
  else
    laserIdx =
      findLaser(quant.scaleEns(pos) - angularOrigin, thetaLaser, numLasers);

  int resLaser = laserIdx - nodeLaserIdx;
  encodeThetaRes(resLaser, _prevLaserIndexResidual[nodeLaserIdx]);

  if (_angularExtension)
    _prevLaserIndexResidual[nodeLaserIdx] = resLaser;

  // find predictor
  const int thInterp = 1 << 13;

  int phiNode = iatan2(posXyz[1], posXyz[0]);
  int phiTop = directAxis
    ? iatan2(posXyz[1], posXyz[0] + (1 << nodeSizeLog2Rem[!directAxis]))
    : iatan2(posXyz[1] + (1 << nodeSizeLog2Rem[!directAxis]), posXyz[0]);
  int phiMiddle = (phiNode + phiTop) >> 1;
  if (_angularExtension && !(std::abs(phiNode - phiTop) < thInterp))
    phiMiddle = directAxis
      ? iatan2(
          posXyz[1], posXyz[0] + ((1 << nodeSizeLog2Rem[!directAxis]) >> 1))
      : iatan2(
          posXyz[1] + ((1 << nodeSizeLog2Rem[!directAxis]) >> 1), posXyz[0]);

  int predPhi = _phiBuffer[laserIdx];
  int phiRef = _angularExtension ? phiMiddle : phiNode;
  if (predPhi == 0x80000000)
    predPhi = phiRef;

  // elementary shift predictor
  int nShift =
    ((predPhi - phiRef) * _phiZi.invDelta(laserIdx) + (1 << 29)) >> 30;
  predPhi -= _phiZi.delta(laserIdx) * nShift;

  // azimuthal code x or y
  const int phiAxis = !directAxis;
  for (int mask = (1 << nodeSizeLog2Rem[phiAxis]) >> 1,
           shiftBits = nodeSizeLog2Rem[phiAxis];
       mask; mask >>= 1, shiftBits--) {
    // angles left and right
    int scaledMask = quant.scaleEns(phiAxis, mask);
    int phiL, phiR;

    if (_angularExtension) {
      const int offset = scaledMask - 1;
      const int offset2 = shiftBits > 1 ? (shiftBits > 2 ? 0 : 1) : 2;

      phiL =
        phiNode + ((offset - offset2) * (phiMiddle - phiNode) >> (shiftBits));
      phiR = phiMiddle
        + ((offset + offset2) * (phiMiddle - phiNode) >> (shiftBits));
    } else {
      phiL = phiNode;
      phiR = directAxis ? iatan2(posXyz[1], posXyz[0] + scaledMask)
                        : iatan2(posXyz[1] + scaledMask, posXyz[0]);
    }

    // ctx azimutal
    int angleL = phiL - predPhi;
    int angleR = phiR - predPhi;
    int contextAnglePhi =
      (angleL >= 0 && angleR >= 0) || (angleL < 0 && angleR < 0) ? 2 : 0;
    angleL = std::abs(angleL);
    angleR = std::abs(angleR);
    if (angleL > angleR) {
      contextAnglePhi++;
      std::swap(angleL, angleR);
    }
    if (angleR > (angleL << 1))
      contextAnglePhi += 4;

    // entropy coding
    int bit = !!(pos[phiAxis] & mask);
    int ctxIndex = 0;
    if (_angularExtension)
      ctxIndex = determineContextIndexForAngularPhiIDCM(
        _phiZi.delta(laserIdx), std::abs(phiL - phiR));
    auto& ctx =
      _ctxPlanarPlaneLastIndexAngularPhiIDCM[contextAnglePhi][ctxIndex];
    _arithmeticEncoder->encode(bit, ctx);
    if (bit) {
      posXyz[phiAxis] += scaledMask;
      if (_angularExtension)
        phiNode = phiMiddle;
      else {
        phiNode = phiR;
        predPhi = _phiBuffer[laserIdx];
        if (predPhi == 0x80000000)
          predPhi = phiNode;

        // elementary shift predictor
        int nShift =
          ((predPhi - phiNode) * _phiZi.invDelta(laserIdx) + (1 << 29)) >> 30;
        predPhi -= _phiZi.delta(laserIdx) * nShift;
      }
    } else if (_angularExtension)
      phiTop = phiMiddle;

    if (_angularExtension) {
      // update Phi middle
      if (std::abs(phiNode - phiTop) < thInterp)
        phiMiddle = (phiNode + phiTop) >> 1;
      else
        phiMiddle = directAxis
          ? iatan2(posXyz[1], posXyz[0] + (scaledMask >> 1))
          : iatan2(posXyz[1] + (scaledMask >> 1), posXyz[0]);

      // update elementary shift predictor
      int nShift =
        ((predPhi - phiMiddle) * _phiZi.invDelta(laserIdx) + (1 << 29)) >> 30;
      predPhi -= _phiZi.delta(laserIdx) * nShift;
    }
  }

  _phiBuffer[laserIdx] = phiNode;

  // -- THETA --
  int maskz = (1 << nodeSizeLog2Rem[2]) >> 1;
  if (!maskz)
    return;

  // Since x and y are known,
  // r is known too and does not depend on the bit for z
  if (_angularExtension)
    encodePointPositionZAngularExtension(
      angularOrigin, pos, zLaser, thetaLaser, laserIdx, maskz, posXyz);
  else
    encodePointPositionZAngular(
      quant, nodeSizeLog2Rem, zLaser, thetaLaser, laserIdx, posXyz, pos[2]);
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodePointPositionZAngular(
  const OctreeAngPosScaler& quant,
  const Vec3<int>& nodeSizeLog2Rem,
  const int* zLaser,
  const int* thetaLaser,
  int laserIdx,
  Vec3<int>& posXyz,
  const int& posZ)
{
  uint64_t xLidar = (int64_t(posXyz[0]) << 8) - 128;
  uint64_t yLidar = (int64_t(posXyz[1]) << 8) - 128;
  uint64_t r2 = xLidar * xLidar + yLidar * yLidar;
  int64_t rInv = irsqrt(r2);

  // code z
  int64_t hr = zLaser[laserIdx] * rInv;
  int fixedThetaLaser =
    thetaLaser[laserIdx] + int(hr >= 0 ? -(hr >> 17) : ((-hr) >> 17));

  int maskz = (1 << nodeSizeLog2Rem[2]) >> 1;
  int zShift = rInv * quant.scaleEns(2, 1 << nodeSizeLog2Rem[2]) >> 18;
  for (; maskz; maskz >>= 1, zShift >>= 1) {
    // determine non-corrected theta
    int scaledMaskZ = quant.scaleEns(2, maskz);
    int64_t zLidar = ((posXyz[2] + scaledMaskZ) << 1) - 1;
    int64_t theta = zLidar * rInv;
    int theta32 = theta >= 0 ? theta >> 15 : -((-theta) >> 15);
    int thetaLaserDelta = fixedThetaLaser - theta32;

    int thetaLaserDeltaBot = thetaLaserDelta + zShift;
    int thetaLaserDeltaTop = thetaLaserDelta - zShift;
    int contextAngle = thetaLaserDelta >= 0 ? 0 : 1;
    if (thetaLaserDeltaTop >= 0)
      contextAngle += 2;
    else if (thetaLaserDeltaBot < 0)
      contextAngle += 2;

    int bit = !!(posZ & maskz);
    auto& ctx = _ctxPlanarPlaneLastIndexAngularIdcm[contextAngle];
    _arithmeticEncoder->encode(bit, ctx);
    if (bit)
      posXyz[2] += scaledMaskZ;
  }
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodePointPositionZAngularExtension(
  const Vec3<int>& angularOrigin,
  const Vec3<int>& nodePos,
  const int* zLaser,
  const int* thetaLaser,
  int laserIdx,
  int maskz,
  Vec3<int>& posXyz)
{
  uint64_t xLidar = (int64_t(posXyz[0]) << 8);
  uint64_t yLidar = (int64_t(posXyz[1]) << 8);
  uint64_t r2 = xLidar * xLidar + yLidar * yLidar;
  int64_t r = isqrt(r2);

  // encode z
  int64_t zRec26 = thetaLaser[laserIdx] * r;
  zRec26 -= int64_t(zLaser[laserIdx]) << 23;
  int32_t zRec = divExp2RoundHalfInf(zRec26, 26);

  zRec = std::max(zRec, posXyz[2]);
  zRec = std::min(zRec, posXyz[2] + (2 * maskz - 1));

  int32_t zRes = (nodePos[2] - angularOrigin[2]) - zRec;
  encodeZRes(zRes);
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeNodeQpOffetsPresent(bool flag)
{
  _arithmeticEncoder->encode(flag);
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeQpOffset(int dqp)
{
  _arithmeticEncoder->encode(dqp != 0, _ctxQpOffsetAbsGt0);
  if (dqp == 0)
    return;

  _arithmeticEncoder->encodeExpGolomb(abs(dqp) - 1, 0, _ctxQpOffsetAbsEgl);
  _arithmeticEncoder->encode(dqp < 0, _ctxQpOffsetSign);
}

//-------------------------------------------------------------------------

template<typename It>
void
setNodeQpsUniform(
  Vec3<int> nodeSizeLog2,
  int qp,
  int geom_qp_multiplier_log2,
  It nodesBegin,
  It nodesEnd)
{
  // Conformance: limit the qp such that it cannot overquantize the node
  qp = std::min(qp, nodeSizeLog2.min() * 8);
  assert(qp % (1 << geom_qp_multiplier_log2) == 0);

  for (auto it = nodesBegin; it != nodesEnd; ++it)
    it->qp = qp;
}

//-------------------------------------------------------------------------
// Sets QP randomly

template<typename It>
void
setNodeQpsRandom(
  Vec3<int> nodeSizeLog2,
  int /* qp */,
  int geom_qp_multiplier_log2,
  It nodesBegin,
  It nodesEnd)
{
  // Conformance: limit the qp such that it cannot overquantize the node
  int maxQp = nodeSizeLog2.min() * 8;

  int seed = getenv("SEED") ? atoi(getenv("SEED")) : 0;
  static std::minstd_rand gen(seed);
  std::uniform_int_distribution<> uniform(0, maxQp);

  // pick a random qp, avoiding unrepresentable values
  for (auto it = nodesBegin; it != nodesEnd; ++it)
    it->qp = uniform(gen) & (~0 << geom_qp_multiplier_log2);
}

//-------------------------------------------------------------------------
// determine delta qp for each node based on the point density

template<typename It>
void
setNodeQpsByDensity(
  Vec3<int> nodeSizeLog2,
  int baseQp,
  int geom_qp_multiplier_log2,
  It nodesBegin,
  It nodesEnd)
{
  // Conformance: limit the qp such that it cannot overquantize the node
  int maxQp = nodeSizeLog2.min() * 8;
  int lowQp = PCCClip(baseQp - 8, 0, maxQp);
  int mediumQp = std::min(baseQp, maxQp);
  int highQp = std::min(baseQp + 8, maxQp);

  // NB: node.qp always uses a step size doubling interval of 8 QPs.
  //     the chosen QPs (and conformance limit) must respect the qp multiplier
  assert(lowQp % (1 << geom_qp_multiplier_log2) == 0);
  assert(mediumQp % (1 << geom_qp_multiplier_log2) == 0);
  assert(highQp % (1 << geom_qp_multiplier_log2) == 0);

  std::vector<int> numPointsInNode;
  std::vector<double> cum_prob;
  int32_t numPointsInLvl = 0;
  for (auto it = nodesBegin; it != nodesEnd; ++it) {
    numPointsInNode.push_back(it->end - it->start);
    numPointsInLvl += it->end - it->start;
  }
  std::sort(numPointsInNode.begin(), numPointsInNode.end());
  double cc = 0;
  for (auto num : numPointsInNode) {
    cc += num;
    cum_prob.push_back(cc / numPointsInLvl);
  }
  int th1 = -1, th2 = -1;
  for (int i = 0; i < cum_prob.size(); i++) {
    if (th1 == -1 && cum_prob[i] > 0.05) {
      th1 = numPointsInNode[i];
    } else if (th2 == -1 && cum_prob[i] > 0.6)
      th2 = numPointsInNode[i];
  }
  for (auto it = nodesBegin; it != nodesEnd; ++it) {
    if (it->end - it->start < th1) {
      it->qp = highQp;
    } else if (it->end - it->start < th2)
      it->qp = mediumQp;
    else
      it->qp = lowQp;
  }
}

//-------------------------------------------------------------------------

template<typename It>
void
calculateNodeQps(
  OctreeEncOpts::QpMethod method,
  Vec3<int> nodeSizeLog2,
  int baseQp,
  int geom_qp_multiplier_log2,
  It nodesBegin,
  It nodesEnd)
{
  auto fn = &setNodeQpsUniform<It>;

  switch (method) {
    using Method = OctreeEncOpts::QpMethod;
  default:
  case Method::kUniform: fn = &setNodeQpsUniform<It>; break;
  case Method::kRandom: fn = &setNodeQpsRandom<It>; break;
  case Method::kByDensity: fn = &setNodeQpsByDensity<It>; break;
  }

  fn(nodeSizeLog2, baseQp, geom_qp_multiplier_log2, nodesBegin, nodesEnd);
}

//-------------------------------------------------------------------------

void
geometryQuantization(
  PCCPointSet3& pointCloud, PCCOctree3Node& node, Vec3<int> nodeSizeLog2)
{
  QuantizerGeom quantizer = QuantizerGeom(node.qp);
  int qpShift = QuantizerGeom::qpShift(node.qp);

  for (int k = 0; k < 3; k++) {
    int quantBitsMask = (1 << nodeSizeLog2[k]) - 1;
    int32_t clipMax = quantBitsMask >> qpShift;

    for (int i = node.start; i < node.end; i++) {
      int32_t pos = int32_t(pointCloud[i][k]);
      int32_t quantPos = quantizer.quantize(pos & quantBitsMask);
      quantPos = PCCClip(quantPos, 0, clipMax);

      // NB: this representation is: |ppppppqqq00|, which, except for
      // the zero padding, is the same as the decoder.
      pointCloud[i][k] = (pos & ~quantBitsMask) | (quantPos << qpShift);
    }
  }
}

//-------------------------------------------------------------------------

void
geometryScale(
  PCCPointSet3& pointCloud, PCCOctree3Node& node, Vec3<int> quantNodeSizeLog2)
{
  QuantizerGeom quantizer = QuantizerGeom(node.qp);
  int qpShift = QuantizerGeom::qpShift(node.qp);

  for (int k = 0; k < 3; k++) {
    int quantBitsMask = (1 << quantNodeSizeLog2[k]) - 1;
    for (int i = node.start; i < node.end; i++) {
      int pos = pointCloud[i][k];
      int lowPart = (pos & quantBitsMask) >> qpShift;
      int lowPartScaled = PCCClip(quantizer.scale(lowPart), 0, quantBitsMask);
      int highPartScaled = pos & ~quantBitsMask;
      pointCloud[i][k] = highPartScaled | lowPartScaled;
    }
  }
}

//-------------------------------------------------------------------------

void
checkDuplicatePoints(
  PCCPointSet3& pointCloud,
  PCCOctree3Node& node,
  std::vector<int>& pointIdxToDmIdx)
{
  auto first = PCCPointSet3::iterator(&pointCloud, node.start);
  auto last = PCCPointSet3::iterator(&pointCloud, node.end);

  std::set<Vec3<int32_t>> uniquePointsSet;
  for (auto i = first; i != last;) {
    if (uniquePointsSet.find(**i) == uniquePointsSet.end()) {
      uniquePointsSet.insert(**i);
      i++;
    } else {
      std::iter_swap(i, last - 1);
      last--;
      pointIdxToDmIdx[--node.end] = -2;  // mark as duplicate
    }
  }
}

//-------------------------------------------------------------------------
// Direct coding of position of points in node (early tree termination).

DirectMode
canEncodeDirectPosition(
  bool geom_unique_points_flag,
  const PCCOctree3Node& node,
  const PCCPointSet3& pointCloud)
{
  int numPoints = node.end - node.start;
  // Check for duplicated points only if there are less than 10.
  // NB: this limit is rather arbitrary
  if (numPoints > 10)
    return DirectMode::kUnavailable;

  bool allPointsAreEqual = numPoints > 1 && !geom_unique_points_flag;
  for (auto idx = node.start + 1; allPointsAreEqual && idx < node.end; idx++)
    allPointsAreEqual &= pointCloud[node.start] == pointCloud[idx];

  if (allPointsAreEqual)
    return DirectMode::kAllPointSame;

  if (numPoints > MAX_NUM_DM_LEAF_POINTS)
    return DirectMode::kUnavailable;

  return DirectMode::kTwoPoints;
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeIsIdcm(DirectMode mode)
{
  bool isIdcm = mode != DirectMode::kUnavailable;
  _arithmeticEncoder->encode(isIdcm, _ctxBlockSkipTh);
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeDirectPosition(
  bool geom_unique_points_flag,
  bool joint_2pt_idcm_enabled_flag,
  bool geom_angular_mode_enabled_flag,
  DirectMode mode,
  const Vec3<uint32_t>& quantMasks,
  const Vec3<int>& effectiveNodeSizeLog2,
  int shiftBits,
  const PCCOctree3Node& node,
  const OctreeNodePlanar& planar,
  PCCPointSet3& pointCloud,
  const Vec3<int>& angularOrigin,
  const int* zLaser,
  const int* thetaLaser,
  int numLasers)
{
  int numPoints = node.end - node.start;

  switch (mode) {
  case DirectMode::kUnavailable: return;

  case DirectMode::kTwoPoints:
    _arithmeticEncoder->encode(numPoints > 1, _ctxNumIdcmPointsGt1);
    if (!geom_unique_points_flag && numPoints == 1)
      _arithmeticEncoder->encode(0, _ctxDupPointCntGt0);
    break;

  case DirectMode::kAllPointSame:
    _arithmeticEncoder->encode(0, _ctxNumIdcmPointsGt1);
    _arithmeticEncoder->encode(1, _ctxDupPointCntGt0);
    _arithmeticEncoder->encode(numPoints - 1 > 1, _ctxDupPointCntGt1);
    if (numPoints - 1 > 1)
      _arithmeticEncoder->encodeExpGolomb(
        numPoints - 3, 0, _ctxDupPointCntEgl);

    // only one actual psoition to code
    numPoints = 1;
  }

  // if the points have been quantised, the following representation is used
  // for point cloud positions:
  //          |---| = nodeSizeLog2 (example)
  //   ppppppqqqq00 = cloud[ptidx]
  //          |-|   = effectiveNodeSizeLog2 (example)
  // where p are unquantised bits, qqq are quantised bits, and 0 are zero bits.
  // nodeSizeLog2 is the size of the current node prior to quantisation.
  // effectiveNodeSizeLog2 is the size of the node after quantisation.
  //
  // NB: while nodeSizeLog2 may be used to access the current position bit
  //     in both quantised and unquantised forms, effectiveNodeSizeLog2 cannot
  //     without taking into account the padding.
  //
  // NB: this contrasts with node.pos, which contains the previously coded
  //     position bits ("ppppppq" in the above example) without any padding.
  //
  // When coding the direct mode, the zero padding is removed to permit
  // indexing by the effective node size instead.
  Vec3<int> points[2];
  for (int i = 0; i < numPoints; i++)
    points[i] = pointCloud[node.start + i] >> shiftBits;

  OctreeAngPosScaler quant(node.qp, quantMasks);

  // update node size after planar
  Vec3<int> nodeSizeLog2Rem = effectiveNodeSizeLog2;
  for (int k = 0; k < 3; k++)
    if (nodeSizeLog2Rem[k] > 0 && (planar.planarMode & (1 << k)))
      nodeSizeLog2Rem[k]--;

  // Indicates which components are directly coded, or coded using angular
  // contextualisation.
  Vec3<bool> directIdcm = true;

  // Position of the node relative to the angular origin
  point_t posNodeLidar;
  if (geom_angular_mode_enabled_flag) {
    posNodeLidar = quant.scaleEns(node.pos << effectiveNodeSizeLog2);
    posNodeLidar -= angularOrigin;

    bool directAxis = std::abs(posNodeLidar[0]) <= std::abs(posNodeLidar[1]);
    directIdcm = false;
    directIdcm[directAxis] = true;
  }

  // Jointly code two points
  if (numPoints == 2 && joint_2pt_idcm_enabled_flag) {
    // Apply an implicit ordering to the two points, considering only the
    // directly coded axes
    if (times(points[1], directIdcm) < times(points[0], directIdcm)) {
      std::swap(points[0], points[1]);
      pointCloud.swapPoints(node.start, node.start + 1);
    }

    encodeOrdered2ptPrefix(points, directIdcm, nodeSizeLog2Rem);
  }

  int laserIdx;
  if (geom_angular_mode_enabled_flag) {
    auto delta = points[0] - (node.pos << effectiveNodeSizeLog2);
    delta = (delta >> nodeSizeLog2Rem) << nodeSizeLog2Rem;
    delta += (1 << nodeSizeLog2Rem) >> 1;
    delta = quant.scaleEns(delta);
    if (_angularExtension)
      laserIdx =
        findLaserPrecise(posNodeLidar + delta, thetaLaser, zLaser, numLasers);
    else
      laserIdx = findLaser(posNodeLidar + delta, thetaLaser, numLasers);
  }

  // code points after planar
  for (auto idx = 0; idx < numPoints; idx++) {
    if (geom_angular_mode_enabled_flag)
      encodePointPositionAngular(
        quant, planar, nodeSizeLog2Rem, posNodeLidar, points[idx], laserIdx,
        angularOrigin, zLaser, thetaLaser, numLasers);
    else
      encodePointPosition(nodeSizeLog2Rem, points[idx]);
  }
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeThetaRes(int thetaRes, int prevThetaRes)
{
  int ctx = prevThetaRes != 0;
  _arithmeticEncoder->encode(thetaRes != 0, _ctxThetaRes[ctx][0]);
  if (!thetaRes)
    return;

  int absVal = std::abs(thetaRes);
  _arithmeticEncoder->encode(--absVal > 0, _ctxThetaRes[ctx][1]);
  if (absVal)
    _arithmeticEncoder->encode(--absVal > 0, _ctxThetaRes[ctx][2]);
  if (absVal)
    _arithmeticEncoder->encodeExpGolomb(--absVal, 1, _ctxThetaResExp);

  int ctxSign = (prevThetaRes > 0) + 2 * (prevThetaRes < 0);
  _arithmeticEncoder->encode(thetaRes < 0, _ctxThetaResSign[ctxSign]);
}

//-------------------------------------------------------------------------

void
GeometryOctreeEncoder::encodeZRes(int zRes)
{
  _arithmeticEncoder->encode(zRes != 0, _ctxZRes[0]);
  if (!zRes)
    return;

  int absVal = std::abs(zRes);
  _arithmeticEncoder->encode(--absVal > 0, _ctxZRes[1]);
  if (absVal)
    _arithmeticEncoder->encode(--absVal > 0, _ctxZRes[2]);
  if (absVal)
    _arithmeticEncoder->encodeExpGolomb(--absVal, 1, _ctxZResExp);

  _arithmeticEncoder->encode(zRes < 0, _ctxZResSign);
}

//-------------------------------------------------------------------------
void
applyGlobalMotion(
  const InterGeomEncOpts& params,
  const SequenceParameterSet& sps,
  const GeometryParameterSet& gps,
  GeometryBrickHeader& gbh,
  PCCPointSet3& pointCloud,
  PCCPointSet3& predPointCloud,
  PCCPointSet3& pointPredictorWorld,
  EntropyEncoder* arithmeticEncoder)
{
  PCCPointSet3 pointCloudZeroOrigin;
  pointCloudZeroOrigin = pointCloud;
  // shift for matching origin position between predPointCloud and pointCloud
  for (int i = 0; i < pointCloud.getPointCount(); i++) {
    pointCloudZeroOrigin[i] += gbh.geomBoxOrigin;
  }
  Vec3<int> minimum_position;

  // search for global motion
  switch (params.motionSrc) {
  case InterGeomEncOpts::kExternalGMSrc:
    gbh.min_zero_origin_flag = false;
    minimum_position = sps.seqBoundingBoxOrigin;
    params.motionParams.getMotionParams(
      gbh.gm_thresh, gbh.gm_matrix, gbh.gm_trans);
    break;
  case InterGeomEncOpts::kInternalLMSGMSrc:
    if (params.lpuType != InterGeomEncOpts::kCuboidPartition) {
      params.motionParams.getMotionParams(
        gbh.gm_thresh, gbh.gm_matrix, gbh.gm_trans);
      gbh.gm_thresh.first -= sps.seqBoundingBoxOrigin[2];
      gbh.gm_thresh.second -= sps.seqBoundingBoxOrigin[2];
    }
    minimum_position = {0, 0, 0};
    gbh.min_zero_origin_flag = true;
    SearchGlobalMotionPerTile(
      pointCloudZeroOrigin, predPointCloud, sps.seqGeomScale, gbh,
      params.th_dist, params.useCuboidalRegionsInGMEstimation);
    break;
  case InterGeomEncOpts::kInternalICPGMSrc: break;
  }

  // compensation
  switch (params.lpuType) {
  case InterGeomEncOpts::kRoadObjClassfication:
    compensateWithRoadObjClassfication(
      pointPredictorWorld, gbh.gm_matrix, gbh.gm_trans, gbh.gm_thresh,
      minimum_position);
    break;
  case InterGeomEncOpts::kCuboidPartition:
    compensateWithCuboidPartition(
      pointCloudZeroOrigin, predPointCloud, pointPredictorWorld, gbh,
      params.motion_window_size, minimum_position, arithmeticEncoder);
    break;
  }
}

//-------------------------------------------------------------------------

void
encodeGeometryOctree(
  const OctreeEncOpts& params,
  const GeometryParameterSet& gps,
  GeometryBrickHeader& gbh,
  PCCPointSet3& pointCloud,
  GeometryOctreeContexts& ctxtMem,
  std::vector<std::unique_ptr<EntropyEncoder>>& arithmeticEncoders,
  pcc::ringbuf<PCCOctree3Node>* nodesRemaining,
  PCCPointSet3& predPointCloud,
  const SequenceParameterSet& sps,
  const InterGeomEncOpts& interParams)
{
  auto arithmeticEncoderIt = arithmeticEncoders.begin();
  GeometryOctreeEncoder encoder(gps, gbh, ctxtMem, arithmeticEncoderIt->get());

  // saved state for use with parallel bistream coding.
  // the saved state is restored at the start of each parallel octree level
  std::unique_ptr<GeometryOctreeEncoder> savedState;

   bool isInter = gbh.interPredictionEnabledFlag;

  // for inter prediction
  PCCPointSet3 pointPredictorWorld;
  if (isInter) {
    pointPredictorWorld = predPointCloud;

    if (gps.globalMotionEnabled) {
      applyGlobalMotion(
        interParams, sps, gps, gbh, pointCloud, predPointCloud,
        pointPredictorWorld, arithmeticEncoderIt->get());
    }

    for (int i = 0; i < pointPredictorWorld.getPointCount(); i++) {
      pointPredictorWorld[i] -= gbh.geomBoxOrigin;
    }
  }

  // init main fifo
  //  -- worst case size is the last level containing every input poit
  //     and each point being isolated in the previous level.
  pcc::ringbuf<PCCOctree3Node> fifo(pointCloud.getPointCount() + 1);

  // push the first node
  fifo.emplace_back();
  PCCOctree3Node& node00 = fifo.back();
  node00.start = uint32_t(0);
  node00.end = uint32_t(pointCloud.getPointCount());
  node00.pos = int32_t(0);

  node00.numSiblingsMispredicted = 0;
  node00.predEnd = uint32_t(pointPredictorWorld.getPointCount());
  node00.predStart = uint32_t(0);

  node00.numSiblingsPlus1 = 8;
  node00.siblingOccupancy = 0;
  node00.qp = 0;
  node00.idcmEligible = 0;

  // map of pointCloud idx to DM idx, used to reorder the points
  // after coding.
  std::vector<int> pointIdxToDmIdx(int(pointCloud.getPointCount()), -1);
  int nextDmIdx = 0;

  // generate the list of the node size for each level in the tree
  auto lvlNodeSizeLog2 = mkQtBtNodeSizeList(gps, params.qtbt, gbh);

  // rotating mask used to enable idcm
  uint32_t idcmEnableMaskInit = mkIdcmEnableMask(gps);

  //  Lidar angles for planar prediction
  const int numLasers = gps.numLasers();
  const int* thetaLaser = gps.angularTheta.data();
  const int* zLaser = gps.angularZ.data();

  // Lidar position relative to slice origin
  auto angularOrigin = gbh.geomAngularOrigin(gps);

  int deltaAngle = 128 << 18;
  for (int i = 0; i < numLasers - 1; i++) {
    int d = std::abs(thetaLaser[i] - thetaLaser[i + 1]);
    if (deltaAngle > d)
      deltaAngle = d;
  }

  MortonMap3D occupancyAtlas;
  if (gps.neighbour_avail_boundary_log2_minus1) {
    occupancyAtlas.resize(
      gps.adjacent_child_contextualization_enabled_flag,
      gps.neighbour_avail_boundary_log2_minus1 + 1);
    occupancyAtlas.clear();
  }

  // the minimum node size is ordinarily 2**0, but may be larger due to
  // early termination for trisoup.
  int minNodeSizeLog2 = gbh.trisoupNodeSizeLog2(gps);

  // prune anything smaller than the minimum node size (these won't be coded)
  // NB: this must result in a cubic node at the end of the list
  // NB: precondition: root node size >= minNodeSizeLog2.
  lvlNodeSizeLog2.erase(
    std::remove_if(
      lvlNodeSizeLog2.begin(), lvlNodeSizeLog2.end(),
      [&](Vec3<int>& size) { return size < minNodeSizeLog2; }),
    lvlNodeSizeLog2.end());
  assert(lvlNodeSizeLog2.back() == minNodeSizeLog2);

  // append a dummy entry to the list so that depth+2 access is always valid
  lvlNodeSizeLog2.emplace_back(lvlNodeSizeLog2.back());

  // the termination depth of the octree phase
  // NB: the tree depth may be greater than the maxNodeSizeLog2 due to
  //     perverse qtbt splitting.
  // NB: by definition, the last two elements are minNodeSizeLog2
  int maxDepth = lvlNodeSizeLog2.size() - 2;

  // generate the qtbt splitting list
  //  - start at the leaf, and work up
  std::vector<int8_t> tree_lvl_partition_list;
  for (int lvl = 0; lvl < maxDepth; lvl++) {
    gbh.tree_lvl_coded_axis_list.push_back(
      ~nonSplitQtBtAxes(lvlNodeSizeLog2[lvl], lvlNodeSizeLog2[lvl + 1]));

    // Conformance: at least one axis must attempt to be coded at each level
    assert(gbh.tree_lvl_coded_axis_list.back() != 0);
  }

  // the node size where quantisation is performed
  Vec3<int> quantNodeSizeLog2 = 0;
  Vec3<uint32_t> posQuantBitMasks = 0xffffffff;
  int idcmQp = 0;
  int sliceQp = gbh.sliceQp(gps);
  int numLvlsUntilQuantization = 0;
  if (gps.geom_scaling_enabled_flag) {
    numLvlsUntilQuantization = params.qpOffsetDepth;

    // Determine the desired quantisation depth after qtbt is determined
    if (params.qpOffsetNodeSizeLog2 > 0) {
      // find the first level that matches the scaling node size
      for (int lvl = 0; lvl < maxDepth; lvl++) {
        if (lvlNodeSizeLog2[lvl].min() > params.qpOffsetNodeSizeLog2)
          continue;
        numLvlsUntilQuantization = lvl;
        break;
      }
    }

    // if an invalid depth is set, use tree height instead
    if (numLvlsUntilQuantization < 0)
      numLvlsUntilQuantization = maxDepth;
    numLvlsUntilQuantization++;
  }

  // The number of nodes to wait before updating the planar rate.
  // This is to match the prior behaviour where planar is updated once
  // per coded occupancy.
  int nodesBeforePlanarUpdate = 1;

  if (gps.octree_point_count_list_present_flag)
    gbh.footer.octree_lvl_num_points_minus1.reserve(maxDepth);

  encoder.resetMap();

  bool planarEligibleKOctreeDepth = 0;
  int numPointsCodedByIdcm = 0;
  const bool checkPlanarEligibilityBasedOnOctreeDepth =
    gps.geom_planar_mode_enabled_flag
    && gps.geom_octree_depth_planar_eligibiity_enabled_flag
    && !gps.geom_angular_mode_enabled_flag;

  for (int depth = 0; depth < maxDepth; depth++) {
    int numSubnodes = 0;

    // The tree terminated early (eg, due to IDCM or quantisation)
    // Delete any unused arithmetic coders
    if (fifo.empty()) {
      ++arithmeticEncoderIt;
      arithmeticEncoders.erase(arithmeticEncoderIt, arithmeticEncoders.end());
      break;
    }

    // setyo at the start of each level
    auto fifoCurrLvlEnd = fifo.end();
    int numNodesNextLvl = 0;
    Vec3<int32_t> occupancyAtlasOrigin = 0xffffffff;

    // derive per-level node size related parameters
    auto nodeSizeLog2 = lvlNodeSizeLog2[depth];
    auto childSizeLog2 = lvlNodeSizeLog2[depth + 1];
    // represents the largest dimension of the current node
    int nodeMaxDimLog2 = nodeSizeLog2.max();

    // if one dimension is not split, atlasShift[k] = 0
    int codedAxesPrevLvl = depth ? gbh.tree_lvl_coded_axis_list[depth - 1] : 7;
    int codedAxesCurLvl = gbh.tree_lvl_coded_axis_list[depth];

    auto pointSortMask = qtBtChildSize(nodeSizeLog2, childSizeLog2);

    // Idcm quantisation applies to child nodes before per node qps
    if (--numLvlsUntilQuantization > 0) {
      // Indicate that the quantisation level has not been reached
      encoder.encodeNodeQpOffetsPresent(false);

      // If planar is enabled, the planar bits are not quantised (since
      // the planar mode is determined before quantisation)
      quantNodeSizeLog2 = nodeSizeLog2;
      if (gps.geom_planar_mode_enabled_flag)
        quantNodeSizeLog2 -= 1;

      for (int k = 0; k < 3; k++)
        quantNodeSizeLog2[k] = std::max(0, quantNodeSizeLog2[k]);

      // limit the idcmQp such that it cannot overquantise the node
      auto minNs = quantNodeSizeLog2.min();
      idcmQp = gps.geom_base_qp + gps.geom_idcm_qp_offset;
      idcmQp <<= gps.geom_qp_multiplier_log2;
      idcmQp = std::min(idcmQp, minNs * 8);
      for (int k = 0; k < 3; k++)
        posQuantBitMasks[k] = (1 << quantNodeSizeLog2[k]) - 1;
    }

    // determing a per node QP at the appropriate level
    if (!numLvlsUntilQuantization) {
      // Indicate that this is the level where per-node QPs are signalled.
      encoder.encodeNodeQpOffetsPresent(true);

      // idcm qps are no longer independent
      idcmQp = 0;
      quantNodeSizeLog2 = nodeSizeLog2;
      for (int k = 0; k < 3; k++)
        posQuantBitMasks[k] = (1 << quantNodeSizeLog2[k]) - 1;
      calculateNodeQps(
        params.qpMethod, nodeSizeLog2, sliceQp, gps.geom_qp_multiplier_log2,
        fifo.begin(), fifoCurrLvlEnd);
    }

    // save context state for parallel coding
    if (depth == maxDepth - 1 - gbh.geom_stream_cnt_minus1)
      if (gbh.geom_stream_cnt_minus1)
        savedState.reset(new GeometryOctreeEncoder(encoder));

    // load context state for parallel coding starting one level later
    if (depth > maxDepth - 1 - gbh.geom_stream_cnt_minus1) {
      encoder = *savedState;
      encoder._arithmeticEncoder = (++arithmeticEncoderIt)->get();
    }

    // reset the idcm eligibility mask at the start of each level to
    // support multiple streams
    auto idcmEnableMask = rotateRight(idcmEnableMaskInit, depth);

    auto planarDepth = gbh.rootNodeSizeLog2 - nodeSizeLog2;
    encoder.beginOctreeLevel(planarDepth);

    // process all nodes within a single level
    for (; fifo.begin() != fifoCurrLvlEnd; fifo.pop_front()) {
      PCCOctree3Node& node0 = fifo.front();

      // encode delta qp for each octree block
      if (numLvlsUntilQuantization == 0) {
        int qpOffset = (node0.qp - sliceQp) >> gps.geom_qp_multiplier_log2;
        encoder.encodeQpOffset(qpOffset);
      }

      int shiftBits = QuantizerGeom::qpShift(node0.qp);
      auto effectiveNodeSizeLog2 = nodeSizeLog2 - shiftBits;
      auto effectiveChildSizeLog2 = childSizeLog2 - shiftBits;

      // make quantisation work with qtbt and planar.
      int codedAxesCurNode = codedAxesCurLvl;
      if (shiftBits != 0) {
        for (int k = 0; k < 3; k++) {
          if (effectiveChildSizeLog2[k] < 0)
            codedAxesCurNode &= ~(4 >> k);
        }
      }

      if (numLvlsUntilQuantization == 0) {
        geometryQuantization(pointCloud, node0, quantNodeSizeLog2);
        if (gps.geom_unique_points_flag)
          checkDuplicatePoints(pointCloud, node0, pointIdxToDmIdx);
      }

      GeometryNeighPattern gnp{};
      // The position of the node in the parent's occupancy map
      int posInParent = 0;
      posInParent |= (node0.pos[0] & 1) << 2;
      posInParent |= (node0.pos[1] & 1) << 1;
      posInParent |= (node0.pos[2] & 1) << 0;
      posInParent &= codedAxesPrevLvl;

      if (gps.neighbour_avail_boundary_log2_minus1) {
        updateGeometryOccupancyAtlas(
          node0.pos, codedAxesPrevLvl, fifo, fifoCurrLvlEnd, &occupancyAtlas,
          &occupancyAtlasOrigin);
        gnp = makeGeometryNeighPattern(
          node0.pos, codedAxesPrevLvl, occupancyAtlas);
      } else {
        gnp.neighPattern =
          neighPatternFromOccupancy(posInParent, node0.siblingOccupancy);
      }

      // split the current node into 8 children
      //  - perform an 8-way counting sort of the current node's points
      //  - (later) map to child nodes
      std::array<int, 8> childCounts = {};
      countingSort(
        PCCPointSet3::iterator(&pointCloud, node0.start),
        PCCPointSet3::iterator(&pointCloud, node0.end), childCounts,
        [=](const PCCPointSet3::Proxy& proxy) {
          const auto& point = *proxy;
          return !!(int(point[2]) & pointSortMask[2])
            | (!!(int(point[1]) & pointSortMask[1]) << 1)
            | (!!(int(point[0]) & pointSortMask[0]) << 2);
        });

      // sort and partition the predictor
      std::array<int, 8> predCounts = {};
      if (isInter) {
        countingSort(
          PCCPointSet3::iterator(
            &pointPredictorWorld,
            node0.predStart),  // Need to update the predStar
          PCCPointSet3::iterator(&pointPredictorWorld, node0.predEnd),
          predCounts, [=](const PCCPointSet3::Proxy& proxy) {
            const auto& point = *proxy;
            return !!(int(point[2]) & pointSortMask[2])
              | (!!(int(point[1]) & pointSortMask[1]) << 1)
              | (!!(int(point[0]) & pointSortMask[0]) << 2);
          });
      }

      // generate the bitmap of child occupancy and count
      // the number of occupied children in node0.

      int predOccupancy = 0;
      int predFailureCount = 0;

      int occupancy = 0;
      int numSiblings = 0;
      for (int i = 0; i < 8; i++) {
        if (childCounts[i]) {
          occupancy |= 1 << i;
          numSiblings++;
        }

        bool childOccupiedTmp = !!childCounts[i];
        bool childPredicted = !!predCounts[i];
        if (childPredicted) {
          predOccupancy |= 1 << i;
        }
        predFailureCount += childOccupiedTmp != childPredicted;
      }

      bool occupancyIsPredictable =
        predOccupancy && node0.numSiblingsMispredicted <= 5;
      if (!occupancyIsPredictable) {
        predOccupancy = 0;
      }
      OctreeNodePlanar planarRef;
      if (isInter)
        setPlanesFromOccupancy(predOccupancy, planarRef);

      DirectMode mode = DirectMode::kUnavailable;
      // At the scaling depth, it is possible for a node that has previously
      // been marked as being eligible for idcm to be fully quantised due
      // to the choice of QP.  There is therefore nothing to code with idcm.
      if (isLeafNode(effectiveNodeSizeLog2))
        node0.idcmEligible = false;
      bool planar_eligibility_idcm_angular = true;
      if (node0.idcmEligible) {
        // todo(df): this is pessimistic in the presence of idcm quantisation,
        // since that is eligible may only meet the point count constraint
        // after quantisation, which is performed after the decision is taken.
        mode = canEncodeDirectPosition(
          gps.geom_unique_points_flag, node0, pointCloud);
        if (gps.geom_planar_disabled_idcm_angular_flag) {
          encoder.encodeIsIdcm(mode);
          if (
            mode != DirectMode::kUnavailable
            && gps.geom_angular_mode_enabled_flag)
            planar_eligibility_idcm_angular = false;
        }
      }

      int contextAngle = -1;
      int contextAnglePhiX = -1;
      int contextAnglePhiY = -1;
      if (
        gps.geom_angular_mode_enabled_flag
        && planar_eligibility_idcm_angular) {
        contextAngle = determineContextAngleForPlanar(
          node0, nodeSizeLog2, angularOrigin, zLaser, thetaLaser, numLasers,
          deltaAngle, encoder._phiZi, encoder._phiBuffer.data(),
          &contextAnglePhiX, &contextAnglePhiY, posQuantBitMasks);
      }

      if (
        gps.geom_planar_mode_enabled_flag && planar_eligibility_idcm_angular) {
        // update the plane rate depending on the occupancy and local density
        auto occupancy = node0.siblingOccupancy;
        auto numSiblings = node0.numSiblingsPlus1;
        if (!nodesBeforePlanarUpdate--) {
          encoder._planar.updateRate(occupancy, numSiblings);
          nodesBeforePlanarUpdate = numSiblings - 1;
        }
      }

      OctreeNodePlanar planar;
      if (!isLeafNode(effectiveNodeSizeLog2)) {
        // planar eligibility
        bool planarEligible[3] = {false, false, false};
        if (
          gps.geom_planar_mode_enabled_flag
          && planar_eligibility_idcm_angular) {
          if (gps.geom_octree_depth_planar_eligibiity_enabled_flag) {
            if (gps.geom_angular_mode_enabled_flag) {
              if (contextAngle != -1)
                planarEligible[2] = true;
              planarEligible[0] = (contextAnglePhiX != -1);
              planarEligible[1] = (contextAnglePhiY != -1);
            } else if (planarEligibleKOctreeDepth) {
              planarEligible[0] = true;
              planarEligible[1] = true;
              planarEligible[2] = true;
            }
          } else
          {
            encoder._planar.isEligible(planarEligible);
            if (gps.geom_angular_mode_enabled_flag) {
              if (contextAngle != -1)
                planarEligible[2] = true;
              planarEligible[0] = (contextAnglePhiX != -1);
              planarEligible[1] = (contextAnglePhiY != -1);
            }
          }

          for (int k = 0; k < 3; k++)
            planarEligible[k] &= (codedAxesCurNode >> (2 - k)) & 1;
        }

        planar.allowPCM = (isInter) && (occupancyIsPredictable)
          && (planarEligible[0] || planarEligible[1] || planarEligible[2]);
        planar.isPreDirMatch = true;
        planar.eligible[0] = planarEligible[0];
        planar.eligible[1] = planarEligible[1];
        planar.eligible[2] = planarEligible[2];
        planar.lastDirIdx =
          planarEligible[2] ? 2 : (planarEligible[1] ? 1 : 0);

        // determine planarity if eligible
        if (planarEligible[0] || planarEligible[1] || planarEligible[2])
          encoder.determinePlanarMode(
            gps.adjacent_child_contextualization_enabled_flag, occupancy,
            planarEligible, posInParent, gnp, node0, planar, contextAngle,
            contextAnglePhiX, contextAnglePhiY, planarRef);
      }

      if (node0.idcmEligible && !gps.geom_planar_disabled_idcm_angular_flag)
        encoder.encodeIsIdcm(mode);

      if (mode != DirectMode::kUnavailable) {
        int idcmShiftBits = shiftBits;
        auto idcmSize = effectiveNodeSizeLog2;

        if (idcmQp) {
          node0.qp = idcmQp;
          idcmShiftBits = QuantizerGeom::qpShift(idcmQp);
          idcmSize = nodeSizeLog2 - idcmShiftBits;
          geometryQuantization(pointCloud, node0, quantNodeSizeLog2);

          if (gps.geom_unique_points_flag)
            checkDuplicatePoints(pointCloud, node0, pointIdxToDmIdx);
        }

        encoder.encodeDirectPosition(
          gps.geom_unique_points_flag, gps.joint_2pt_idcm_enabled_flag,
          gps.geom_angular_mode_enabled_flag, mode, posQuantBitMasks, idcmSize,
          idcmShiftBits, node0, planar, pointCloud, angularOrigin, zLaser,
          thetaLaser, numLasers);

        // calculating the number of points coded by IDCM for determining
        // eligibility of planar mode
        if (checkPlanarEligibilityBasedOnOctreeDepth)
          numPointsCodedByIdcm += node0.end - node0.start;

        // inverse quantise any quantised positions
        geometryScale(pointCloud, node0, quantNodeSizeLog2);

        // point reordering to match decoder's order
        for (auto idx = node0.start; idx < node0.end; idx++)
          pointIdxToDmIdx[idx] = nextDmIdx++;

        // NB: by definition, this is the only child node present
        if (gps.inferred_direct_coding_mode <= 1)
          assert(node0.numSiblingsPlus1 == 1);

        // This node has no children, ensure that future nodes avoid
        // accessing stale child occupancy data.
        if (gps.adjacent_child_contextualization_enabled_flag)
          updateGeometryOccupancyAtlasOccChild(node0.pos, 0, &occupancyAtlas);

        continue;
      }

      // when all points are quantized to a single point
      if (!isLeafNode(effectiveNodeSizeLog2)) {
        // encode child occupancy map
        assert(occupancy > 0);

        // planar mode for current node
        // mask to be used for the occupancy coding
        // (bit =1 => occupancy bit not coded due to not belonging to the plane)
        int planarMask[3] = {0, 0, 0};
        maskPlanar(planar, planarMask, codedAxesCurNode);
        bool flagWord4 = gps.neighbour_avail_boundary_log2_minus1 > 0;
        encoder.encodeOccupancyFullNeihbourgs(
          gnp.neighPattern, occupancy, planarMask[0], planarMask[1],
          planarMask[2], planar.planarPossible & 1, planar.planarPossible & 2,
          planar.planarPossible & 4, occupancyAtlas, node0.pos,
          codedAxesPrevLvl, flagWord4,
          gps.adjacent_child_contextualization_enabled_flag, predOccupancy);
      }

      // update atlas for child neighbours
      // NB: the child occupancy atlas must be updated even if the current
      //     node has no occupancy coded in order to clear any stale state in
      //     the atlas.
      if (gps.adjacent_child_contextualization_enabled_flag)
        updateGeometryOccupancyAtlasOccChild(
          node0.pos, occupancy, &occupancyAtlas);

      // calculating the number of subnodes for determining eligibility
      // of planar mode
      if (checkPlanarEligibilityBasedOnOctreeDepth)
        numSubnodes += numSiblings;

      // Leaf nodes are immediately coded.  No further splitting occurs.
      if (isLeafNode(effectiveChildSizeLog2)) {
        int childStart = node0.start;

        // inverse quantise any quantised positions
        geometryScale(pointCloud, node0, quantNodeSizeLog2);

        for (int i = 0; i < 8; i++) {
          if (!childCounts[i]) {
            // child is empty: skip
            continue;
          }

          int childEnd = childStart + childCounts[i];
          for (auto idx = childStart; idx < childEnd; idx++)
            pointIdxToDmIdx[idx] = nextDmIdx++;

          childStart = childEnd;

          // if the bitstream is configured to represent unique points,
          // no point count is sent.
          if (gps.geom_unique_points_flag) {
            assert(childCounts[i] == 1);
            continue;
          }

          encoder.encodePositionLeafNumPoints(childCounts[i]);
        }

        // leaf nodes do not get split
        continue;
      }

      // nodeSizeLog2 > 1: for each child:
      //  - determine elegibility for IDCM
      //  - directly code point positions if IDCM allowed and selected
      //  - otherwise, insert split children into fifo while updating neighbour state
      int childPointsStartIdx = node0.start;

      int predPointsStartIdx = node0.predStart;
      int pos_fs = 1;  // first split falg is popped
      int pos_fp = 0;
      int pos_MV = 0;

      for (int i = 0; i < 8; i++) {
        if (!childCounts[i]) {
          // child is empty: skip
          predPointsStartIdx += predCounts[i];

          continue;
        }

        // create new child
        fifo.emplace_back();
        auto& child = fifo.back();

        int x = !!(i & 4);
        int y = !!(i & 2);
        int z = !!(i & 1);

        child.qp = node0.qp;
        // only shift position if an occupancy bit was coded for the axis
        child.pos[0] = (node0.pos[0] << !!(codedAxesCurLvl & 4)) + x;
        child.pos[1] = (node0.pos[1] << !!(codedAxesCurLvl & 2)) + y;
        child.pos[2] = (node0.pos[2] << !!(codedAxesCurLvl & 1)) + z;

        child.start = childPointsStartIdx;
        childPointsStartIdx += childCounts[i];
        child.end = childPointsStartIdx;
        child.numSiblingsPlus1 = numSiblings;
        child.siblingOccupancy = occupancy;

        child.predStart = predPointsStartIdx;
        predPointsStartIdx += predCounts[i];
        child.predEnd = predPointsStartIdx;
        child.numSiblingsMispredicted = predFailureCount;

        child.laserIndex = node0.laserIndex;
        if (isInter && !gps.geom_angular_mode_enabled_flag)
          child.idcmEligible = isDirectModeEligible_Inter(
            gps.inferred_direct_coding_mode, nodeMaxDimLog2, gnp.neighPattern,
            node0, child, occupancyIsPredictable);
        else
          child.idcmEligible = isDirectModeEligible(
            gps.inferred_direct_coding_mode, nodeMaxDimLog2, gnp.neighPattern,
            node0, child, occupancyIsPredictable,
            gps.geom_angular_mode_enabled_flag);

        if (child.idcmEligible) {
          child.idcmEligible &= idcmEnableMask & 1;
          idcmEnableMask = rotateRight(idcmEnableMask, 1);
        }

        numNodesNextLvl++;
      }
    }
    if (checkPlanarEligibilityBasedOnOctreeDepth)
      planarEligibleKOctreeDepth =
        (pointCloud.getPointCount() - numPointsCodedByIdcm) * 10
        < numSubnodes * 13;

    // calculate the number of points that would be decoded if decoding were
    // to stop at this point.
    if (gps.octree_point_count_list_present_flag) {
      int numPtsAtLvl = numNodesNextLvl + nextDmIdx - 1;
      gbh.footer.octree_lvl_num_points_minus1.push_back(numPtsAtLvl);
    }
  }

  encoder.clearMap();

  // the last element is the number of decoded points
  if (!gbh.footer.octree_lvl_num_points_minus1.empty())
    gbh.footer.octree_lvl_num_points_minus1.pop_back();

  // save the context state for re-use by a future slice if required
  ctxtMem = encoder.getCtx();

  // return partial coding result
  //  - add missing levels to node positions
  //  - inverse quantise the point cloud
  // todo(df): this does not yet support inverse quantisation of node.pos
  if (nodesRemaining) {
    auto nodeSizeLog2 = lvlNodeSizeLog2[maxDepth];
    for (auto& node : fifo) {
      node.pos <<= nodeSizeLog2;
      geometryScale(pointCloud, node, quantNodeSizeLog2);
    }
    *nodesRemaining = std::move(fifo);
    return;
  }

  ////
  // The following is to re-order the points according in the decoding
  // order since IDCM causes leaves to be coded earlier than they
  // otherwise would.
  PCCPointSet3 pointCloud2;
  pointCloud2.addRemoveAttributes(
    pointCloud.hasColors(), pointCloud.hasReflectances());
  pointCloud2.resize(pointCloud.getPointCount());

  // copy points with DM points first, the rest second
  int outIdx = nextDmIdx;
  for (int i = 0; i < pointIdxToDmIdx.size(); i++) {
    int dstIdx = pointIdxToDmIdx[i];
    if (dstIdx == -1) {
      dstIdx = outIdx++;
    } else if (dstIdx == -2) {  // ignore duplicated points
      continue;
    }

    pointCloud2[dstIdx] = pointCloud[i];
    if (pointCloud.hasColors())
      pointCloud2.setColor(dstIdx, pointCloud.getColor(i));
    if (pointCloud.hasReflectances())
      pointCloud2.setReflectance(dstIdx, pointCloud.getReflectance(i));
  }
  pointCloud2.resize(outIdx);
  swap(pointCloud, pointCloud2);
}

//============================================================================

void
encodeGeometryOctree(
  const OctreeEncOpts& opt,
  const GeometryParameterSet& gps,
  GeometryBrickHeader& gbh,
  PCCPointSet3& pointCloud,
  GeometryOctreeContexts& ctxtMem,
  std::vector<std::unique_ptr<EntropyEncoder>>& arithmeticEncoders,
  PCCPointSet3& predPointCloud,
  const SequenceParameterSet& sps,
  const InterGeomEncOpts& interParams)
{
  encodeGeometryOctree(
    opt, gps, gbh, pointCloud, ctxtMem, arithmeticEncoders, nullptr,
    predPointCloud, sps, interParams);
}

//============================================================================
}  // namespace pcc
