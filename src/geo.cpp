/*!
 * \file geo.cpp
 * \brief Class for handling geometry setup & modification
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 0.0.1
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2014 Jacob Crabill.
 *
 */

#include "../include/geo.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_set>

#ifndef _NO_MPI
#include "mpi.h"
#include "metis.h"
#endif

geo::geo()
{
  nodesPerCell = NULL;
}

geo::~geo()
{
#ifndef _NO_MPI
  MPI_Comm_free(&gridComm);
#endif

  if (nodesPerCell != NULL)
    delete nodesPerCell;
}

void geo::setup(input* params)
{
  this->params = params;

  meshType = params->meshType;
  gridID = 0;
  gridRank = params->rank;
  nprocPerGrid = params->nproc;

  switch(meshType) {
    case READ_MESH:
      readGmsh(params->meshFileName);
      break;

    case CREATE_MESH:
      createMesh();
      break;

    case OVERSET_MESH:
      // Find out which grid this process will be handling
      nprocPerGrid = params->nproc/params->nGrids;
      gridID = params->rank / nprocPerGrid;
      gridRank = params->rank % nprocPerGrid;
      readGmsh(params->oversetGrids[gridID]);
      break;

    default:
      FatalError("Mesh type not recognized.");
  }

#ifndef _NO_MPI
  partitionMesh();
#endif

  processConnectivity();

  processPeriodicBoundaries();

//  if (meshType == OVERSET_MESH) {
//    registerGridDataTIOGA();
//  }
}


void geo::processConnectivity()
{
  if (params->rank==0) cout << "Geo: Processing element connectivity" << endl;

  processConnEdges();
  processConnFaces();
  processConnDual();
}

void geo::processConnEdges(void)
{
  /* --- Store Cell-Centers --- */

  c2xc.resize(nEles);
  for (int e=0; e<nEles; e++) {
    point xc;
    for (int i=0; i<c2nv[e]; i++) {
      xc += point(xv[c2v(e,i)]);
    }
    xc /= c2nv[e];

    c2xc[e] = xc;
  }

  /* --- Setup Edges --- */

  matrix<int> e2v1;
  vector<int> edge(2);

  // Populate global list of edges in mesh
  for (int e=0; e<nEles; e++) {
    switch (c2ne[e]) {
      case 6: // Tet
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        break;
      case 8: // Pyramid
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,4)}; e2v1.insertRow(edge);
        break;
      case 9: // Prism
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,3), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,4), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,5), c2v(e,3)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,5)}; e2v1.insertRow(edge);
        break;
      case 12: // Hex
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,4), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,5), c2v(e,6)}; e2v1.insertRow(edge);
        edge = {c2v(e,6), c2v(e,7)}; e2v1.insertRow(edge);
        edge = {c2v(e,7), c2v(e,4)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,6)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,7)}; e2v1.insertRow(edge);
        break;
      default:
        cout << "C2NE: " << c2ne[e] << endl;
        FatalError("Number of edges per element not recognized.");
    }
  }

  /* --- Get a list of the unique edges --- */

  // iE is of length [original e2v1] with range [final e2v]
  // The number of times an edge appears in iE is equal to
  // the number of cells that edge touches
  vector<int> iE;
  // First, organize all edges such that the lower vertex ID comes first
  for (int ie=0; ie<e2v1.getDim0(); ie++) {
    if (e2v1(ie,0) > e2v1(ie,1)) {
      int tmp = e2v1(ie,1);
      e2v1(ie,1) = e2v1(ie,0);
      e2v1(ie,0) = tmp;
    }
  }
  e2v1.unique(e2v,iE);
  nEdges = e2v.getDim0();

  /* --- Generate Vertex-To-Edge/Vertex Connectivity --- */

  vector<set<int>> v2v_tmp(nVerts);
  vector<set<int>> v2e_tmp(nVerts);
  for (int ie=0; ie<nEdges; ie++) {
    int iv1 = e2v(ie,0);
    int iv2 = e2v(ie,1);
    v2v_tmp[iv1].insert(iv2);
    v2v_tmp[iv2].insert(iv1);
    v2e_tmp[iv1].insert(ie);
    v2e_tmp[iv2].insert(ie);
  }

  v2nv.resize(nVerts);
  for (int iv=0; iv<nVerts; iv++) {
    v2nv[iv] = v2v_tmp[iv].size();
  }

  v2v.setup(nVerts,getMax(v2nv));
  v2e.setup(nVerts,getMax(v2nv));
  for (int iv=0; iv<nVerts; iv++) {
    int j = 0;
    for (auto &iv2:v2v_tmp[iv]) {
      v2v(iv,j) = iv2;
      j++;
    }

    j = 0;
    for (auto &ie:v2e_tmp[iv]) {
      v2e(iv,j) = ie;
      j++;
    }
  }

  /* Flag for whether global face ID corresponds to interior or boundary face
     (note that, at this stage, MPI faces will be considered boundary faces) */

  isBndEdge.assign(nEdges,0);
  for (int ie=0; ie<nEdges; ie++) {
    bool found = false;
    for (int ib=0; ib<nBounds; ib++) {
      if (findFirst(bndPts[ib],e2v(ie,0),nBndPts[ib]) != -1) {
        for (int ib=0; ib<nBounds; ib++) {
          if (findFirst(bndPts[ib],e2v(ie,1),nBndPts[ib]) != -1) {
            isBndEdge[ie] = 1;
            found = true;
            break;
          }
        }

        if (found) break;
      }
    }
  }

  /* --- Get maximum number of cells per edge --- */

  int maxCPerE = 0;
  for (uint i=0; i<iE.size(); i++) {
    if (iE[i]!=-1) {
      auto ie = findEq(iE,iE[i]);
      maxCPerE = max(maxCPerE,(int)ie.size());

      // Mark edges as completed
      vecAssign(iE,ie,-1);
    }
  }

  /* --- Setup Cell-To-Edge, Edge-To-Cell --- */

  c2e.setup(nEles,getMax(c2ne));
  c2b.setup(nEles,getMax(c2ne));
  c2b.initializeToZero();
  e2c.setup(nEdges,maxCPerE);
  e2c.initializeToValue(-1);
  e2nc.assign(nEdges,0);

  for (int e=0; e<nEles; e++) {
    // First, load up all the edges for the current cell
    e2v1.setup(0,0);
    switch (c2ne[e]) {
      case 6: // Tet
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        break;
      case 8: // Pyramid
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,4)}; e2v1.insertRow(edge);
        break;
      case 9: // Prism
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,3), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,4), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,5), c2v(e,3)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,5)}; e2v1.insertRow(edge);
        break;
      case 12: // Hex
        edge = {c2v(e,0), c2v(e,1)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,2)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,3)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,0)}; e2v1.insertRow(edge);

        edge = {c2v(e,4), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,5), c2v(e,6)}; e2v1.insertRow(edge);
        edge = {c2v(e,6), c2v(e,7)}; e2v1.insertRow(edge);
        edge = {c2v(e,7), c2v(e,4)}; e2v1.insertRow(edge);

        edge = {c2v(e,0), c2v(e,4)}; e2v1.insertRow(edge);
        edge = {c2v(e,1), c2v(e,5)}; e2v1.insertRow(edge);
        edge = {c2v(e,2), c2v(e,6)}; e2v1.insertRow(edge);
        edge = {c2v(e,3), c2v(e,7)}; e2v1.insertRow(edge);
        break;
      default:
        cout << "C2NE: " << c2ne[e] << endl;
        FatalError("Number of edges per element not recognized.");
    }

    // Next, match each cell edge with the global edge list
    for (int j=0; j<c2ne[e]; j++) {
      if (e2v1(j,1) < e2v1(j,0)) {
        // Swap nodes so that lower node always first
        int tmp = e2v1(j,1);
        e2v1(j,1) = e2v1(j,0);
        e2v1(j,0) = tmp;
      }

      auto ie1 = findEq(e2v.getCol(0),e2v1(j,0));
      auto col2 = (e2v.getRows(ie1)).getCol(1);
      int ie2 = findFirst(col2,e2v1(j,1));
      int ie0 = ie1[ie2];

      // Find ID of face within type-specific array
      if (isBndEdge[ie0]) {
        c2e(e,j) = ie0;
        c2b(e,j) = 1;
      }else{
        c2e(e,j) = ie0;
        c2b(e,j) = 0;
      }

      e2c(ie0,e2nc[ie0]) = e;
      e2nc[ie0]++;
    }
  }
}

