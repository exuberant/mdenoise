/* mdenoise.h: header for feature-preserving mesh denoising.
 * Copyright (C) 2007 Cardiff University, UK
 *
 * Version: 1.0
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Su
 *
 * Author: Xianfang Sun
 */

#ifndef MDENOISE_H
#define MDENOISE_H 1

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

// Original Mesh 
int			m_nNumVertex;
int			m_nNumFace;
FVECTOR3*	m_pf3Vertex;
NVECTOR3*	m_pn3Face;
FVECTOR3*	m_pf3FaceNormal;
FVECTOR3*	m_pf3VertexNormal;
int**		m_ppnVRing1V; //1-Ring neighbouring vertices of each vertex  
int**		m_ppnVRing1T; //1-Ring neighbouring triangles of each vertex 
int**		m_ppnTRing1TCV; //1-Ring neighbouring triangles with common vertex of each triangle 
int**		m_ppnTRing1TCE; //1-Ring neighbouring triangles with common edge of each triangle 

//Scale parameter
float		m_fScale;
float		m_f3Centre[3];

// Produced Mesh 
int			m_nNumVertexP;
int			m_nNumFaceP;
FVECTOR3*	m_pf3VertexP;
NVECTOR3*	m_pn3FaceP;
FVECTOR3*	m_pf3FaceNormalP;
FVECTOR3*	m_pf3VertexNormalP;

//Operation Parameters
bool m_bNeighbourCV;
float m_fSigma;
int m_nIterations;
int m_nVIterations;

//Add vertices in triangulation 
bool m_bAddVertices;
//Only z-direction position is updated
bool m_bZOnly;

//lowercase comparison of strings
int strcicmp(const char *string1, const char *string2);

// Header file for ESRI
struct ESRIHeader {
  int ncols;                  /* number of columns */
  int nrows;                  /* number of rows */
  double xllcorner;           /* western (left) x-coordinate */
  double yllcorner;           /* southern (bottom) y-coordinate */
  double cellsize;            /* length of one side of a square cell */
  double nodata_value;        /* value for missing data */
  bool isnodata;              /* the header has nodata_value line */
  int * index;
};


// File Operations
int FindInputExt(char* pPath);
int FindOutputExt(char* pPath);

int ReadData(FILE * fp, int nfileext, struct ESRIHeader* header);
void ReadGTS(FILE* fp);
void ReadOBJ(FILE* fp);
void ReadOFF(FILE* fp);
void ReadPLY(FILE* fp);
void ReadPLY2(FILE* fp);
void ReadSMF(FILE* fp);
void ReadSTL(FILE* fp);
void ReadWRL(FILE* fp);
void ReadXYZ(FILE* fp);
void ReadESRI(FILE* fp, struct ESRIHeader* header);

void SaveData(FILE * fp, int nfileext, struct ESRIHeader* header);
void SaveOBJ(FILE * fp);
void SaveOFF(FILE * fp);
void SavePLY(FILE * fp);
void SavePLY2(FILE * fp);
void SaveXYZ(FILE * fp);
void SaveESRI(FILE * fp, struct ESRIHeader* header);

// Preprocessing Operations
void ScalingBox(void);
void V3Normalize(FVECTOR3 v);
void ComputeNormal(bool bProduced);
void ComputeVRing1V(void);
void ComputeVRing1T(void);
void ComputeTRing1TCV(void);
void ComputeTRing1TCE(void);

// Main Operations
void MeshDenoise(bool bNeighbourCV, float fSigma, int nIterations, int nVIterations);
void VertexUpdate(int** tRing, int nVIterations);

// Command Line Options
void options(char *progname);

#endif // MDENOISE_H
