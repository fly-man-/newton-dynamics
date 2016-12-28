/* Copyright (c) <2003-2016> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "dgPhysicsStdafx.h"
#include "dgBody.h"
#include "dgWorld.h"
#include "dgMeshEffect.h"
#include "dgCollisionConvexHull.h"


void dgMeshEffect::LoadOffMesh(const char* const fileName)
{
	class ParceOFF
	{
		public:
		enum Token
		{
			m_off,
			m_value,
			m_end,
		};

		ParceOFF(FILE* const file)
			:m_file(file)
		{
		}

		Token GetToken(char* const buffer) const
		{
			while (!feof(m_file) && fscanf(m_file, "%s", buffer)) {
				if (buffer[0] == '#') {
					SkipLine();
				} else {
					if (!_stricmp(buffer, "OFF")) {
						return m_off;
					}
					return m_value;
				}
			}
			return m_end;
		}

		char* SkipLine() const
		{
			char tmp[1024];
			return fgets(tmp, sizeof (tmp), m_file);
		}

		dgInt32 GetInteger() const
		{
			char buffer[1024];
			GetToken(buffer);
			return atoi(buffer);
		}

		dgFloat64 GetFloat() const
		{
			char buffer[1024];
			GetToken(buffer);
			return atof(buffer);
		}

		FILE* m_file;
	};

	FILE* const file = fopen(fileName, "rb");
	if (file) {
		ParceOFF parcel(file);

		dgInt32 vertexCount = 0;
		dgInt32 faceCount = 0;
		//dgInt32 edgeCount = 0;

		char buffer[1024];
		bool stillData = true;
		while (stillData) {
			ParceOFF::Token token = parcel.GetToken(buffer);
			switch (token) 
			{
				case ParceOFF::m_off:
				{
					vertexCount = parcel.GetInteger();
					faceCount = parcel.GetInteger();
					//					edgeCount = parcel.GetInteger();
					parcel.SkipLine();

					dgArray<dgBigVector> points(GetAllocator());
					for (dgInt32 i = 0; i < vertexCount; i++) {
						dgFloat64 x = parcel.GetFloat();
						dgFloat64 y = parcel.GetFloat();
						dgFloat64 z = parcel.GetFloat();
						dgBigVector p(x, y, z, dgFloat32(0.0f));
						points[i] = p;
					}

					dgArray<dgInt32> indexList(GetAllocator());
					dgArray<dgInt32> faceVertex(GetAllocator());
					dgInt32 index = 0;
					for (dgInt32 i = 0; i < faceCount; i++) {
						const dgInt32 faceVertexCount = parcel.GetInteger();
						faceVertex[i] = faceVertexCount;
						for (dgInt32 j = 0; j < faceVertexCount; j++) {
							indexList[index] = parcel.GetInteger();
							index++;
						}
						parcel.SkipLine();
					}

					dgMeshVertexFormat vertexFormat;
					vertexFormat.m_faceCount = faceCount;
					vertexFormat.m_faceIndexCount = &faceVertex[0];

					vertexFormat.m_vertex.m_data = &points[0].m_x;
					vertexFormat.m_vertex.m_strideInBytes = sizeof (dgBigVector);
					vertexFormat.m_vertex.m_indexList = &indexList[0];
					BuildFromIndexList(&vertexFormat);

					CalculateNormals(3.1416f * 30.0f / 180.0f);
					stillData = false;
					break;
				}

				default:;
			}
		}

		fclose(file);
	}
}

void dgMeshEffect::LoadTetraMesh (const char* const filename)
{
	FILE* const file = fopen(filename, "rb");
	if (file) {
		dgInt32 vertexCount;
		fscanf(file, "%d", &vertexCount);
		dgArray<dgBigVector> points(GetAllocator());
		for (dgInt32 i = 0; i < vertexCount; i ++) {
			dgFloat32 x;
			dgFloat32 y;
			dgFloat32 z;
			fscanf(file, "%f %f %f", &x, &y, &z);
			points[i] = dgBigVector (x, y, z, dgFloat32 (0.0f));
		}
		
		BeginBuild();
		dgInt32 tetras;
		fscanf(file, "%d", &tetras);
		dgMemoryAllocator* const allocator = GetAllocator();
		for (dgInt32 layers = 0; layers < tetras; layers ++) {
			dgInt32 tetra[4];
			fscanf(file, "%d %d %d %d", &tetra[0], &tetra[1], &tetra[2], &tetra[3]);
			dgBigVector pointArray[4];
			for (dgInt32 i = 0; i < 4; i++) {
				dgInt32 index = tetra[i];
				pointArray[i] = points[index];
			}

			dgMeshEffect convexMesh(allocator, &pointArray[0].m_x, 4, sizeof (dgBigVector), dgFloat64(0.0f));

			dgAssert(convexMesh.GetCount());
			convexMesh.CalculateNormals(dgFloat32(30.0f * 3.1416f / 180.0f));
			for (dgInt32 i = 0; i < convexMesh.m_points.m_vertex.m_count; i++) {
				convexMesh.m_points.m_layers[i] = layers;
			}
			MergeFaces(&convexMesh);
		}
		EndBuild(dgFloat64(1.0e-8f), false);
		fclose(file);
	}
}

dgMeshEffect* dgMeshEffect::CreateVoronoiConvexDecomposition (dgMemoryAllocator* const allocator, dgInt32 pointCount, dgInt32 pointStrideInBytes, const dgFloat32* const pointCloud, dgInt32 materialId, const dgMatrix& textureProjectionMatrix)
{
	dgStack<dgBigVector> buffer(pointCount + 16);
	dgBigVector* const pool = &buffer[0];
	dgInt32 count = 0;
	dgFloat64 quantizeFactor = dgFloat64 (16.0f);
	dgFloat64 invQuantizeFactor = dgFloat64 (1.0f) / quantizeFactor;
	dgInt32 stride = pointStrideInBytes / sizeof (dgFloat32); 

	dgBigVector pMin (dgFloat32 (1.0e10f), dgFloat32 (1.0e10f), dgFloat32 (1.0e10f), dgFloat32 (0.0f));
	dgBigVector pMax (dgFloat32 (-1.0e10f), dgFloat32 (-1.0e10f), dgFloat32 (-1.0e10f), dgFloat32 (0.0f));
	for (dgInt32 i = 0; i < pointCount; i ++) {
		dgFloat64 x = pointCloud[i * stride + 0];
		dgFloat64 y	= pointCloud[i * stride + 1];
		dgFloat64 z	= pointCloud[i * stride + 2];
		x = floor (x * quantizeFactor) * invQuantizeFactor;
		y = floor (y * quantizeFactor) * invQuantizeFactor;
		z = floor (z * quantizeFactor) * invQuantizeFactor;
		dgBigVector p (x, y, z, dgFloat64 (0.0f));
		pMin = dgBigVector (dgMin (x, pMin.m_x), dgMin (y, pMin.m_y), dgMin (z, pMin.m_z), dgFloat64 (0.0f));
		pMax = dgBigVector (dgMax (x, pMax.m_x), dgMax (y, pMax.m_y), dgMax (z, pMax.m_z), dgFloat64 (0.0f));
		pool[count] = p;
		count ++;
	}
	// add the bbox as a barrier
	pool[count + 0] = dgBigVector ( pMin.m_x, pMin.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 1] = dgBigVector ( pMax.m_x, pMin.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 2] = dgBigVector ( pMin.m_x, pMax.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 3] = dgBigVector ( pMax.m_x, pMax.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 4] = dgBigVector ( pMin.m_x, pMin.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 5] = dgBigVector ( pMax.m_x, pMin.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 6] = dgBigVector ( pMin.m_x, pMax.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 7] = dgBigVector ( pMax.m_x, pMax.m_y, pMax.m_z, dgFloat64 (0.0f));
	count += 8;

	dgStack<dgInt32> indexList(count);
	count = dgVertexListToIndexList(&pool[0].m_x, sizeof (dgBigVector), 3, count, &indexList[0], dgFloat64 (5.0e-2f));	
	dgAssert (count >= 8);

	dgFloat64 maxSize = dgMax(pMax.m_x - pMin.m_x, pMax.m_y - pMin.m_y, pMax.m_z - pMin.m_z);
	pMin -= dgBigVector (maxSize, maxSize, maxSize, dgFloat64 (0.0f));
	pMax += dgBigVector (maxSize, maxSize, maxSize, dgFloat64 (0.0f));

	// add the a guard zone, so that we do no have to clip
	dgInt32 guardVertexKey = count;
	pool[count + 0] = dgBigVector ( pMin.m_x, pMin.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 1] = dgBigVector ( pMax.m_x, pMin.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 2] = dgBigVector ( pMin.m_x, pMax.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 3] = dgBigVector ( pMax.m_x, pMax.m_y, pMin.m_z, dgFloat64 (0.0f));
	pool[count + 4] = dgBigVector ( pMin.m_x, pMin.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 5] = dgBigVector ( pMax.m_x, pMin.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 6] = dgBigVector ( pMin.m_x, pMax.m_y, pMax.m_z, dgFloat64 (0.0f));
	pool[count + 7] = dgBigVector ( pMax.m_x, pMax.m_y, pMax.m_z, dgFloat64 (0.0f));
	count += 8; 

	dgDelaunayTetrahedralization delaunayTetrahedras (allocator, &pool[0].m_x, count, sizeof (dgBigVector), dgFloat32 (0.0f));
	delaunayTetrahedras.RemoveUpperHull ();

//	delaunayTetrahedras.Save("xxx0.txt");
	dgInt32 tetraCount = delaunayTetrahedras.GetCount();
	dgStack<dgBigVector> voronoiPoints(tetraCount + 32);
	dgStack<dgDelaunayTetrahedralization::dgListNode*> tetradrumNode(tetraCount);
	dgTree<dgList<dgInt32>, dgInt32> delaunayNodes (allocator);	

	dgInt32 index = 0;
	const dgConvexHull4dVector* const convexHulPoints = delaunayTetrahedras.GetHullVertexArray();
	for (dgDelaunayTetrahedralization::dgListNode* node = delaunayTetrahedras.GetFirst(); node; node = node->GetNext()) {
		dgConvexHull4dTetraherum& tetra = node->GetInfo();
		voronoiPoints[index] = tetra.CircumSphereCenter (convexHulPoints);
		tetradrumNode[index] = node;

		for (dgInt32 i = 0; i < 4; i ++) {
			dgTree<dgList<dgInt32>, dgInt32>::dgTreeNode* header = delaunayNodes.Find(tetra.m_faces[0].m_index[i]);
			if (!header) {
				dgList<dgInt32> list (allocator);
				header = delaunayNodes.Insert(list, tetra.m_faces[0].m_index[i]);
			}
			header->GetInfo().Append (index);
		}
		index ++;
	}

	const dgFloat32 normalAngleInRadians = dgFloat32 (30.0f * 3.1416f / 180.0f);
	dgMeshEffect* const voronoiPartition = new (allocator) dgMeshEffect (allocator);
	voronoiPartition->BeginBuild();
	dgInt32 layer = 0;
	dgTree<dgList<dgInt32>, dgInt32>::Iterator iter (delaunayNodes);
	for (iter.Begin(); iter; iter ++) {
		dgTree<dgList<dgInt32>, dgInt32>::dgTreeNode* const nodeNode = iter.GetNode();
		const dgList<dgInt32>& list = nodeNode->GetInfo();
		dgInt32 key = nodeNode->GetKey();

		if (key < guardVertexKey) {
			dgBigVector pointArray[512];
			dgInt32 indexArray[512];
			
			dgInt32 count = 0;
			for (dgList<dgInt32>::dgListNode* ptr = list.GetFirst(); ptr; ptr = ptr->GetNext()) {
				dgInt32 i = ptr->GetInfo();
				pointArray[count] = voronoiPoints[i];
				count ++;
				dgAssert (count < dgInt32 (sizeof (pointArray) / sizeof (pointArray[0])));
			}

			count = dgVertexListToIndexList(&pointArray[0].m_x, sizeof (dgBigVector), 3, count, &indexArray[0], dgFloat64 (1.0e-3f));	
			if (count >= 4) {
				dgMeshEffect convexMesh (allocator, &pointArray[0].m_x, count, sizeof (dgBigVector), dgFloat64 (0.0f));
				if (convexMesh.GetCount()) {
					convexMesh.CalculateNormals(normalAngleInRadians);
					convexMesh.UniformBoxMapping (materialId, textureProjectionMatrix);
					for (dgInt32 i = 0; i < convexMesh.m_points.m_vertex.m_count; i ++) {
						convexMesh.m_points.m_layers[i] = layer;
					}
					voronoiPartition->MergeFaces(&convexMesh);
					layer ++;
				}
			}
		}
	}
	voronoiPartition->EndBuild(dgFloat64 (1.0e-8f), false);
	//voronoiPartition->SaveOFF("xxx0.off");
	return voronoiPartition;
}



class dgTetraIsoSufaceStuffing
{
	public:
	class dgTetrahedra
	{
		public:
		const dgInt32& operator[] (dgInt32 i) const
		{
			return m_index[i];
		}

		dgInt32& operator[] (dgInt32 i)
		{
			return m_index[i];
		}

		private:
		dgInt32 m_index[4];
	};

	dgTetraIsoSufaceStuffing (dgMeshEffect* const mesh, dgFloat64 cellSize)
		:m_points(mesh->GetAllocator())
		,m_tetraList(mesh->GetAllocator())
		,m_pointCount(0)
		,m_gridSizeX(0)
		,m_gridSizeY(0)
		,m_gridSizeZ(0)
	{
		dgBigVector origin (CalculateGridSize (mesh, cellSize));
		PopulatePointCrid (origin, cellSize);
		BuildTetraList ();
	}

	dgBigVector CalculateGridSize (const dgMeshEffect* const mesh, dgFloat64 cellsize)
	{
		dgBigVector minBox;
		dgBigVector maxBox;
		mesh->CalculateAABB(minBox, maxBox);
		minBox -= (maxBox - minBox).Scale3(dgFloat64(1.e-3f));
		maxBox += (maxBox - minBox).Scale3(dgFloat64(1.e-3f));

		dgBigVector mMinInt((minBox.Scale4(dgFloat64(1.0f) / cellsize)).Floor());
		dgBigVector mMaxInt((maxBox.Scale4(dgFloat64(1.0f) / cellsize)).Floor() + dgBigVector::m_one);

		dgBigVector gridSize(mMaxInt - mMinInt + dgBigVector::m_one);
		m_gridSizeX = dgInt32(gridSize.m_x);
		m_gridSizeY = dgInt32(gridSize.m_y);
		m_gridSizeZ = dgInt32(gridSize.m_z);
		return minBox;
	}

	void PopulatePointCrid (const dgBigVector& origin, dgFloat64 cellsize)
	{
		m_pointCount = 0;
		m_points.Resize(m_gridSizeX * m_gridSizeY * m_gridSizeZ + (m_gridSizeX + 1) * (m_gridSizeY + 1) * (m_gridSizeZ + 1));
		for (dgInt32 z = 0; z < m_gridSizeZ; z++) {
			for (dgInt32 y = 0; y < m_gridSizeY; y++) {
				for (dgInt32 x = 0; x < m_gridSizeX; x++) {
					m_points[m_pointCount] = origin + dgBigVector(x*cellsize, y*cellsize, z*cellsize, dgFloat64(0.0f));
					m_pointCount++;
				}
			}
		}

		dgBigVector outerOrigin (origin - dgBigVector (cellsize * dgFloat64 (0.5f)));
		for (dgInt32 z = 0; z < m_gridSizeZ + 1; z++) {
			for (dgInt32 y = 0; y < m_gridSizeY + 1; y++) {
				for (dgInt32 x = 0; x < m_gridSizeX + 1; x++) {
					m_points[m_pointCount] = outerOrigin + dgBigVector(x*cellsize, y*cellsize, z*cellsize, dgFloat64(0.0f));
					m_pointCount++;
				}
			}
		}
	}

	void BuildTetraList ()
	{
		const dgInt32 base = m_gridSizeX * m_gridSizeY * m_gridSizeZ;
		for (dgInt32 z = 0; z < m_gridSizeZ; z++) {
			for (dgInt32 y = 0; y < m_gridSizeY; y++) {
				for (dgInt32 x = 0; x < m_gridSizeX - 1; x++) {
					dgTetrahedra tetra;
					tetra[0] = ((z * m_gridSizeY + y) * m_gridSizeX) + x;
					tetra[1] = ((z * m_gridSizeY + y) * m_gridSizeX) + x + 1;
					tetra[2] = base + ((z * (m_gridSizeY + 1) + y + 0) * (m_gridSizeX + 1)) + x + 1;
					tetra[3] = base + ((z * (m_gridSizeY + 1) + y + 1) * (m_gridSizeX + 1)) + x + 1;
					dgAssert (TestVolume(tetra));
					m_tetraList.Append(tetra);
				}
			}
		}
	}

	bool TestVolume (const dgTetrahedra& tetra) const
	{
		const dgBigVector& p0 = m_points[tetra[0]];
		const dgBigVector& p1 = m_points[tetra[1]];
		const dgBigVector& p2 = m_points[tetra[2]];
		const dgBigVector& p3 = m_points[tetra[3]];
		dgBigVector p10(p1 - p0);
		dgBigVector p20(p2 - p0);
		dgBigVector p30(p3 - p0);
		return p10.DotProduct3(p20.CrossProduct3(p30)) > dgFloat64 (0.0f);
	}

	dgArray<dgBigVector> m_points;
	dgList<dgTetrahedra> m_tetraList;
	dgInt32 m_pointCount;
	dgInt32 m_gridSizeX;
	dgInt32 m_gridSizeY;
	dgInt32 m_gridSizeZ;
};

dgMeshEffect* dgMeshEffect::CreateTetrahedraIsoSurface() const
{
	dgMeshEffect copy(*this);
	copy.Triangulate();

	dgTetraIsoSufaceStuffing tetraIsoStuffing (&copy, dgFloat64(0.25f));

	dgMeshEffect* delaunayPartition = NULL;
	if (tetraIsoStuffing.m_tetraList.GetCount()) {
		dgMemoryAllocator* const allocator = GetAllocator();
		delaunayPartition = new (allocator) dgMeshEffect (allocator);
		delaunayPartition->BeginBuild();
		dgInt32 layer = 0;
		dgBigVector pointArray[4];
		for (dgList<dgTetraIsoSufaceStuffing::dgTetrahedra>::dgListNode* tetNode = tetraIsoStuffing.m_tetraList.GetFirst(); tetNode; tetNode = tetNode->GetNext()) {
			dgTetraIsoSufaceStuffing::dgTetrahedra& tetra = tetNode->GetInfo();
			for (dgInt32 i = 0; i < 4; i ++) {
				dgInt32 index = tetra[i];
				pointArray[i] = tetraIsoStuffing.m_points[index];
			}
			dgMeshEffect convexMesh(allocator, &pointArray[0].m_x, 4, sizeof (dgBigVector), dgFloat64(0.0f));
			dgAssert (convexMesh.GetCount());
			convexMesh.CalculateNormals(dgFloat32 (30.0f * 3.1416f / 180.0f));

			for (dgInt32 i = 0; i < convexMesh.m_points.m_vertex.m_count; i++) {
				convexMesh.m_points.m_layers[i] = layer;
			}
			delaunayPartition->MergeFaces(&convexMesh);
			layer++;
		}
		delaunayPartition->EndBuild(dgFloat64(1.0e-8f), false);
	}

	return delaunayPartition;
}