void geo::processConnFaces(void)
{
  /* --- Setup Single List of All Faces (sorted vertex lists) --- */

  matrix<int> f2v1;
  vector<int> f2nv1;

  // Handy map to store local face-vertex lists for each ele type
  map<int,matrix<int>> ct2fv;
  map<int,vector<int>> ct2fnv;
  // --- FIX ORDERING FOR FUTURE USE ---
  ct2fv[HEX].insertRow(vector<int>{0,1,2,3});  // Bottom
  ct2fv[HEX].insertRow(vector<int>{4,5,6,7});  // Top
  ct2fv[HEX].insertRow(vector<int>{3,0,4,7});  // Left
  ct2fv[HEX].insertRow(vector<int>{2,1,5,6});  // Right
  ct2fv[HEX].insertRow(vector<int>{1,0,4,5});  // Front
  ct2fv[HEX].insertRow(vector<int>{3,2,6,7});  // Back
  ct2fnv[HEX] = {4,4,4,4,4,4};

  ct2fv[PRISM].insertRowUnsized(vector<int>{0,1,2});    // Bottom
  ct2fv[PRISM].insertRowUnsized(vector<int>{3,4,5});    // Top
  ct2fv[PRISM].insertRowUnsized(vector<int>{0,1,4,3});
  ct2fv[PRISM].insertRowUnsized(vector<int>{0,3,5,2});
  ct2fv[PRISM].insertRowUnsized(vector<int>{1,2,5,4});
  ct2fnv[PRISM] = {3,3,4,4,4};

  ct2fv[PYRAMID].insertRowUnsized(vector<int>{0,1,2,3});    // Bottom
  ct2fv[PYRAMID].insertRowUnsized(vector<int>{0,1,4});    // Top
  ct2fv[PYRAMID].insertRowUnsized(vector<int>{1,2,4});
  ct2fv[PYRAMID].insertRowUnsized(vector<int>{2,3,4});
  ct2fv[PYRAMID].insertRowUnsized(vector<int>{3,0,4});
  ct2fnv[PYRAMID] = {4,3,3,3,3};

  ct2fv[TET].insertRowUnsized(vector<int>{0,1,2});
  ct2fv[TET].insertRowUnsized(vector<int>{1,0,3});
  ct2fv[TET].insertRowUnsized(vector<int>{2,1,3});
  ct2fv[TET].insertRowUnsized(vector<int>{0,2,3});
  ct2fnv[TET] = {3,3,3,3};

  for (int e=0; e<nEles; e++) {
    for (int f=0; f<c2nf[e]; f++) {
      // Get local vertex list for face
      auto iface = ct2fv[ctype[e]].getRow(f);

      // Get global vertex list for face
      vector<int> facev(ct2fnv[ctype[e]][f]);
      for (int i=0; i<ct2fnv[ctype[e]][f]; i++) {
        facev[i] = c2v(e,iface[i]);
        if (i>0 && facev[i] == facev[i-1]) facev[i] = -1;
      }

      // Sort the vertices for easier comparison later
      std::sort(facev.begin(),facev.end());
      f2v1.insertRowUnsized(facev);
      f2nv1.push_back(ct2fnv[ctype[e]][f]);
    }
  }

  /* --- Get a unique list of faces --- */

  // NOTE: Could setup f2c here, but I already have another algorithm implemented later

  // iE is of length [original f2v1] with range [final f2v]
  // The number of times a face appears in iF is equal to
  // the number of cells that face touches
  vector<int> iF;
  f2v1.unique(f2v,iF);
  nFaces = f2v.getDim0();

  f2nv.resize(nFaces);
  for (uint i=0; i<f2nv1.size(); i++)
    f2nv[iF[i]] = f2nv1[i];


  /* --- Generate Internal and Boundary Face Lists --- */

  // Flag for whether global face ID corresponds to interior or boundary face
  // (note that, at this stage, MPI faces will be considered boundary faces)
  isBnd.assign(nFaces,0);

  nIntFaces = 0;
  nBndFaces = 0;
  nMpiFaces = 0;

  for (uint i=0; i<iF.size(); i++) {
    if (iF[i]!=-1) {
      auto ff = findEq(iF,iF[i]);
      if (ff.size()>2) {
        stringstream ss; ss << i;
        string errMsg = "More than 2 cells for face " + ss.str();
        FatalError(errMsg.c_str());
      }
      else if (ff.size()==2) {
        // Internal Edge which has not yet been added
        intFaces.push_back(iF[i]);
        nIntFaces++;
      }
      else if (ff.size()==1) {
        // Boundary or MPI Edge
        bndFaces.push_back(iF[i]);
        isBnd[iF[i]] = true;
        nBndFaces++;
      }

      // Mark edges as completed
      vecAssign(iF,ff,-1);
    }
  }

  /* --- Match Boundary Faces to Boundary Conditions --- */

  // For each vertex on each boundary, the surrounding boundary-face ID's
  matrixBase<set<int>,2> bcV2F(nBounds,getMax(nBndPts));
  matrix<int> bcv2nf(nBounds,getMax(nBndPts));
  bcv2nf.initializeToZero();

  v2b.assign(nVerts,0);
  bcFaces.resize(nBounds);
  bcFaceList.resize(nBounds);
  nBcFaces.assign(nBounds,0);
  bcTypeF.assign(nBndFaces,-1);
  for (int i=0; i<nBndFaces; i++) {
    for (int bnd=0; bnd<nBounds; bnd++) {
      bool isOnBound = true;
      set<int> bfpts;
      for (int j=0; j<f2nv[bndFaces[i]]; j++) {
        int ivb = findFirst(bndPts[bnd],f2v(bndFaces[i],j),bndPts.dims[1]);
        if (ivb == -1) {
          isOnBound = false;
          break;
        }
        bfpts.insert(ivb);
        v2b[bndPts(bnd,ivb)] = 1;
      }

      if (isOnBound) {
        // The face lies on this boundary
        int ff = bndFaces[i];
        bcTypeF[i] = bcList[bnd];
        bcFaceList[bnd].push_back(ff);
        bcFaces[bnd].insertRow(f2v[ff],INSERT_AT_END,f2v.dims[1]);
        for (auto &ivb: bfpts) {
          bcV2F(bnd,ivb).insert(nBcFaces[bnd]);
          bcv2nf(bnd,ivb)++;
        }
        nBcFaces[bnd]++;
        break;
      }
    }
  }

  /* --- Setup Cell-To-Face, Face-To-Cell --- */

  c2f.setup(nEles,getMax(c2nf));
  c2b.setup(nEles,getMax(c2nf));
  c2c.setup(nEles,getMax(c2nf));
  c2b.initializeToZero();
  c2c.initializeToValue(-1);
  c2nc.assign(nEles,0);
  f2c.setup(nFaces,2);
  f2c.initializeToValue(-1);

  for (int ic=0; ic<nEles; ic++) {
    for (int j=0; j<c2nf[ic]; j++) {
      // Get local vertex list for face
      auto iface = ct2fv[ctype[ic]].getRow(j);

      // Get global vertex list for face
      int fnv = ct2fnv[ctype[ic]][j];
      vector<int> facev(fnv);
      for (int i=0; i<fnv; i++)
        facev[i] = c2v(ic,iface[i]);

      // Sort the vertices for easier comparison
      std::sort(facev.begin(),facev.end());

      bool found = false;
      for (int f=0; f<nFaces; f++) {
        if (std::equal(f2v[f],f2v[f]+fnv,facev.begin())) {
          found = true;
          c2f(ic,j) = f;
          break;
        }
      }

      if (!found) FatalError("Unable to match cell face to global face list!");

      int ff = c2f(ic,j);

      // Find ID of face within type-specific array
      if (isBnd[ff])
        c2b(ic,j) = 1;
      else
        c2b(ic,j) = 0;

      if (f2c(ff,0) == -1) {
        // No cell yet assigned to edge; put on left
        f2c(ff,0) = ic;
      }else{
        // Put cell on right
        f2c(ff,1) = ic;
      }
    }
  }

  /* --- Setup Cell-To-Cell --- */

  for (int ic=0; ic<nEles; ic++) {
    for (int j=0; j<c2nf[ic]; j++) {
      int ff = c2f(ic,j);
      if (ff >= 0) {
        if (f2c(ff,0) != ic)
          c2c(ic,j) = f2c(ff,0);
        else
          c2c(ic,j) = f2c(ff,1);
        c2nc[ic]++;
      }
    }
  }

  /* --- Compute (Outward) Face Normals at All Boundary Faces --- */

  // Outward face normal for each boundary face
  matrixBase<Vec3,2> bcFaceNorm(nBounds,getMax(nBcFaces));

  for (int bnd=0; bnd<nBounds; bnd++) {
    for (int i=0; i<nBcFaces[bnd]; i++) {
      int ff = bcFaceList[bnd][i];
      switch (f2nv[ff]) {
      case 3:
        bcFaceNorm(bnd,i) = getFaceNormalTri(ff)/3.;
        break;
      case 4:
        bcFaceNorm(bnd,i) = getFaceNormalQuad(ff)/4.;
        break;
      default:
        FatalError("Number of face vertices not recognized.");
      }
    }
  }

  /* --- Compute Outward Normals for Each Boundary Point --- */

  // Outward normals for each boundary point
  bndNorm.setup(nBounds,getMax(nBndPts));
  bndArea.setup(nBounds,getMax(nBndPts));

  for (int bnd=0; bnd<nBounds; bnd++) {
    for (int i=0; i<nBndPts[bnd]; i++) {
      for (auto &bf: bcV2F(bnd,i)) {
        bndArea(bnd,i) += bcFaceNorm(bnd,bf).norm();
        bndNorm(bnd,i) += bcFaceNorm(bnd,bf);
      }
      // Normalize based upon total area
      bndNorm(bnd,i) /= bndArea(bnd,i);
    }
  }

  /* --- Setup MPI Processor Boundary Faces --- */
//#ifndef _NO_MPI
//  matchMPIFaces();
//#endif

}

Vec3 geo::getFaceNormalTri(int faceID)
{
  point pt0 = point(xv[f2v(faceID,0)]);
  point pt1 = point(xv[f2v(faceID,1)]);
  point pt2 = point(xv[f2v(faceID,2)]);
  Vec3 a = pt1 - pt0;
  Vec3 b = pt2 - pt0;
  Vec3 norm = a.cross(b);                         // Face normal vector
  Vec3 dxc = c2xc[f2c(faceID,0)] - (pt0+pt1+pt2)/3.;  // Face centroid to cell centroid
  if (norm*dxc > 0) {
    // Face normal is pointing into cell; flip
    norm *= -1;
  }
  else {
    // Face normal is pointing out of cell; keep direction
    //norm /= norm.norm();
  }

  return norm;
}

Vec3 geo::getFaceNormalQuad(int faceID)
{
  // Get the (approximate) face normal of an arbitrary 3D quadrilateral
  // by splitting into 2 triangles and averaging

  // Triangle #1
  point pt0 = point(xv[f2v(faceID,0)]);
  point pt1 = point(xv[f2v(faceID,1)]);
  point pt2 = point(xv[f2v(faceID,2)]);
  Vec3 a = pt1 - pt0;
  Vec3 b = pt2 - pt0;
  Vec3 norm1 = a.cross(b);                            // Face normal vector
  Vec3 dxc = c2xc[f2c(faceID,0)] - (pt0+pt1+pt2)/3.;  // Face centroid to cell centroid
  if (norm1*dxc > 0) {
    // Face normal is pointing into cell; flip
    norm1 *= -1;
  }

  // Triangle #2
  pt0 = point(xv[f2v(faceID,3)]);
  a = pt1 - pt0;
  b = pt2 - pt0;
  Vec3 norm2 = a.cross(b);
  if (norm2*dxc > 0) {
    // Face normal is pointing into cell; flip
    norm2 *= -1;
  }

  // Average the two triangle's face outward normals
  Vec3 norm = (norm1+norm2)/2.;

  return norm;
}

void geo::processConnDual(void)
{
  /* --- Setup Dual Mesh Faces --- */

  e2A.resize(nEdges);
  v2vol.assign(nVerts,0);
  for (int i=0; i<nEdges; i++) {
    point midPt = (point(xv[e2v(i,0)])+point(xv[e2v(i,1)]))/2.;

    Array<point,2> facePts;

    // Get the 'edges' of the dual-mesh face
    // Use similar concept to getting e2v: get unique 'edges' by putting lower
    // cell ID first
//    if (isBndEdge[i]) {
      // Have to do some special logic to get face-centers of boundary faces
//      int nmatched = 0;
      for (int j=0; j<e2nc[i]; j++) {
        int ic = e2c(i,j);
        for (int k=0; k<c2nf[ic]; k++) {
          if (c2b(ic,k)) {
            // To find dual edge, find correct face of boundary cells to get face center
            // Only look at boundary faces
            int ff = c2f(ic,k);
            int nepts = 0;
            // Check face's points to find edge
            for (int m=0; m<f2nv[ff]; m++)
              if (f2v(ff,m) == e2v(i,0) || f2v(ff,m) == e2v(i,1))
                nepts++;

            if (nepts==2) {
              // Edge is part of this face; dual edge is cell-center to face-center
              point pt1 = c2xc[ic];
              point pt2;
              for (int n=0; n<f2nv[ff]; n++) pt2 += point(xv[f2v(ff,n)]);
              pt2 /= f2nv[ff];
              facePts.insertRow({pt1,pt2},INSERT_AT_END);
            }
          }
          else {
            int ic2 = c2c(ic,k);
            // see if this c2c is another cell for this edge
            if (ic2>ic && findFirst(e2c[i],ic2,e2nc[i]) != -1) {
              facePts.insertRow({c2xc[ic],c2xc[ic2]},INSERT_AT_END);
            }
          }
        }
      }
//    }
//    else {
//      // each 'edge' of the dual face (neighboring cell centers)
//      facePts.setup(e2nc[i],2);
//      int nmatched = 0;
//      for (int j=0; j<e2nc[i]; j++) {
//        int ic = e2c(i,j);
//        // To find dual edge, find cells adjacent to current cell which also share mesh edge
//        for (int k=0; k<c2nf[ic]; k++) {
//          int ic2 = c2c(ic,k);
//          // see if this c2c is another cell for this edge
//          if (ic2>ic && findFirst(e2c[i],ic2,e2nc[i]) != -1) {
//            facePts(nmatched,0) = c2xc[ic];
//            facePts(nmatched,1) = c2xc[ic2];
//            nmatched++;
//          }
//        }
//      }

      // Get area of dual face
      for (int j=0; j<facePts.getDim0(); j++) {
        Vec3 dx1 = facePts(j,0) - midPt;
        Vec3 dx2 = facePts(j,1) - midPt;
        Vec3 A = dx1.cross(dx2)/2.;
        e2A[i] += A.norm();
      }

      // Add to volumes around edge's points
      // Have the dual-mesh-face edges; get the dual-tet volumes
      // http://mathworld.wolfram.com/Tetrahedron.html

      for (int j=0; j<2; j++) {
        int iv = e2v(i,j);
        point vert = point(xv[iv]);
        Vec3 a = midPt - vert;
        for (int j=0; j<facePts.getDim0(); j++) {
          Vec3 b = facePts(j,0) - vert;
          Vec3 c = facePts(j,1) - vert;
          Vec3 bc = b.cross(c);
          double vol = std::abs(a*bc)/6.;
          v2vol[iv] += vol;
        }
      }
  }


  /* --- Get Volumes of Dual-Mesh Elements --- */

//  v2vol.assign(nVerts,0);
//  for (int iv=0; iv<nVerts; iv++) {
//    point vert = point(xv[iv]);

//    // Sum up contributions from each dual tetrahedron around each edge
//    for (int j=0; j<v2nv[iv]; j++) {
//      int ie = v2e(iv,j);
//      // Get the 'edges' of the dual-mesh face
//      matrix<int> dualEdges(e2nc[ie],2); // each 'edge' of the dual face (neighboring cell IDs)
//      int nmatched = 0;
//      for (int k=0; k<e2nc[ie]; k++) {
//        int ic = e2c(ie,k);
//        // To find dual edge, find cells adjacent to current cell which also share mesh edge
//        for (int k=0; k<c2nf[ic]; k++) {
//          int ic2 = c2c(ic,k);
//          // see if this c2c is another cell for this edge
//          if (ic2>ic && findFirst(e2c[ie],ic2,e2nc[ie]) != -1) {
//            dualEdges(nmatched,0) = ic;
//            dualEdges(nmatched,1) = ic2;
//            nmatched++;
//          }
//        }
//      }

//      // Have the dual-mesh-face edges; get the dual-tet volumes
//      // http://mathworld.wolfram.com/Tetrahedron.html

//      point midPt = (point(xv[e2v(ie,0)])+point(xv[e2v(ie,1)]))/2.;
//      Vec3 a = midPt - vert;

//      for (int k=0; k<e2nc[ie]; k++) {
//        Vec3 b = c2xc[dualEdges(k,0)] - vert;
//        Vec3 c = c2xc[dualEdges(k,1)] - vert;
//        Vec3 bc = b.cross(c);
//        double vol = std::abs(a*bc)/6.;
//        v2vol[iv] += vol;
//      }
//    }
//  }

  /* --- Get Boundary-Point Normals ---- */

//  for (int bnd=0; bnd<nBounds; bnd++) {
//    for (int i=0; i<nBndPts[bnd]; i++) {
//      int iv = bndPts(bnd,i);
//      set<int> bpts;
//      for (int j=0; j<v2nv[iv]; j++) {
//        if (v2b[v2v(iv,j)]) {
//          bpts.insert(v2v(iv,j));
//        }
//      }

//      // Have the surrounding points in random order; now
//    }
//  }
}

//void geo::registerGridDataTIOGA(void)
//{
//  /* Note that this function should only be needed once during preprocessing */

//  // Allocate TIOGA grid processor
//  tg = new tioga();

//  tg->setCommunicator(MPI_COMM_WORLD,params->rank,params->nproc);

//  // Setup iwall, iover (nodes on wall & overset boundaries)
//  iover.resize(0);
//  iwall.resize(0);
//  for (int ib=0; ib<nBounds; ib++) {
//    if (bcList[ib] == OVERSET) {
//      for (int iv=0; iv<nBndPts[ib]; iv++) {
//        iover.push_back(bndPts(ib,iv));
//      }
//    }
//    else if (bcList[ib] == SLIP_WALL || bcList[ib] == ADIABATIC_NOSLIP || bcList[ib] == ISOTHERMAL_NOSLIP) {
//      for (int iv=0; iv<nBndPts[ib]; iv++) {
//        iwall.push_back(bndPts(ib,iv));
//      }
//    }
//  }

//  int nwall = iwall.size();
//  int nover = iover.size();
//  int ntypes = 1;           //! Number of element types in grid block
//  nodesPerCell = new int[1];
//  nodesPerCell[0] = 8;      //! Number of nodes per element for each element type (but only one type so far)
//  iblank.resize(nVerts);
//  iblankCell.resize(nEles);

//  // Need an int**, even if only have one element type
//  conn[0] = c2v.getData();

//  tg->registerGridData(gridID,nVerts,xv.getData(),iblank.data(),nwall,nover,iwall.data(),
//                       iover.data(),ntypes,nodesPerCell,&nEles,&conn[0]);

//  // Give iblankCell to TIOGA for Flurry to access later
//  tg->set_cell_iblank(iblankCell.data());
//}

//void geo::updateOversetConnectivity(void)
//{
//  // Pre-process the grids
//  tg->profile();

//  // This appears to be needed in addition to the high-order-specific processing below?
//  tg->performConnectivity();
//}

//void geo::writeOversetConnectivity(void)
//{
//  // Write out only the mesh with IBLANK info (no solution data)
//  tg->writeData(0,NULL,0);
//}

void geo::matchMPIFaces(void)
{
#ifndef _NO_MPI
  if (nprocPerGrid <= 1) return;

  if (gridRank == 0) {
    if (meshType == OVERSET_MESH)
      cout << "Geo: Grid block " << gridID << ": Matching MPI faces" << endl;
    else
      cout << "Geo: Matching MPI faces" << endl;
  }

  /* --- Split MPI Processes Based Upon gridID: Create MPI_Comm for each grid --- */
  MPI_Comm_split(MPI_COMM_WORLD, gridID, params->rank, &gridComm);

  MPI_Comm_rank(gridComm,&gridRank);
  MPI_Comm_size(gridComm,&nprocPerGrid);

  // 1) Get a list of all the MPI faces on the processor
  // These will be all unassigned boundary faces (bcType == -1) - copy over to mpiEdges
  for (int i=0; i<nBndFaces; i++) {
    if (bcTypeF[i] < 0) {
      mpiFaces.push_back(bndFaces[i]);
      if (nDims == 3) {
        // Get cell ID & cell-local face ID for face-rotation mapping
        mpiCells.push_back(ic2icg[f2c(bndFaces[i],0)]);
        auto cellFaces = c2f.getRow(f2c(bndFaces[i],0));
        int fid = findFirst(cellFaces,bndFaces[i]);
        mpiLocF.push_back(fid);
      }
      bndFaces[i] = -1;
    }
  }
  nMpiFaces = mpiFaces.size();

  // Clean up the bcType and bndEdges arrays now that it's safe to do so [remove mpiFaces from them]
  bndFaces.erase(std::remove(bndFaces.begin(), bndFaces.end(), -1), bndFaces.end());
  bcTypeF.erase(std::remove(bcTypeF.begin(), bcTypeF.end(), -1), bcTypeF.end());
  nBndFaces = bndFaces.size();

  // For future compatibility with 3D mixed meshes: allow faces with different #'s nodes
  // mpi_fptr is like csr matrix ptr (or like eptr from METIS, but for faces instead of eles)
  matrix<int> mpiFaceNodes;
  vector<int> mpiFptr(nMpiFaces+1);
  for (int i=0; i<nMpiFaces; i++) {
    mpiFaceNodes.insertRow(f2v[mpiFaces[i]],INSERT_AT_END,f2nv[mpiFaces[i]]);
    mpiFptr[i+1] = mpiFptr[i]+f2nv[mpiFaces[i]];
  }

  // Convert local node ID's to global
  std::transform(mpiFaceNodes.getData(),mpiFaceNodes.getData()+mpiFaceNodes.getSize(),mpiFaceNodes.getData(), [=](int iv){return iv2ivg[iv];} );

  // Get the number of mpiFaces on each processor (for later communication)
  vector<int> nMpiFaces_proc(nprocPerGrid);
  MPI_Allgather(&nMpiFaces,1,MPI_INT,nMpiFaces_proc.data(),1,MPI_INT,gridComm);

  // 2 for 2D, 4 for 3D; recall that we're treating all elements as being linear, as
  // the extra nodes for quadratic edges or faces are unimportant for determining connectivity
  int maxNodesPerFace = (nDims==2) ? 2 : 4;
  int maxNMpiFaces = getMax(nMpiFaces_proc);
  matrix<int> mpiFaceNodes_proc(nprocPerGrid,maxNMpiFaces*maxNodesPerFace);
  matrix<int> mpiFptr_proc(nprocPerGrid,maxNMpiFaces+1);
  MPI_Allgather(mpiFaceNodes.getData(),mpiFaceNodes.getSize(),MPI_INT,mpiFaceNodes_proc.getData(),maxNMpiFaces*maxNodesPerFace,MPI_INT,gridComm);
  MPI_Allgather(mpiFptr.data(),mpiFptr.size(),MPI_INT,mpiFptr_proc.getData(),maxNMpiFaces+1,MPI_INT,gridComm);

  matrix<int> mpiCells_proc, mpiLocF_proc;
  if (nDims == 3) {
    // Needed for 3D face-matching (to find relRot)
    mpiCells_proc.setup(nprocPerGrid,maxNMpiFaces);
    mpiLocF_proc.setup(nprocPerGrid,maxNMpiFaces);
    MPI_Allgather(mpiCells.data(),mpiCells.size(),MPI_INT,mpiCells_proc.getData(),maxNMpiFaces,MPI_INT,gridComm);
    MPI_Allgather(mpiLocF.data(),mpiLocF.size(),MPI_INT,mpiLocF_proc.getData(),maxNMpiFaces,MPI_INT,gridComm);
  }

  // Now that we have each processor's boundary nodes, start matching faces
  // Again, note that this is written for to be entirely general instead of 2D-specific
  // Find out what processor each face is adjacent to
  procR.resize(nMpiFaces);
  locF_R.resize(nMpiFaces);
  if (nDims == 3) {
    gIC_R.resize(nMpiFaces);
    mpiLocF_R.resize(nMpiFaces);
  }
  for (auto &P:procR) P = -1;

  vector<int> tmpFace(maxNodesPerFace);
  vector<int> myFace(maxNodesPerFace);
  for (int p=0; p<nprocPerGrid; p++) {
    if (p == gridRank) continue;

    // Check all of the processor's faces to see if any match our faces
    for (int i=0; i<nMpiFaces_proc[p]; i++) {
      tmpFace.resize(maxNodesPerFace);
      int k = 0;
      for (int j=mpiFptr_proc(p,i); j<mpiFptr_proc(p,i+1); j++) {
        tmpFace[k] = mpiFaceNodes_proc(p,j);
        k++;
      }
      tmpFace.resize(k);

      // See if this face matches any on this processor
      for (int F=0; F<nMpiFaces; F++) {
        if (procR[F] != -1) continue; // Face already matched

        for (int j=0; j<f2nv[mpiFaces[F]]; j++) {
          myFace[j] = mpiFaceNodes(F,j);
        }
        if (compareFaces(myFace,tmpFace)) {
          procR[F] = p;
          locF_R[F] = i;
          if (nDims == 3) {
            gIC_R[F] = mpiCells_proc(p,i);
            mpiLocF_R[F] = mpiLocF_proc(p,i);
          }
          break;
        }
      }
    }
  }

  for (auto &P:procR)
    if (P==-1) FatalError("MPI face left unmatched!");

  //if (params->rank == 0)
  //cout << "rank " << params->rank << ": ";
  if (gridRank == 0)
    cout << "Geo: Grid " << gridID << ": All MPI faces matched!  nMpiFaces = " << nMpiFaces << endl;

  MPI_Barrier(gridComm);

  //cout << "rank " << params->rank << " leaving matchMPIFaces()" << endl;
#endif
}

//void geo::setupElesFaces(vector<ele> &eles, vector<shared_ptr<face>> &faces, vector<shared_ptr<mpiFace>> &mpiFacesVec)
//{
//  if (nEles<=0) FatalError("Cannot setup elements array - nEles = 0");

//  eles.resize(nEles);
//  faces.resize(nIntFaces+nBndFaces);
//  mpiFacesVec.resize(nMpiFaces);

//  if (gridRank==0) {
//    if (meshType == OVERSET_MESH)
//      cout << "Geo: Grid " << gridID << ": Setting up elements" << endl;
//    else
//      cout << "Geo: Setting up elements" << endl;
//  }

//  // Setup the elements
//  int ic = 0;
//  for (auto& e:eles) {
//    e.ID = ic;
//    e.eType = ctype[ic];
//    e.nNodes = c2nv[ic];

//    // Shape [mesh] nodes
//    e.nodeID.resize(c2nv[ic]);
//    e.nodes.resize(c2nv[ic]);
//    for (int iv=0; iv<c2nv[ic]; iv++) {
//      e.nodeID[iv] = c2v(ic,iv);
//      e.nodes[iv] = point(xv[c2v(ic,iv)]);
//    }

//    // Global face IDs for internal & boundary faces
//    e.faceID.resize(c2nf[ic]);
//    e.bndFace.resize(c2nf[ic]);
//    for (int k=0; k<c2nf[ic]; k++) {
//      e.bndFace[k] = c2b(ic,k);
//      e.faceID[k] = c2f(ic,k);
//    }

//    ic++;
//  }

//  /* --- Setup the faces --- */

//  vector<int> cellFaces;

//  if (gridRank==0) {
//    if (meshType == OVERSET_MESH)
//      cout << "Geo: Grid " << gridID << ": Setting up internal faces" << endl;
//    else
//      cout << "Geo: Setting up internal faces" << endl;
//  }


//  // Internal Faces
//  for (int i=0; i<nIntFaces; i++) {
//    faces[i] = make_shared<intFace>();
//    // Find global face ID of current interior face
//    int ff = intFaces[i];
//    int ic1 = f2c(ff,0);
//    // Find local face ID of global face within first element [on left]
//    cellFaces.assign(c2f[ic1],c2f[ic1]+c2nf[ic1]);
//    int fid1 = findFirst(cellFaces,ff);
//    if (f2c(ff,1) == -1) {
//      FatalError("Interior face does not have a right element assigned.");
//    }
//    else {
//      int ic2 = f2c(ff,1);
//      cellFaces.assign(c2f[ic2], c2f[ic2]+c2nf[ic2]);  // List of cell's faces
//      int fid2 = findFirst(cellFaces,ff);           // Which one is this face
//      int relRot = compareOrientation(ic1,fid1,ic2,fid2);
//      struct faceInfo info;
//      info.locF_R = fid2;
//      info.relRot = relRot;
//      faces[i]->initialize(&eles[f2c(ff,0)],&eles[f2c(ff,1)],ff,fid1,info,params);
//    }
//  }

//  if (gridRank==0) {
//    if (meshType == OVERSET_MESH)
//      cout << "Geo: Grid " << gridID << ": Setting up boundary faces" << endl;
//    else
//      cout << "Geo: Setting up boundary faces" << endl;
//  }

//  // Boundary Faces
//  for (int i=0; i<nBndFaces; i++) {
//    faces[nIntFaces+i] = make_shared<boundFace>();
//    // Find global face ID of current boundary face
//    int ff = bndFaces[i];
//    ic = f2c(ff,0);
//    // Find local face ID of global face within element
//    cellFaces.assign(c2f[ic],c2f[ic]+c2nf[ic]);
//    int fid1 = findFirst(cellFaces,ff);
//    if (f2c(ff,1) != -1) {
//      FatalError("Boundary face has a right element assigned.");
//    }else{
//      struct faceInfo info;
//      info.bcType = bcType[i];
//      faces[nIntFaces+i]->initialize(&eles[f2c(ff,0)],NULL,ff,fid1,info,params);
//    }
//  }

//#ifndef _NO_MPI
//  // MPI Faces
//  if (params->nproc > 1) {

//    if (gridRank==0) {
//      if (meshType == OVERSET_MESH)
//        cout << "Geo: Grid " << gridID << ": Setting up MPI faces" << endl;
//      else
//        cout << "Geo: Setting up MPI faces" << endl;
//    }

//    for (int i=0; i<nMpiFaces; i++) {
//      mpiFacesVec[i] = make_shared<mpiFace>();
//      // Find global face ID of current boundary face
//      int ff = mpiFaces[i];
//      ic = f2c(ff,0);
//      // Find local face ID of global face within element
//      int fid1 = mpiLocF[i];
//      if (f2c(ff,1) != -1) {
//        FatalError("MPI face has a right element assigned.");
//      }else{
//        int relRot = 0;
//        if (nDims == 3) {
//          // Find the relative orientation (rotation) between left & right faces
//          relRot = compareOrientationMPI(ic,fid1,gIC_R[i],mpiLocF_R[i]);
//        }
//        struct faceInfo info;
//        info.IDR = locF_R[i];
//        info.locF_R = mpiLocF_R[i];
//        info.relRot = relRot;
//        info.procL = gridRank;
//        info.procR = procR[i];
//        info.isMPI = 1;
//        info.gridComm = gridComm;  // Note that this is equivalent to MPI_COMM_WORLD if non-overset (ngrids = 1)
//        mpiFacesVec[i]->initialize(&eles[f2c(ff,0)],NULL,i,fid1,info,params);
//      }
//    }
//  }
//#endif
//}

void geo::readGmsh(string fileName)
{
  ifstream meshFile;
  string str;

  if (meshType == OVERSET_MESH) {
    if (gridRank==0) cout << "Geo: Grid " << gridID << ": Reading mesh file " << fileName << endl;
  }
  else {
    if (gridRank==0) cout << "Geo: Reading mesh file " << fileName << endl;
  }

  meshFile.open(fileName.c_str());
  if (!meshFile.is_open())
    FatalError("Unable to open mesh file.");

  /* --- Read Boundary Conditions & Fluid Field(s) --- */

  // Move cursor to $PhysicalNames
  while(1) {
    getline(meshFile,str);
    if (str.find("$PhysicalNames")!=string::npos) break;
    if(meshFile.eof()) FatalError("$PhysicalNames tag not found in Gmsh file!");
  }

  // Read number of boundaries and fields defined
  int nBnds;              // Temp. variable for # of Gmsh regions ("PhysicalNames")
  meshFile >> nBnds;
  getline(meshFile,str);  // clear rest of line

  nBounds = 0;
  for (int i=0; i<nBnds; i++) {
    string bcStr;
    stringstream ss;
    int bcdim, bcid;

    getline(meshFile,str);
    ss << str;
    ss >> bcdim >> bcid >> bcStr;

    // Remove quotation marks from around boundary condition
    size_t ind = bcStr.find("\"");
    while (ind!=string::npos) {
      bcStr.erase(ind,1);
      ind = bcStr.find("\"");
    }

    // Convert to lowercase to match Flurry's boundary condition strings
    std::transform(bcStr.begin(), bcStr.end(), bcStr.begin(), ::tolower);

    // First, map mesh boundary to boundary condition in input file
    if (!params->meshBounds.count(bcStr)) {
      string errS = "Unrecognized mesh boundary: \"" + bcStr + "\"\n";
      errS += "Boundary names in input file must match those in mesh file.";
      FatalError(errS.c_str());
    }

    // Map the Gmsh PhysicalName to the input-file-specified Flurry boundary condition
    bcStr = params->meshBounds[bcStr];

    // Next, check that the requested boundary condition exists
    if (!bcStr2Num.count(bcStr)) {
      string errS = "Unrecognized boundary condition: \"" + bcStr + "\"";
      FatalError(errS.c_str());
    }

    if (bcStr.compare("fluid")==0) {
      nDims = bcdim;
      params->nDims = bcdim;
      bcIdMap[bcid] = -1;
    }
    else {
      bcList.push_back(bcStr2Num[bcStr]);
      bcIdMap[bcid] = nBounds; // Map Gmsh bcid to Flurry bound index
      nBounds++;
    }
  }

  /* --- Read Mesh Vertex Locations --- */

  // Move cursor to $Nodes
  meshFile.clear();
  meshFile.seekg(0, ios::beg);
  while(1) {
    getline(meshFile,str);
    if (str.find("$Nodes")!=string::npos) break;
    if(meshFile.eof()) FatalError("$Nodes tag not found in Gmsh file!");
  }

  uint iv;
  meshFile >> nVerts;
  xv.setup(nVerts,nDims);
  getline(meshFile,str); // Clear end of line, just in case

  for (int i=0; i<nVerts; i++) {
    meshFile >> iv >> xv(i,0) >> xv(i,1);
    if (nDims == 3) meshFile >> xv(i,2);
  }

  /* --- Read Element Connectivity --- */

  // Move cursor to $Elements
  meshFile.clear();
  meshFile.seekg(0, ios::beg);
  while(1) {
    getline(meshFile,str);
    if (str.find("$Elements")!=string::npos) break;
    if(meshFile.eof()) FatalError("$Elements tag not found in Gmsh file!");
  }

  int nElesGmsh;
  vector<int> c2v_tmp(9,0);  // Maximum number of nodes/element possible
  vector<set<int>> boundPoints(nBounds);
  map<int,int> eType2nv;
  eType2nv[3] = 4;  // Linear quad
  eType2nv[16] = 4; // Quadratic serendipity quad
  eType2nv[10] = 4; // Quadratic Lagrange quad
  eType2nv[8] = 8;  // Linear hex

  // Setup bndPts matrix - Just an initial estimate; will be adjusted on the fly
  //bndPts.setup(nBounds,std::round(nNodes/nBounds));
  nBndPts.resize(nBounds);

  // Read total number of interior + boundary elements
  meshFile >> nElesGmsh;
  getline(meshFile,str);    // Clear end of line, just in case

  // For Gmsh node ordering, see: http://geuz.org/gmsh/doc/texinfo/gmsh.html#Node-ordering
  int ic = 0;
  for (int k=0; k<nElesGmsh; k++) {
    int id, eType, nTags, bcid, tmp;
    meshFile >> id >> eType >> nTags;
    meshFile >> bcid;
    bcid = bcIdMap[bcid];
    for (int tag=0; tag<nTags-1; tag++)
      meshFile >> tmp;

    if (bcid == -1) {
      // NOTE: Currently, only quads are supported
      switch(eType) {
      case 2:
        // linear triangle
        c2nv.push_back(3);
        c2nf.push_back(3);
        c2ne.push_back(3);
        ctype.push_back(TRI);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2];
        break;

      case 4:
        // linear tetrahedron
        c2nv.push_back(4);
        c2nf.push_back(4);
        c2ne.push_back(6);
        ctype.push_back(QUAD);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2];
        c2v_tmp[3] = c2v_tmp[2];
        break;

      case 9:
        // quadratic triangle [corner nodes, then edge-center nodes]
        c2nv.push_back(6);
        c2nf.push_back(3);
        c2ne.push_back(3);
        ctype.push_back(TRI);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3] >> c2v_tmp[4] >> c2v_tmp[5];
        break;

      case 3:
        // linear quadrangle
        c2nv.push_back(4);
        c2nf.push_back(4);
        c2ne.push_back(4);
        ctype.push_back(QUAD);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3];
        break;

      case 16:
        // quadratic 8-node (serendipity) quadrangle
        c2nv.push_back(8);
        c2nf.push_back(4);
        c2ne.push_back(4);
        ctype.push_back(QUAD);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3] >> c2v_tmp[4] >> c2v_tmp[5] >> c2v_tmp[6] >> c2v_tmp[7];
        break;

      case 10:
        // quadratic (9-node Lagrange) quadrangle (read as 8-node serendipity)
        c2nv.push_back(8);
        c2nf.push_back(4);
        c2ne.push_back(4);
        ctype.push_back(QUAD);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3] >> c2v_tmp[4] >> c2v_tmp[5] >> c2v_tmp[6] >> c2v_tmp[7];
        break;

      case 5:
        // Linear hexahedron
        c2nv.push_back(8);
        c2nf.push_back(6);
        c2ne.push_back(12);
        ctype.push_back(HEX);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3] >> c2v_tmp[4] >> c2v_tmp[5] >> c2v_tmp[6] >> c2v_tmp[7];
        break;

      case 6:
        // Linear prism
        c2nv.push_back(6);
        c2nf.push_back(5);
        c2ne.push_back(9);
        ctype.push_back(PRISM);
        meshFile >> c2v_tmp[0] >> c2v_tmp[1] >> c2v_tmp[2] >> c2v_tmp[3] >> c2v_tmp[4] >> c2v_tmp[5];
        break;

      default:
        cout << "Gmsh element ID " << k << ", Gmsh Element Type = " << eType << endl;
        FatalError("element type not recognized");
        break;
      }

      // Increase the size of c2v (max # of vertices per cell) if needed
//      if (c2v.getDim1()<(uint)c2nv[ic]) {
//        for (int dim=c2v.getDim1(); dim<c2nv[ic]; dim++) {
//          c2v.addCol();
//        }
//      }

      // Number of nodes in c2v_tmp may vary, so use pointer rather than vector
      c2v.insertRowUnsized(c2v_tmp.data(),c2nv[ic]);

      // Shift every value of c2v by -1 (Gmsh is 1-indexed; we need 0-indexed)
      for(int k=0; k<c2nv[ic]; k++) {
        if(c2v(ic,k)!=0) {
          c2v(ic,k)--;
        }
      }

      ic++;
      getline(meshFile,str); // skip end of line
    }
    else {
      // Boundary cell; put vertices into bndPts
      int nPtsFace = 0;
      switch(eType) {
      case 1: // Linear edge
        nPtsFace = 2;
        break;

      case 3: // Linear quad
        nPtsFace = 4;
        break;

      case 8: // Quadratic edge
        nPtsFace = 3;
        break;

      case 26: // Cubic Edge
        nPtsFace = 4;
        break;

      case 27: // Quartic Edge
        nPtsFace = 5;
        break;

      case 28: // Quintic Edge
        nPtsFace = 6;
        break;

      default:
          cout << "Gmsh element ID " << k << ", Gmsh Element Type = " << eType << endl;
          FatalError("Boundary Element (Face) Type Not Recognized!");
      }

      for (int i=0; i<nPtsFace; i++) {
        meshFile >> iv;  iv--;
        boundPoints[bcid].insert(iv);
      }
      getline(meshFile,str);
    }
  } // End of loop over entities

  int maxNBndPts = 0;
  for (int i=0; i<nBounds; i++) {
    nBndPts[i] = boundPoints[i].size();
    maxNBndPts = max(maxNBndPts,nBndPts[i]);
  }

  // Copy temp boundPoints data into bndPts matrix
  bndPts.setup(nBounds,maxNBndPts);
  for (int i=0; i<nBounds; i++) {
    int j = 0;
    for (auto& it:boundPoints[i]) {
      bndPts(i,j) = it;
      j++;
    }
  }

  nEles = c2v.getDim0();

  meshFile.close();
}

void geo::createMesh()
{
  int nx = params->nx;
  int ny = params->ny;
  int nz = params->nz;
  nDims = params->nDims;

  if (nDims == 2)
    nz = 1;

  if (params->rank==0)
    cout << "Geo: Creating " << nx << "x" << ny << "x" << nz << " cartesian mesh" << endl;

  double xmin = params->xmin;
  double xmax = params->xmax;
  double ymin = params->ymin;
  double ymax = params->ymax;
  double zmin = params->zmin;
  double zmax = params->zmax;

  double dx = (xmax-xmin)/nx;
  double dy = (ymax-ymin)/ny;
  double dz = (zmax-zmin)/nz;

  params->periodicDX = xmax-xmin;
  params->periodicDY = ymax-ymin;
  params->periodicDZ = zmax-zmin;

  nEles = nx*ny*nz;
  vector<int> c2v_tmp;

  if (nDims == 2) {
    nVerts = (nx+1)*(ny+1);
    xv.setup(nVerts,nDims);

    c2nv.assign(nEles,4);
    c2nf.assign(nEles,4);
    ctype.assign(nEles,QUAD);

    /* --- Setup Vertices --- */

    c2v_tmp.assign(4,0);

    int nv = 0;

    for (int i=0; i<ny+1; i++) {
      for (int j=0; j<nx+1; j++) {
        xv(nv,0) = xmin + j*dx;
        xv(nv,1) = ymin + i*dy;
        nv++;
      }
    }

    /* --- Setup Elements --- */

    for (int i=0; i<nx; i++) {
      for (int j=0; j<ny; j++) {
        c2v_tmp[0] = j*(nx+1) + i;
        c2v_tmp[1] = j*(nx+1) + i + 1;
        c2v_tmp[2] = (j+1)*(nx+1) + i + 1;
        c2v_tmp[3] = (j+1)*(nx+1) + i;
        c2v.insertRow(c2v_tmp);
      }
    }
  }
  else if (nDims == 3) {
    nVerts = (nx+1)*(ny+1)*(nz+1);
    xv.setup(nVerts,nDims);

    c2nv.assign(nEles,8);
    c2nf.assign(nEles,6);
    ctype.assign(nEles,HEX);

    c2ne.assign(nEles,12);

    /* --- Setup Vertices --- */

    c2v_tmp.assign(8,0);

    int nv = 0;

    for (int k=0; k<nz+1; k++) {
      for (int j=0; j<ny+1; j++) {
        for (int i=0; i<nx+1; i++) {
          xv(nv,0) = xmin + i*dx;
          xv(nv,1) = ymin + j*dy;
          xv(nv,2) = zmin + k*dz;
          nv++;
        }
      }
    }

    /* --- Setup Elements --- */

    for (int k=0; k<nz; k++) {
      for (int i=0; i<nx; i++) {
        for (int j=0; j<ny; j++) {
          c2v_tmp[0] = i + (nx+1)*(j   + (ny+1)*k);
          c2v_tmp[1] = i + (nx+1)*(j   + (ny+1)*k) + 1;
          c2v_tmp[2] = i + (nx+1)*(j+1 + (ny+1)*k) + 1;
          c2v_tmp[3] = i + (nx+1)*(j+1 + (ny+1)*k);

          c2v_tmp[4] = i + (nx+1)*(j   + (ny+1)*(k+1));
          c2v_tmp[5] = i + (nx+1)*(j   + (ny+1)*(k+1)) + 1;
          c2v_tmp[6] = i + (nx+1)*(j+1 + (ny+1)*(k+1)) + 1;
          c2v_tmp[7] = i + (nx+1)*(j+1 + (ny+1)*(k+1));
          c2v.insertRow(c2v_tmp);
        }
      }
    }
  }

  /* --- Setup Boundaries --- */

  // List of all boundary conditions being used (bcNum maps string->int)
  bcList.push_back(bcStr2Num[params->create_bcBottom]);
  bcList.push_back(bcStr2Num[params->create_bcRight]);
  bcList.push_back(bcStr2Num[params->create_bcTop]);
  bcList.push_back(bcStr2Num[params->create_bcLeft]);

  if (nDims == 3) {
    bcList.push_back(bcStr2Num[params->create_bcFront]);
    bcList.push_back(bcStr2Num[params->create_bcBack]);
  }

  // Sort the list & remove any duplicates
  std::sort(bcList.begin(), bcList.end());
  vector<int>::iterator vIt = std::unique(bcList.begin(), bcList.end());
  nBounds = std::distance(bcList.begin(), vIt);     // will I need both an nBounds (i.e., in mesh) and an nBC's (current nBounds)?
  bcList.resize(nBounds);

  // Setup a map so we know where each BC# is inside of bcList
  map<int,int> bc2bcList;

  // Setup boundary connectivity storage
  nFacesPerBnd.assign(nBounds,0);
  if (nDims == 2) {
    bndPts.setup(nBounds,2*4*(std::max(nx,ny)+1));
  }
  else if (nDims == 3) {
    int maxN_BFace = std::max(nx*ny,nx*nz);
    maxN_BFace = std::max(maxN_BFace,ny*nz);
    bndPts.setup(nBounds,6*4*maxN_BFace);
  }
  nBndPts.resize(nBounds);
  for (int i=0; i<nBounds; i++) {
    bc2bcList[bcList[i]] = i;
  }

  /* --- Setup Boundary Faces --- */

  if (nDims == 2) {
    // Bottom Edge Faces
    int ib = bc2bcList[bcStr2Num[params->create_bcBottom]];
    int ne = nFacesPerBnd[ib];
    for (int ix=0; ix<nx; ix++) {
      bndPts[ib][2*ne]   = ix;
      bndPts[ib][2*ne+1] = ix+1;
      ne++;
    }
    nFacesPerBnd[ib] = ne;

    // Top Edge Faces
    ib = bc2bcList[bcStr2Num[params->create_bcTop]];
    ne = nFacesPerBnd[ib];
    for (int ix=0; ix<nx; ix++) {
      bndPts[ib][2*ne]   = (nx+1)*ny + ix+1;
      bndPts[ib][2*ne+1] = (nx+1)*ny + ix;
      ne++;
    }
    nFacesPerBnd[ib] = ne;

    // Left Edge Faces
    ib = bc2bcList[bcStr2Num[params->create_bcLeft]];
    ne = nFacesPerBnd[ib];
    for (int iy=0; iy<ny; iy++) {
      bndPts[ib][2*ne]   = (iy+1)*(nx+1);
      bndPts[ib][2*ne+1] = iy*(nx+1);
      ne++;
    }
    nFacesPerBnd[ib] = ne;

    // Right Edge Faces
    ib = bc2bcList[bcStr2Num[params->create_bcRight]];
    ne = nFacesPerBnd[ib];
    for (int iy=0; iy<ny; iy++) {
      bndPts[ib][2*ne]   = iy*(nx+1) + nx;
      bndPts[ib][2*ne+1] = (iy+1)*(nx+1) + nx;
      ne++;
    }
    nFacesPerBnd[ib] = ne;
  }
  else if (nDims == 3) {
    // Bottom Side Faces  [zmin]
    int ib = bc2bcList[bcStr2Num[params->create_bcBottom]];
    int nf = nFacesPerBnd[ib];
    for (int ix=0; ix<nx; ix++) {
      for (int iy=0; iy<ny; iy++) {
        bndPts(ib,4*nf)   = iy*(nx+1) + ix;
        bndPts(ib,4*nf+1) = iy*(nx+1) + ix + 1;
        bndPts(ib,4*nf+2) = (iy+1)*(nx+1) + ix + 1;
        bndPts(ib,4*nf+3) = (iy+1)*(nx+1) + ix;
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;

    // Top Side Faces  [zmax]
    ib = bc2bcList[bcStr2Num[params->create_bcTop]];
    nf = nFacesPerBnd[ib];
    for (int ix=0; ix<nx; ix++) {
      for (int iy=0; iy<ny; iy++) {
        bndPts(ib,4*nf)   = (nx+1)*(ny+1)*nz + iy*(nx+1) + ix;
        bndPts(ib,4*nf+1) = (nx+1)*(ny+1)*nz + iy*(nx+1) + ix+1;
        bndPts(ib,4*nf+2) = (nx+1)*(ny+1)*nz + (iy+1)*(nx+1) + ix+1;
        bndPts(ib,4*nf+3) = (nx+1)*(ny+1)*nz + (iy+1)*(nx+1) + ix;
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;

    // Left Side Faces (x = xmin)
    ib = bc2bcList[bcStr2Num[params->create_bcLeft]];
    nf = nFacesPerBnd[ib];
    for (int iz=0; iz<nz; iz++) {
      for (int iy=0; iy<ny; iy++) {
        bndPts(ib,4*nf)   = iz*(nx+1)*(ny+1) + iy*(nx+1);
        bndPts(ib,4*nf+1) = (iz+1)*(nx+1)*(ny+1) + iy*(nx+1);
        bndPts(ib,4*nf+2) = (iz+1)*(nx+1)*(ny+1) + (iy+1)*(nx+1);
        bndPts(ib,4*nf+3) = iz*(nx+1)*(ny+1) + (iy+1)*(nx+1);
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;

    // Right Side Faces (x = xmax)
    ib = bc2bcList[bcStr2Num[params->create_bcRight]];
    nf = nFacesPerBnd[ib];
    for (int iz=0; iz<nz; iz++) {
      for (int iy=0; iy<ny; iy++) {
        bndPts(ib,4*nf)   = iz*(nx+1)*(ny+1) + iy*(nx+1) + nx;
        bndPts(ib,4*nf+1) = (iz+1)*(nx+1)*(ny+1) + iy*(nx+1) + nx;
        bndPts(ib,4*nf+2) = (iz+1)*(nx+1)*(ny+1) + (iy+1)*(nx+1) + nx;
        bndPts(ib,4*nf+3) = iz*(nx+1)*(ny+1) + (iy+1)*(nx+1) + nx;
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;


    // Front Side Faces (y = ymin)
    ib = bc2bcList[bcStr2Num[params->create_bcFront]];
    nf = nFacesPerBnd[ib];
    for (int iz=0; iz<nz; iz++) {
      for (int ix=0; ix<nx; ix++) {
        bndPts(ib,4*nf)   = iz*(nx+1)*(ny+1) + ix;
        bndPts(ib,4*nf+1) = (iz+1)*(nx+1)*(ny+1) + ix;
        bndPts(ib,4*nf+2) = (iz+1)*(nx+1)*(ny+1) + ix + 1;
        bndPts(ib,4*nf+3) = iz*(nx+1)*(ny+1) + ix + 1;
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;

    // Back Side Faces (y = ymax)
    ib = bc2bcList[bcStr2Num[params->create_bcBack]];
    nf = nFacesPerBnd[ib];
    for (int iz=0; iz<nz; iz++) {
      for (int ix=0; ix<nx; ix++) {
        bndPts(ib,4*nf)   = iz*(nx+1)*(ny+1) + ny*(nx+1) + ix;
        bndPts(ib,4*nf+1) = (iz+1)*(nx+1)*(ny+1) + ny*(nx+1) + ix;
        bndPts(ib,4*nf+2) = (iz+1)*(nx+1)*(ny+1) + ny*(nx+1) + ix + 1;
        bndPts(ib,4*nf+3) = iz*(nx+1)*(ny+1) + ny*(nx+1) + ix + 1;
        nf++;
      }
    }
    nFacesPerBnd[ib] = nf;
  }

  // Remove duplicates in bndPts
  for (int i=0; i<nBounds; i++) {
    std::sort(bndPts[i], bndPts[i]+bndPts.getDim1());
    int* it = std::unique(bndPts[i], bndPts[i]+bndPts.getDim1());
    nBndPts[i] = std::distance(bndPts[i],it);
  }
  int maxNBndPts = getMax(nBndPts);
  bndPts.removeCols(bndPts.getDim1()-maxNBndPts);
}

void geo::processPeriodicBoundaries(void)
{
  uint nPeriodic, bi, bj, ic;
  vector<int> iPeriodic(0);

  for (int i=0; i<nBndFaces; i++) {
    if (bcTypeF[i] == PERIODIC) {
      iPeriodic.push_back(i);
    }
  }

  nPeriodic = iPeriodic.size();

#ifndef _NO_MPI
  if (nPeriodic > 0)
    FatalError("Periodic boundaries not implemented yet with MPI! Recompile for serial.");
#endif

  if (nPeriodic == 0) return;
  if (nPeriodic%2 != 0) FatalError("Expecting even number of periodic faces; have odd number.");
  if (params->rank==0) cout << "Geo: Processing periodic boundaries" << endl;

  for (auto& i:iPeriodic) {
    if (bndFaces[i]==-1) continue;
    for (auto& j:iPeriodic) {
      if (i==j || bndFaces[i]==-1 || bndFaces[j]==-1) continue;
      bool match;
      if (nDims == 2) {
        match = checkPeriodicFaces(f2v[bndFaces[i]],f2v[bndFaces[j]]);
      }
      else if (nDims == 3) {
        auto face1 = f2v.getRow(bndFaces[i]);
        auto face2 = f2v.getRow(bndFaces[j]);
        match = checkPeriodicFaces3D(face1, face2);
      }
      if (match) {

        /* --- Match found - now take care of transfer from boundary -> internal --- */

        if (i>j) FatalError("How did this happen?!");

        bi = bndFaces[i];
        bj = bndFaces[j];

        // Transfer combined edge from boundary to internal list
        intFaces.push_back(bi);

        // Flag global edge IDs as internal faces
        isBnd[bi] = false;
        isBnd[bj] = false;

        // Fix f2c - add right cell to combined face, make left cell = -1 in 'deleted' face
        f2c(bi,1) = f2c[bj][0];
        f2c(bj,0) = -1;

        // Fix c2f - replace 'deleted' edge from right cell with combined face
        ic = f2c[bi][1];
        int fID = findFirst(c2f[ic],(int)bj,c2nf[ic]);
        c2f(f2c(bi,1),fID) = bi;

        // Fix c2b - set element-local face to be internal face
        c2b(f2c(bi,1),fID) = false;

        // Flag edges as gone in boundary edges list
        bndFaces[i] = -1;
        bndFaces[j] = -1;
      }
    }
  }

  // Remove no-longer-existing periodic boundary edges and update nBndEdges
  bndFaces.erase(std::remove(bndFaces.begin(), bndFaces.end(), -1), bndFaces.end());
  nBndFaces = bndFaces.size();
  nIntFaces = intFaces.size();
}

bool geo::compareFaces(vector<int> &face1, vector<int> &face2)
{
  uint nv = face1.size();
  if (face2.size() != nv) return false;

  std::sort(face1.begin(),face1.end());
  std::sort(face2.begin(),face2.end());

  bool found = true;
  for (uint i=0; i<nv; i++) {
    if (face1[i] != face2[i]) found = false;
  }

  return found;
}

bool geo::checkPeriodicFaces(int* edge1, int* edge2)
{
  double x11, x12, y11, y12, x21, x22, y21, y22;
  x11 = xv[edge1[0]][0];  y11 = xv[edge1[0]][1];
  x12 = xv[edge1[1]][0];  y12 = xv[edge1[1]][1];
  x21 = xv[edge2[0]][0];  y21 = xv[edge2[0]][1];
  x22 = xv[edge2[1]][0];  y22 = xv[edge2[1]][1];

  double tol = params->periodicTol;
  double dx = params->periodicDX;
  double dy = params->periodicDY;

  if ( abs(abs(x21-x11)-dx)<tol && abs(abs(x22-x12)-dx)<tol && abs(y21-y11)<tol && abs(y22-y12)<tol ) {
    // Faces match up across x-direction, with [0]->[0] and [1]->[1] in each edge
    return true;
  }
  else if ( abs(abs(x22-x11)-dx)<tol && abs(abs(x21-x12)-dx)<tol && abs(y22-y11)<tol && abs(y21-y12)<tol ) {
    // Faces match up across x-direction, with [0]->[1] and [1]->[0] in each edge
    return true;
  }
  else if ( abs(abs(y21-y11)-dy)<tol && abs(abs(y22-y12)-dy)<tol && abs(x21-x11)<tol && abs(x22-x12)<tol ) {
    // Faces match up across y-direction, with [0]->[0] and [1]->[1] in each edge
    return true;
  }
  else if ( abs(abs(y22-y11)-dy)<tol && abs(abs(y21-y12)-dy)<tol && abs(x22-x11)<tol && abs(x21-x12)<tol ) {
    // Faces match up across y-direction, with [0]->[1] and [1]->[0] in each edge
    return true;
  }

  // None of the above
  return false;
}

bool geo::checkPeriodicFaces3D(vector<int> &face1, vector<int> &face2)
{
  if (face1.size() != face2.size())
    return false;

  double tol = params->periodicTol;
  double dx = params->periodicDX;
  double dy = params->periodicDY;
  double dz = params->periodicDZ;

  /* --- Compare faces using normal vectors: normals should be aligned
   * and offset by norm .dot. {dx,dy,dz} --- */

  Vec3 vec1, vec2;

  // Calculate face normal & centriod for face 1
  Vec3 norm1;
  point c1;
  vec1 = point(xv[face1[1]]) - point(xv[face1[0]]);
  vec2 = point(xv[face1[2]]) - point(xv[face1[0]]);
  for (uint j=0; j<face1.size(); j++)
    c1 += xv[face1[j]];
  c1 /= face1.size();

  norm1[0] = vec1[1]*vec2[2] - vec1[2]*vec2[1];
  norm1[1] = vec1[2]*vec2[0] - vec1[0]*vec2[2];
  norm1[2] = vec1[0]*vec2[1] - vec1[1]*vec2[0];
  // Normalize
  double magNorm1 = sqrt(norm1[0]*norm1[0]+norm1[1]*norm1[1]+norm1[2]*norm1[2]);
  norm1 /= magNorm1;

  // Calculate face normal & centroid for face 2
  Vec3 norm2;
  point c2;
  vec1 = point(xv[face2[1]]) - point(xv[face2[0]]);
  vec2 = point(xv[face2[2]]) - point(xv[face2[0]]);
  for (uint j=0; j<face2.size(); j++)
    c2 += point(xv[face2[j]]);
  c2 /= face2.size();
  norm2[0] = vec1[1]*vec2[2] - vec1[2]*vec2[1];
  norm2[1] = vec1[2]*vec2[0] - vec1[0]*vec2[2];
  norm2[2] = vec1[0]*vec2[1] - vec1[1]*vec2[0];
  // Normalize
  double magNorm2 = sqrt(norm2[0]*norm2[0]+norm2[1]*norm2[1]+norm2[2]*norm2[2]);
  norm2 /= magNorm2;

  // Check for same orientation - norm1 .dot. norm2 should be +/- 1
  double dot = norm1*norm2;
  if (abs(1-abs(dot))>tol) return false;

  // Check offset distance - norm .times. {dx,dy,dz} should equal centroid2 - centriod1
  Vec3 nDXYZ;
  nDXYZ.x = norm1[0]*dx;
  nDXYZ.y = norm1[1]*dy;
  nDXYZ.z = norm1[2]*dz;
  nDXYZ.abs();

  Vec3 Offset = c2 - c1;
  Offset.abs();

  Vec3 Diff = Offset - nDXYZ;
  Diff.abs();

  for (int i=0; i<3; i++) {
    if (Diff[i]>tol) return false;
  }

  // The faces are aligned across a periodic direction
  return true;
}

int geo::compareOrientation(int ic1, int f1, int ic2, int f2)
{
  if (nDims == 2) return 1;

  int nv = f2nv[c2f(ic1,f1)];

  vector<int> tmpFace1(nv), tmpFace2(nv);

  switch (ctype[ic1]) {
    case HEX:
      // Flux points arranged in 2D grid on each face oriented with each
      // dimension increasing in its +'ve direction ['btm-left' to 'top-right']
      // Node ordering reflects this: CCW from 'bottom-left' node on each face
      switch (f1) {
        case 0:
          // Bottom face  (z = -1)
          tmpFace1[0] = c2v(ic1,0);
          tmpFace1[1] = c2v(ic1,1);
          tmpFace1[2] = c2v(ic1,2);
          tmpFace1[3] = c2v(ic1,3);
          break;
        case 1:
          // Top face  (z = +1)
          tmpFace1[0] = c2v(ic1,5);
          tmpFace1[1] = c2v(ic1,4);
          tmpFace1[2] = c2v(ic1,7);
          tmpFace1[3] = c2v(ic1,6);
          break;
        case 2:
          // Left face  (x = -1)
          tmpFace1[0] = c2v(ic1,0);
          tmpFace1[1] = c2v(ic1,3);
          tmpFace1[2] = c2v(ic1,7);
          tmpFace1[3] = c2v(ic1,4);
          break;
        case 3:
          // Right face  (x = +1)
          tmpFace1[0] = c2v(ic1,2);
          tmpFace1[1] = c2v(ic1,1);
          tmpFace1[2] = c2v(ic1,5);
          tmpFace1[3] = c2v(ic1,6);
          break;
        case 4:
          // Front face  (y = -1)
          tmpFace1[0] = c2v(ic1,1);
          tmpFace1[1] = c2v(ic1,0);
          tmpFace1[2] = c2v(ic1,4);
          tmpFace1[3] = c2v(ic1,5);
          break;
        case 5:
          // Back face  (y = +1)
          tmpFace1[0] = c2v(ic1,3);
          tmpFace1[1] = c2v(ic1,2);
          tmpFace1[2] = c2v(ic1,6);
          tmpFace1[3] = c2v(ic1,7);
          break;
      }
      break;

    default:
      FatalError("Element type not supported.");
      break;
  }

  switch (ctype[ic2]) {
    case HEX:
      switch (f2) {
        case 0:
          // Bottom face  (z = -1)
          tmpFace2[0] = c2v(ic2,0);
          tmpFace2[1] = c2v(ic2,1);
          tmpFace2[2] = c2v(ic2,2);
          tmpFace2[3] = c2v(ic2,3);
          break;
        case 1:
          // Top face  (z = +1)
          tmpFace2[0] = c2v(ic2,5);
          tmpFace2[1] = c2v(ic2,4);
          tmpFace2[2] = c2v(ic2,7);
          tmpFace2[3] = c2v(ic2,6);
          break;
        case 2:
          // Left face  (x = -1)
          tmpFace2[0] = c2v(ic2,0);
          tmpFace2[1] = c2v(ic2,3);
          tmpFace2[2] = c2v(ic2,7);
          tmpFace2[3] = c2v(ic2,4);
          break;
        case 3:
          // Right face  (x = +1)
          tmpFace2[0] = c2v(ic2,2);
          tmpFace2[1] = c2v(ic2,1);
          tmpFace2[2] = c2v(ic2,5);
          tmpFace2[3] = c2v(ic2,6);
          break;
        case 4:
          // Front face  (y = -1)
          tmpFace2[0] = c2v(ic2,1);
          tmpFace2[1] = c2v(ic2,0);
          tmpFace2[2] = c2v(ic2,4);
          tmpFace2[3] = c2v(ic2,5);
          break;
        case 5:
          // Back face  (y = +1)
          tmpFace2[0] = c2v(ic2,3);
          tmpFace2[1] = c2v(ic2,2);
          tmpFace2[2] = c2v(ic2,6);
          tmpFace2[3] = c2v(ic2,7);
          break;
      }
      break;

    default:
      FatalError("Element type not supported.");
      break;
  }

  // Now, compare the two faces to see the relative orientation [rotation]
  if      (tmpFace1[0] == tmpFace2[0]) return 0;
  else if (tmpFace1[1] == tmpFace2[0]) return 1;
  else if (tmpFace1[2] == tmpFace2[0]) return 2;
  else if (tmpFace1[3] == tmpFace2[0]) return 3;
  else if (checkPeriodicFaces3D(tmpFace1,tmpFace2)) {
    point c1, c2;
    for (auto iv:tmpFace1) c1 += point(xv[iv]);
    for (auto iv:tmpFace2) c2 += point(xv[iv]);
    c1 /= tmpFace1.size();
    c2 /= tmpFace2.size();
    Vec3 fDist = c2 - c1;
    fDist /= sqrt(fDist*fDist); // Normalize

    point pt1;
    point pt2 = point(xv[tmpFace2[0]]);

    for (int i=0; i<4; i++) {
      pt1 = point(xv[tmpFace1[i]]);
      Vec3 ptDist = pt2 - pt1;        // Vector between points
      ptDist /= sqrt(ptDist*ptDist); // Normalize

      double dot = fDist*ptDist;
      if (abs(1-abs(dot))<params->periodicTol) return i; // These points align
    }
    // Matching points not found
    FatalError("Unable to orient periodic faces using simple algorithm.");
  }
  else FatalError("Internal faces improperly matched.");

}

int geo::compareOrientationMPI(int ic1, int f1, int ic2, int f2)
{
#ifndef _NO_MPI
  if (nDims == 2) return 1;

  int nv = f2nv[c2f(ic1,f1)];

  vector<int> tmpFace1(nv), tmpFace2(nv);

  switch (ctype[ic1]) {
    case HEX:
      // Flux points arranged in 2D grid on each face oriented with each
      // dimension increasing in its +'ve direction ['btm-left' to 'top-right']
      // Node ordering reflects this: CCW from 'bottom-left' node on each face
      switch (f1) {
        case 0:
          // Bottom face  (z = -1)
          tmpFace1[0] = c2v(ic1,0);
          tmpFace1[1] = c2v(ic1,1);
          tmpFace1[2] = c2v(ic1,2);
          tmpFace1[3] = c2v(ic1,3);
          break;
        case 1:
          // Top face  (z = +1)
          tmpFace1[0] = c2v(ic1,5);
          tmpFace1[1] = c2v(ic1,4);
          tmpFace1[2] = c2v(ic1,7);
          tmpFace1[3] = c2v(ic1,6);
          break;
        case 2:
          // Left face  (x = -1)
          tmpFace1[0] = c2v(ic1,0);
          tmpFace1[1] = c2v(ic1,3);
          tmpFace1[2] = c2v(ic1,7);
          tmpFace1[3] = c2v(ic1,4);
          break;
        case 3:
          // Right face  (x = +1)
          tmpFace1[0] = c2v(ic1,2);
          tmpFace1[1] = c2v(ic1,1);
          tmpFace1[2] = c2v(ic1,5);
          tmpFace1[3] = c2v(ic1,6);
          break;
        case 4:
          // Front face  (y = -1)
          tmpFace1[0] = c2v(ic1,1);
          tmpFace1[1] = c2v(ic1,0);
          tmpFace1[2] = c2v(ic1,4);
          tmpFace1[3] = c2v(ic1,5);
          break;
        case 5:
          // Back face  (y = +1)
          tmpFace1[0] = c2v(ic1,3);
          tmpFace1[1] = c2v(ic1,2);
          tmpFace1[2] = c2v(ic1,6);
          tmpFace1[3] = c2v(ic1,7);
          break;
      }
      break;

    default:
      FatalError("Element type not supported.");
      break;
  }

  switch (ctype_g[ic2]) {
    case HEX:
      switch (f2) {
        case 0:
          // Bottom face  (z = -1)
          tmpFace2[0] = c2v_g(ic2,0);
          tmpFace2[1] = c2v_g(ic2,1);
          tmpFace2[2] = c2v_g(ic2,2);
          tmpFace2[3] = c2v_g(ic2,3);
          break;
        case 1:
          // Top face  (z = +1)
          tmpFace2[0] = c2v_g(ic2,5);
          tmpFace2[1] = c2v_g(ic2,4);
          tmpFace2[2] = c2v_g(ic2,7);
          tmpFace2[3] = c2v_g(ic2,6);
          break;
        case 2:
          // Left face  (x = -1)
          tmpFace2[0] = c2v_g(ic2,0);
          tmpFace2[1] = c2v_g(ic2,3);
          tmpFace2[2] = c2v_g(ic2,7);
          tmpFace2[3] = c2v_g(ic2,4);
          break;
        case 3:
          // Right face  (x = +1)
          tmpFace2[0] = c2v_g(ic2,2);
          tmpFace2[1] = c2v_g(ic2,1);
          tmpFace2[2] = c2v_g(ic2,5);
          tmpFace2[3] = c2v_g(ic2,6);
          break;
        case 4:
          // Front face  (y = -1)
          tmpFace2[0] = c2v_g(ic2,1);
          tmpFace2[1] = c2v_g(ic2,0);
          tmpFace2[2] = c2v_g(ic2,4);
          tmpFace2[3] = c2v_g(ic2,5);
          break;
        case 5:
          // Back face  (y = +1)
          tmpFace2[0] = c2v_g(ic2,3);
          tmpFace2[1] = c2v_g(ic2,2);
          tmpFace2[2] = c2v_g(ic2,6);
          tmpFace2[3] = c2v_g(ic2,7);
          break;
      }
      break;

    default:
      FatalError("Element type not supported.");
      break;
  }

  // Now, compare the two faces to see the relative orientation [rotation]
  if      (iv2ivg[tmpFace1[0]] == tmpFace2[0]) return 0;
  else if (iv2ivg[tmpFace1[1]] == tmpFace2[0]) return 1;
  else if (iv2ivg[tmpFace1[2]] == tmpFace2[0]) return 2;
  else if (iv2ivg[tmpFace1[3]] == tmpFace2[0]) return 3;
  else FatalError("MPI faces improperly matched.");
#else
  return 0;
#endif
}


void geo::partitionMesh(void)
{
#ifndef _NO_MPI
  int rank, nproc;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (nproc <= 1) return;

  if (meshType == OVERSET_MESH) {
    if (nproc % params->nGrids != 0) FatalError("Expcting # of processes to be evenly divisible by # of grids.");
    // Partitioning each grid independantly; local 'grid rank' is the important rank
    gridRank = rank % nprocPerGrid;
    rank = gridRank;
    nproc = nprocPerGrid;

    if (nproc <= 1) return; // No additional partitioning needed

    if (rank == 0) cout << "Geo: Partitioning mesh block " << gridID << " across " << nprocPerGrid << " processes" << endl;
    if (rank == 0) cout << "Geo:   Number of elements in block " << gridID << " : " << nEles << endl;
  }
  else {
    if (rank == 0) cout << "Geo: Partitioning mesh across " << nproc << " processes" << endl;
    if (rank == 0) cout << "Geo:   Number of elements globally: " << nEles << endl;
  }

  vector<idx_t> eptr(nEles+1);
  vector<idx_t> eind;

  int nn = 0;
  for (int i=0; i<nEles; i++) {
    eind.push_back(c2v(i,0));
    nn++;
    for (int j=1; j<c2nv[i]; j++) {
      if (c2v(i,j)==c2v(i,j-1)) {
        continue; // To deal with collapsed edges
      }
      eind.push_back(c2v(i,j));
      nn++;
    }
    eptr[i+1] = nn;
  }

  int objval;
  vector<int> epart(nEles);
  vector<int> npart(nVerts);

  // int errVal = METIS PartMeshDual(idx_t *ne, idx_t *nn, idx_t *eptr, idx_t *eind, idx_t *vwgt, idx_t *vsize,
  // idx_t *ncommon, idx_t *nparts, real_t *tpwgts, idx_t *options, idx_t *objval,idx_t *epart, idx_t *npart)

  int ncommon; // 2 for 2D, ~3 for 3D [#nodes per face: 2 for quad/tri, 3 for tet, 4 for hex]
  if (nDims == 2) ncommon = 2;
  else if (nDims == 3) ncommon = 4;

  idx_t options[METIS_NOPTIONS];
  METIS_SetDefaultOptions(options);
  options[METIS_OPTION_NUMBERING] = 0;
  options[METIS_OPTION_IPTYPE] = METIS_IPTYPE_NODE; // needed?
  options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
  options[METIS_OPTION_PTYPE] = METIS_PTYPE_KWAY;

  METIS_PartMeshDual(&nEles,&nVerts,eptr.data(),eind.data(),NULL,NULL,
                     &ncommon,&nproc,NULL,options,&objval,epart.data(),npart.data());

  // Copy data to the global arrays & reset local arrays
  nEles_g   = nEles;
  nVerts_g  = nVerts;
  c2v_g     = c2v;      c2v.setup(0,0);
  xv_g      = xv;       xv.setup(0,0);
  ctype_g   = ctype;    ctype.resize(0);
  c2nv_g    = c2nv;     c2nv.resize(0);
  c2ne_g    = c2nf;     c2nf.resize(0);
  bndPts_g  = bndPts;   bndPts.setup(0,0);
  nBndPts_g = nBndPts;  nBndPts.resize(0);

  // Each processor will now grab its own data according to its rank (proc ID)
  for (int i=0; i<nEles; i++) {
    if (epart[i] == rank) {
      c2v.insertRow(c2v_g[i],-1,c2nv_g[i]);
      ic2icg.push_back(i);
      ctype.push_back(ctype_g[i]);
      c2nv.push_back(c2nv_g[i]);
      c2nf.push_back(c2ne_g[i]);
    }
  }

  nEles = c2v.getDim0();

  // Get list of all vertices (their global IDs) used in new partition
  set<int> myNodes;
  for (int i=0; i<nEles; i++) {
    for (int j=0; j<c2nv[i]; j++) {
      myNodes.insert(c2v(i,j));
    }
  }

  nVerts = myNodes.size();

  // Map from global to local to reset c2v array using local data
  vector<int> ivg2iv;
  ivg2iv.assign(nVerts_g,-1);

  // Transfer over all needed vertices to local array
  int nv = 0;
  for (auto &iv: myNodes) {
    xv.insertRow(xv_g.getRow(iv));
    iv2ivg.push_back(iv);
    ivg2iv[iv] = nv;
    nv++;
  }

  // bndPts array was already setup globally, so remake keeping only local nodes
  vector<set<int>> boundPoints(nBounds);

  for (int i=0; i<nBounds; i++) {
    for (int j=0; j<nBndPts_g[i]; j++) {
      if (findFirst(iv2ivg,bndPts_g(i,j)) != -1) {
        boundPoints[i].insert(bndPts_g(i,j));
      }
    }
  }

  int maxNBndPts = 0;
  for (int i=0; i<nBounds; i++) {
    nBndPts[i] = boundPoints[i].size();
    maxNBndPts = max(maxNBndPts,nBndPts[i]);
  }

  // Copy temp boundPoints data into bndPts matrix
  // [Transform global node IDs --> local]
  bndPts.setup(nBounds,maxNBndPts);
  for (int i=0; i<nBounds; i++) {
    int j = 0;
    for (auto& it:boundPoints[i]) {
      bndPts(i,j) = ivg2iv[it];
      j++;
    }
  }

  // Lastly, update c2v from global --> local node IDs
  std::transform(c2v.getData(),c2v.getData()+c2v.getSize(),c2v.getData(), [=](int ivg){return ivg2iv[ivg];});

  if (meshType == OVERSET_MESH)
    cout << "Geo:   Grid block " << gridID << " on rank " << rank << ": nEles = " << nEles << endl;
  else
    cout << "Geo:   On rank " << rank << ": nEles = " << nEles << endl;

  if (rank == 0) cout << "Geo: Done partitioning mesh" << endl;

  MPI_Barrier(MPI_COMM_WORLD);
#endif
}

///#include "../include/geo.inl"
