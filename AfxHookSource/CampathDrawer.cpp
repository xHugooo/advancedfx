#include "stdafx.h"

// Copyright (c) advancedfx.org
//
// Last changes:
// 2015-08-03 dominik.matrixstorm.com
//
// First changes:
// 2010-04-22 dominik.matrixstorm.com

#include "CampathDrawer.h"

#include "SourceInterfaces.h"
#include "hlaeFolder.h"
#include "RenderView.h"
#include "WrpVEngineClient.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <stdio.h>
#include <string>

/// <remarks>Must be at least 4.</remarks>
const UINT c_VertexBufferVertexCount = 200;

const FLOAT c_CampathCrossRadius = 36.0f;
const FLOAT c_CampathCrossPixelWidth = 4.0f;

const FLOAT c_CameraRadius = c_CampathCrossRadius / 2.0f;
const FLOAT c_CameraPixelWidth = 4.0f;

const FLOAT c_CameraTrajectoryPixelWidth = 8.0f;

/// <summary>Epsilon for point reduction in world units (inch).</summary>
/// <remarks>Must be at least 0.0.</remarks>
const double c_CameraTrajectoryEpsilon = 1.0f;

/// <remarks>Must be at least 2.</remarks>
const size_t c_CameraTrajectoryMaxPointsPerInterval = 1024;

CCampathDrawer g_CampathDrawer;

extern WrpVEngineClient * g_VEngineClient;


bool GetShaderDirectory(std::string & outShaderDirectory)
{
	outShaderDirectory.assign(GetHlaeFolder());

	outShaderDirectory.append("resources\\AfxHookSource\\shaders\\");

	return true;
}

// CCampathDrawer //////////////////////////////////////////////////////////////

CCampathDrawer::CCampathDrawer()
: m_Draw(false)
, m_RebuildDrawing(true)
, m_SetupEngineViewCalled(false)
, m_VertexBuffer(0)
, m_VertexBufferVertexCount(0)
, m_LockedVertexBuffer(0)
{
	m_Device = 0;
	m_PsoFileName = 0;
	m_VsoFileName = 0;
	m_ReloadPixelShader = false;
	m_ReloadVertexShader = false;
	m_PixelShader = 0;
	m_VertexShader = 0;

	LoadVso("afx_line_vs20.fxo");
	LoadPso("afx_line_ps20.fxo");
}

CCampathDrawer::~CCampathDrawer()
{
	free(m_PsoFileName);
	free(m_VsoFileName);
}

void CCampathDrawer::AutoPolyLineStart()
{
	m_PolyLineStarted = true;
}

void CCampathDrawer::AutoPolyLinePoint(Vector3 previous, Vector3 current, DWORD colorCurrent, Vector3 next)
{
	// make sure we have enough space:
	if(c_VertexBufferVertexCount < m_VertexBufferVertexCount+2)
		AutoPolyLineFlush();

	bool isFirst = 0 == m_VertexBufferVertexCount;

	if(!m_LockedVertexBuffer)
	{
		bool locked = LockVertexBuffer();
		if(!locked)
			return;
	}

	Vertex * curBuf = &(m_LockedVertexBuffer[m_VertexBufferVertexCount]);
	if(isFirst && !m_PolyLineStarted)
	{
		BuildPolyLinePoint(m_OldPreviousPolyLinePoint, previous, m_OldCurrentColor, current, curBuf);
		curBuf += 2;
		m_VertexBufferVertexCount += 2;
	}

	BuildPolyLinePoint(previous, current, colorCurrent, next, curBuf);
	m_VertexBufferVertexCount += 2;

	m_PolyLineStarted = false;
	m_OldCurrentColor = colorCurrent;
	m_OldPreviousPolyLinePoint = previous;
}

void CCampathDrawer::AutoPolyLineFlush()
{
	if(!m_LockedVertexBuffer)
		return;

	UnlockVertexBuffer();

	m_Device->SetStreamSource(0, m_VertexBuffer, 0, sizeof(Vertex));

	UINT startVertex = 0;
	UINT primitiveCount = m_VertexBufferVertexCount -2;

	// Draw lines:
	m_Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, startVertex, primitiveCount);
	startVertex += 2*(primitiveCount+1);

	m_VertexBufferVertexCount = 0;
}

void CCampathDrawer::AutoSingleLine(Vector3 from, DWORD colorFrom, Vector3 to, DWORD colorTo)
{
	// make sure we have space for another line:
	if(c_VertexBufferVertexCount < m_VertexBufferVertexCount+4)
		AutoSingleLineFlush();

	if(!m_LockedVertexBuffer)
	{
		bool locked = LockVertexBuffer();
		if(!locked)
			return;
	}

	Vertex * curBuf = &(m_LockedVertexBuffer[m_VertexBufferVertexCount]);
	BuildSingleLine(from, to, curBuf);
	BuildSingleLine(colorFrom, colorTo, curBuf);
	m_VertexBufferVertexCount += 4;
}

void CCampathDrawer::AutoSingleLineFlush()
{
	if(!m_LockedVertexBuffer)
		return;

	UnlockVertexBuffer();

	m_Device->SetStreamSource(0, m_VertexBuffer, 0, sizeof(Vertex));

	UINT startVertex = 0;
	UINT lineCount = m_VertexBufferVertexCount / 4;
	for(UINT i=0; i<lineCount; ++i)
	{
		// Draw line:
		m_Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, startVertex, 2);

		startVertex += 4;
	}

	m_VertexBufferVertexCount = 0;
}

void CCampathDrawer::BeginDevice(IDirect3DDevice9 * device)
{
	EndDevice();

	if(0 == device)
		return;

	device->AddRef();

	m_Device = device;

	LoadShader();

	g_Hook_VClient_RenderView.m_CamPath.OnChanged_set(this);

	m_RebuildDrawing = true;
}

void CCampathDrawer::BuildPolyLinePoint(Vector3 previous, Vector3 current, DWORD currentColor, Vector3 next, Vertex * pOutVertexData)
{
	Vector3 toPrevious = (previous-current).Normalize();
	Vector3 toNext = (next-current).Normalize();
	double lengthPrevious = (previous-current).Length()/8192;
	double lengthNext = (next-current).Length()/8192;
	
	pOutVertexData[1].x = pOutVertexData[0].x = (float)current.X;
	pOutVertexData[1].y = pOutVertexData[0].y = (float)current.Y;
	pOutVertexData[1].z = pOutVertexData[0].z = (float)current.Z;

	pOutVertexData[3].t1u = pOutVertexData[2].t1u = pOutVertexData[1].t1u = pOutVertexData[0].t1u = (float)toPrevious.X;
	pOutVertexData[3].t1v = pOutVertexData[2].t1v = pOutVertexData[1].t1v = pOutVertexData[0].t1v = (float)toPrevious.Y;
	pOutVertexData[3].t1w = pOutVertexData[2].t1w = pOutVertexData[1].t1w = pOutVertexData[0].t1w = (float)toPrevious.Z;

	pOutVertexData[3].t2u = pOutVertexData[2].t2u = pOutVertexData[1].t2u = pOutVertexData[0].t2u = (float)toNext.X;
	pOutVertexData[3].t2v = pOutVertexData[2].t2v = pOutVertexData[1].t2v = pOutVertexData[0].t2v = (float)toNext.Y;
	pOutVertexData[3].t2w = pOutVertexData[2].t2w = pOutVertexData[1].t2w = pOutVertexData[0].t2w = (float)toNext.Z;

	pOutVertexData[0].t0u = 1.0f;
	pOutVertexData[1].t0u = -1.0f;

	pOutVertexData[1].t0v = pOutVertexData[0].t0v = (float)lengthPrevious;
	pOutVertexData[1].t0w = pOutVertexData[0].t0w = (float)lengthNext;

	pOutVertexData[1].diffuse = pOutVertexData[0].diffuse = currentColor;
}

void CCampathDrawer::CamPathChanged(CamPath * obj)
{
	m_RebuildDrawing = true;
}

void CCampathDrawer::Draw_set(bool value)
{
	m_Draw = value;
}

bool CCampathDrawer::Draw_get(void)
{
	return m_Draw;
}

void CCampathDrawer::EndDevice()
{
	if(0 == m_Device)
		return;

	g_Hook_VClient_RenderView.m_CamPath.OnChanged_set(0);

	UnloadVertexBuffer();

	UnloadShader();

	m_Device->Release();
	m_Device = 0;
}

void CCampathDrawer::LoadPso(char const * fileName)
{
	if(m_PsoFileName) free(m_PsoFileName);
	m_PsoFileName = 0;

	if(fileName)
	{
		size_t size = 1 +strlen(fileName);

		m_PsoFileName = (char *)malloc(size);

		if(m_PsoFileName)
			strcpy_s(m_PsoFileName, size, fileName);

		m_ReloadPixelShader = true;
	}
}

void CCampathDrawer::LoadVso(char const * fileName)
{
	if(m_VsoFileName) free(m_VsoFileName);
	m_VsoFileName = 0;

	if(fileName)
	{
		size_t size = 1 +strlen(fileName);

		m_VsoFileName = (char *)malloc(size);

		if(m_VsoFileName)
			strcpy_s(m_VsoFileName, size, fileName);

		m_ReloadVertexShader = true;
	}
}

DWORD * CCampathDrawer::LoadShaderFileInMemory(char const * fileName)
{
	if(!fileName)
		return 0;

	std::string shaderDir;

	if(!GetShaderDirectory(shaderDir))
		return 0;

	shaderDir.append(fileName);

	FILE * file = 0;
	bool bOk = 0 == fopen_s(&file, shaderDir.c_str(), "rb");
	DWORD * so = 0;
	size_t size = 0;

	bOk = bOk 
		&& 0 != file
		&& 0 == fseek(file, 0, SEEK_END)
	;

	if(bOk)
	{
		size = ftell(file);

		so = (DWORD *)malloc(
			(size & 0x3) == 0 ? size : size +(4-(size & 0x3))
		);
		fseek(file, 0, SEEK_SET);
		bOk = 0 != so;
	}

	if(bOk)
	{
		bOk = size == fread(so, 1, size, file);
	}

	if(file) fclose(file);

	if(bOk)
		return so;
	
	if(so) free(so);
	
	return 0;
}

void CCampathDrawer::LoadShader()
{
	LoadVertexShader();
	LoadPixelShader();
}

void CCampathDrawer::LoadPixelShader()
{
	UnloadPixelShader();

	if(!m_Device || !m_PsoFileName)
		return;

	DWORD * so = LoadShaderFileInMemory(m_PsoFileName);

	if(so && SUCCEEDED(m_Device->CreatePixelShader(so, &m_PixelShader)))
		m_PixelShader->AddRef();
	else
		m_PixelShader = 0;

	if(so) free(so);

	m_ReloadPixelShader = false;
}

void CCampathDrawer::LoadVertexShader()
{
	UnloadVertexShader();

	if(!m_Device || !m_VsoFileName)
		return;

	DWORD * so = LoadShaderFileInMemory(m_VsoFileName);

	if(so && SUCCEEDED(m_Device->CreateVertexShader(so, &m_VertexShader)))
		m_VertexShader->AddRef();
	else
		m_VertexShader = 0;

	if(so) free(so);

	m_ReloadVertexShader = false;
}

#define ValToUCCondInv(value,invert) ((invert) ? 0xFF -(unsigned char)(value) : (unsigned char)(value) )

void CCampathDrawer::OnPostRenderAllTools()
{
	//if(m_SetupEngineViewCalled)
	//	return;
	
	// Actually we are often called twice per frame due to an engine bug, once after 3d skybox
	// and once after world is drawn, maybe we will be even called more times,
	// but we can not care about that for now.
	
	if(!m_Draw)
		return;

	if(!(m_Device && m_VertexShader && m_PixelShader && g_VEngineClient))
	{
		static bool firstError = true;

		if(firstError)
		{
			firstError = false;
			Tier0_Msg(
				"AFXERROR: CCampathDrawer::OnEndScene: Missing required dependencies:%s%s%s%s.\n",
				!m_Device ? " m_Device" : "",
				!m_VertexShader ? " m_VertexShader" : "",
				!m_PixelShader ? " m_PixelShader" : "",
				!g_VEngineClient ? " g_VEngineClient" : ""
			);
		}

		return;
	}

	// Save device state:

	IDirect3DPixelShader9 * oldPixelShader = 0;
	m_Device->GetPixelShader(&oldPixelShader);
	if(oldPixelShader) oldPixelShader->AddRef();

	IDirect3DVertexShader9 * oldVertexShader = 0;
	m_Device->GetVertexShader(&oldVertexShader);
	if(oldVertexShader) oldVertexShader->AddRef();

	IDirect3DVertexBuffer9 * oldVertexBuffer = 0;
	UINT oldVertexBufferOffset;
	UINT oldVertexBufferStride;
	m_Device->GetStreamSource(0, &oldVertexBuffer, &oldVertexBufferOffset, &oldVertexBufferStride);
	// this is done already according to doc: // if(oldVertexBuffer) oldVertexBuffer->AddRef();

	IDirect3DIndexBuffer9 * oldIndexBuffer = 0;
	m_Device->GetIndices(&oldIndexBuffer);
	// this is done already according to doc: // if(oldIndexBuffer) oldIndexBuffer->AddRef();

	IDirect3DVertexDeclaration9 * oldDeclaration;
	m_Device->GetVertexDeclaration(&oldDeclaration);
	if(oldDeclaration) oldDeclaration->AddRef();

	DWORD oldFVF;
	m_Device->GetFVF(&oldFVF);

	FLOAT oldCViewProj[4][4];
	m_Device->GetVertexShaderConstantF(8, oldCViewProj[0], 4);

	FLOAT oldCScreenInfo[4];
	m_Device->GetVertexShaderConstantF(48, oldCScreenInfo, 1);

	FLOAT oldCPlane0[4];
	m_Device->GetVertexShaderConstantF(49, oldCPlane0, 1);

	FLOAT oldCPlaneN[4];
	m_Device->GetVertexShaderConstantF(50, oldCPlaneN, 1);

	DWORD oldSrgbWriteEnable;
	m_Device->GetRenderState(D3DRS_SRGBWRITEENABLE, &oldSrgbWriteEnable);

	DWORD oldColorWriteEnable;
	m_Device->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);

	DWORD oldZEnable;
	m_Device->GetRenderState(D3DRS_ZENABLE, &oldZEnable);

	DWORD oldZWriteEnable;
	m_Device->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWriteEnable);
	
	DWORD oldZFunc;
	m_Device->GetRenderState(D3DRS_ZFUNC, &oldZFunc);

	DWORD oldAlphaTestEnable;
	m_Device->GetRenderState(D3DRS_ALPHATESTENABLE, &oldAlphaTestEnable);

	DWORD oldSeparateAlphaBlendEnable;
	m_Device->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, &oldSeparateAlphaBlendEnable);

	DWORD oldAlphaBlendEnable;
	m_Device->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlendEnable);

	DWORD oldBlendOp;
	m_Device->GetRenderState(D3DRS_BLENDOP, &oldBlendOp);

	DWORD oldSrcBlend;
	m_Device->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);

	DWORD oldDestBlend;
	m_Device->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);

	DWORD oldCullMode;
	m_Device->GetRenderState(D3DRS_CULLMODE, &oldCullMode);

	// Draw:
	{
		//Vector3 vvForward, vvUp, vvRight, vvPos;

		double curTime = g_Hook_VClient_RenderView.GetCurTime();
		bool inCampath = 1 <= g_Hook_VClient_RenderView.m_CamPath.GetSize()
			&&	g_Hook_VClient_RenderView.m_CamPath.GetLowerBound() <= curTime
			&& curTime <= g_Hook_VClient_RenderView.m_CamPath.GetUpperBound();
		bool campathCanEval = 4 <= g_Hook_VClient_RenderView.m_CamPath.GetSize();
		bool campathEnabled = g_Hook_VClient_RenderView.m_CamPath.IsEnabled();
		bool cameraMightBeSelected = false;

		m_Device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		m_Device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
		m_Device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		m_Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		m_Device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		m_Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		m_Device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
		m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_Device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		m_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

		m_Device->SetVertexShader(m_VertexShader);
	
		m_Device->SetVertexShaderConstantF(8, m_WorldToScreenMatrix.m[0], 4);

		// Provide view plane info for line clipping:
		{
			double plane0[4]={0,0,0,1};
			double planeN[4]={1,0,0,1};
			//double planeR[4]={0,-1,0,1};
			//double planeU[4]={0,0,1,1};

			unsigned char P[4];
			unsigned char Q[4];

			double L[4][4];
			double U[4][4];

			double M[4][4] = {
				m_WorldToScreenMatrix.m[0][0], m_WorldToScreenMatrix.m[0][1], m_WorldToScreenMatrix.m[0][2], 0,
				m_WorldToScreenMatrix.m[1][0], m_WorldToScreenMatrix.m[1][1], m_WorldToScreenMatrix.m[1][2], 0,
				m_WorldToScreenMatrix.m[2][0], m_WorldToScreenMatrix.m[2][1], m_WorldToScreenMatrix.m[2][2], 0,
				m_WorldToScreenMatrix.m[3][0], m_WorldToScreenMatrix.m[3][1], m_WorldToScreenMatrix.m[3][2], -1,
			};

			double b0[4] = {
				0 -m_WorldToScreenMatrix.m[0][3],
				0 -m_WorldToScreenMatrix.m[1][3],
				0 -m_WorldToScreenMatrix.m[2][3],
				-m_WorldToScreenMatrix.m[3][3],
			};

			double bN[4] = {
				0 -m_WorldToScreenMatrix.m[0][3],
				0 -m_WorldToScreenMatrix.m[1][3],
				1 -m_WorldToScreenMatrix.m[2][3],
				-m_WorldToScreenMatrix.m[3][3],
			};
			/*
			double bR[4] = {
				1 -m_WorldToScreenMatrix.m[0][3],
				0 -m_WorldToScreenMatrix.m[1][3],
				0 -m_WorldToScreenMatrix.m[2][3],
				-m_WorldToScreenMatrix.m[3][3],
			};

			double bU[4] = {
				0 -m_WorldToScreenMatrix.m[0][3],
				1 -m_WorldToScreenMatrix.m[1][3],
				0 -m_WorldToScreenMatrix.m[2][3],
				-m_WorldToScreenMatrix.m[3][3],
			};
			*/
			if(!LUdecomposition(M, P, Q, L, U))
			{
				Tier0_Warning("AFXERROR in CCampathDrawer::OnPostRenderAllTools: LUdecomposition failed\n");
			}
			else
			{
				SolveWithLU(L, U, P, Q, b0, plane0);
				SolveWithLU(L, U, P, Q, bN, planeN);
				
				//SolveWithLU(L, U, P, Q, bR, planeR);
				//SolveWithLU(L, U, P, Q, bU, planeU);
			}

			/*
			vvPos = Vector3(plane0[0], plane0[1], plane0[2]);
			vvForward = Vector3(planeN[0] -vvPos.X, planeN[1] -vvPos.Y, planeN[2]-vvPos.Z);
			vvForward.Normalize();
			vvRight = Vector3(planeR[0] -vvPos.X, planeR[1] -vvPos.Y, planeR[2]-vvPos.Z);
			vvRight.Normalize();
			vvUp = Vector3(planeU[0] -vvPos.X, planeU[1] -vvPos.Y, planeU[2]-vvPos.Z);
			vvUp.Normalize();
			*/

			/*
			Tier0_Msg("CCampathDrawer::OnPostRenderAllTools: curTime = %f\n",curTime);
			Tier0_Msg("M[0]=%f %f %f %f\nM[1]=%f %f %f %f\nM[2]=%f %f %f %f\nM[3]=%f %f %f %f\n", M[0][0],M[0][1],M[0][2],M[0][3], M[1][0],M[1][1],M[1][2],M[1][3], M[2][0],M[2][1],M[2][2],M[2][3], M[3][0],M[3][1],M[3][2],M[3][3]);
			Tier0_Msg("b0[0]=%f %f %f %f\n", b0[0], b0[1], b0[2], b0[3]);
			Tier0_Msg("bN[0]=%f %f %f %f\n", bN[0], bN[1], bN[2], bN[3]);
			Tier0_Msg("plane0=%f %f %f %f\n", plane0[0], plane0[1], plane0[2], plane0[3]);
			Tier0_Msg("planeN=%f %f %f %f\n", planeN[0], planeN[1], planeN[2], planeN[3]);
			*/

			FLOAT vPlane0[4] = {(float)plane0[0], (float)plane0[1], (float)plane0[2], 0.0f};

			Vector3 planeNormal(planeN[0] -plane0[0], planeN[1] -plane0[1], planeN[2] -plane0[2]);
			planeNormal.Normalize();

			FLOAT vPlaneN[4] = {(float)planeNormal.X, (float)planeNormal.Y, (float)planeNormal.Z, 0.0f};

			m_Device->SetVertexShaderConstantF(49, vPlane0, 1);
			m_Device->SetVertexShaderConstantF(50, vPlaneN, 1);
		}

		m_Device->SetPixelShader(m_PixelShader);

		m_Device->SetFVF(CCampathDrawer_VertexFVF);

		int screenWidth, screenHeight;
		g_VEngineClient->GetScreenSize(screenWidth, screenHeight);
		FLOAT newCScreenInfo[4] = { 0 != screenWidth ? 1.0f / screenWidth : 0.0f, 0 != screenHeight ? 1.0f / screenHeight : 0.0f, 0.0, 0.0f};

		// Draw trajectory:
		if(2 <= g_Hook_VClient_RenderView.m_CamPath.GetSize() && campathCanEval)
		{
			if(m_RebuildDrawing)
			{
				// Rebuild trajectory points.
				// This operation can be quite expensive (up to O(N^2)),
				// so it should be done only when s.th.
				// changed (which is what we do here).

				m_TrajectoryPoints.clear();
				
				CamPathIterator last = g_Hook_VClient_RenderView.m_CamPath.GetBegin();				
				CamPathIterator it = last;

				TempPoint * pts = new TempPoint[c_CameraTrajectoryMaxPointsPerInterval];

				for(++it; it != g_Hook_VClient_RenderView.m_CamPath.GetEnd(); ++it)
				{
					double delta = it.GetTime() -last.GetTime();

					for(size_t i = 0; i<c_CameraTrajectoryMaxPointsPerInterval; i++)
					{
						double t = last.GetTime() + delta*((double)i/(c_CameraTrajectoryMaxPointsPerInterval-1));

						CamPathValue cpv = g_Hook_VClient_RenderView.m_CamPath.Eval(t);

						pts[i].t = t;
						pts[i].y = Vector3(cpv.X, cpv.Y, cpv.Z);
						pts[i].nextPt = i+1 <c_CameraTrajectoryMaxPointsPerInterval ? &(pts[i+1]) : 0;
					}

					RamerDouglasPeucker(&(pts[0]), &(pts[c_CameraTrajectoryMaxPointsPerInterval-1]), c_CameraTrajectoryEpsilon);

					// add all points except the last one (to avoid duplicates):
					for(TempPoint * pt = &(pts[0]); pt && pt->nextPt; pt = pt->nextPt)
					{
						m_TrajectoryPoints.push_back(pt->t);
					}

					last = it;
				}

				// add last point:
				m_TrajectoryPoints.push_back(pts[c_CameraTrajectoryMaxPointsPerInterval-1].t);

				delete pts;

				m_RebuildDrawing = false;
			}

			newCScreenInfo[2] = c_CameraTrajectoryPixelWidth;
			m_Device->SetVertexShaderConstantF(48, newCScreenInfo, 1);

			AutoPolyLineStart();

			std::list<double>::iterator itPts = m_TrajectoryPoints.begin();

			CamPathIterator itKeysLast = g_Hook_VClient_RenderView.m_CamPath.GetBegin();
			CamPathIterator itKeysNext = itKeysLast;
			++itKeysNext;

			bool hasLastPt = false;
			bool hasNextPt = false;
			bool hasCurPt = false;
			
			double lastPtTime;
			Vector3 lastPtValue;
			double curPtTime;
			Vector3 curPtValue;
			double nextPtTime;
			Vector3 nextPtValue;

			do
			{
				if(hasNextPt)
				{
					hasLastPt = true;
					lastPtTime = curPtTime;
					lastPtValue = curPtValue;

					hasCurPt = true;
					curPtTime = nextPtTime;
					curPtValue = nextPtValue;

					hasNextPt = false;
				}
				else
				{
					hasCurPt = true;
					curPtTime = *itPts;
					CamPathValue curValue = g_Hook_VClient_RenderView.m_CamPath.Eval(curPtTime);
					curPtValue = Vector3(curValue.X, curValue.Y, curValue.Z);
					++itPts;
				}

				while(itKeysNext.GetTime() < curPtTime)
				{
					itKeysLast = itKeysNext;
					++itKeysNext;
				}

				if(itPts != m_TrajectoryPoints.end())
				{
					hasNextPt = true;
					nextPtTime = *itPts;
					CamPathValue nextValue = g_Hook_VClient_RenderView.m_CamPath.Eval(nextPtTime);
					nextPtValue = Vector3(nextValue.X, nextValue.Y, nextValue.Z);
					++itPts;
				}
				else
				{
					// current point is last point.
					hasNextPt = false;
					nextPtValue = curPtValue;
				}

				if(!hasLastPt)
				{
					// current point is first point.
					lastPtValue = curPtValue;
				}

				// emit current point:
				{
					double deltaTime = abs(curTime -curPtTime);

					bool selected = itKeysLast.IsSelected() && itKeysNext.IsSelected();

					DWORD colour;

					// determine colour:
					if(deltaTime < 1.0)
					{
						double t = (deltaTime -0.0)/1.0;
						colour = D3DCOLOR_RGBA(
							ValToUCCondInv(255.0*t, selected),
							ValToUCCondInv(255, selected),
							ValToUCCondInv(0, selected),
							(unsigned char)(127*(1.0-t))+128
						);
					}
					else
					if(deltaTime < 2.0)
					{
						double t = (deltaTime -1.0)/1.0;
						colour = D3DCOLOR_RGBA(
							ValToUCCondInv(255, selected),
							ValToUCCondInv(255.0*(1.0-t), selected),
							ValToUCCondInv(0, selected),
							(unsigned char)(64*(1.0-t))+64
						);
					}
					else
					{
						colour = D3DCOLOR_RGBA(
							ValToUCCondInv(255, selected),
							ValToUCCondInv(0, selected),
							ValToUCCondInv(0, selected),
							64
						);
					}

					AutoPolyLinePoint(lastPtValue, curPtValue, colour, nextPtValue);
				}
			}
			while(hasNextPt);

			AutoPolyLineFlush();
		}

		// Draw keyframes:
		{
			newCScreenInfo[2] = c_CampathCrossPixelWidth;
			m_Device->SetVertexShaderConstantF(48, newCScreenInfo, 1);

			CamPathIterator last = g_Hook_VClient_RenderView.m_CamPath.GetEnd();
			
			/*if(0 < g_Hook_VClient_RenderView.m_CamPath.GetSize())
			{
				// Test for not too unlikely hard case:
				CamPathValue cpv = g_Hook_VClient_RenderView.m_CamPath.GetBegin().GetValue();
				Vector3 current(cpv.X+76, cpv.Y+76, cpv.Z+76);
				Vector3 previous(current.X+76, current.Y-1*4, current.Z);
				Vector3 next(current.X+76, current.Y+1*4, current.Z);
				Vector3 next2(current.X, current.Y+2*4, current.Z);
				Vector3 next3(current.X+76, current.Y+3*4, current.Z);
				Vector3 next4(current.X, current.Y+4*4, current.Z);
				Vector3 next5(current.X+76, current.Y+5*4, current.Z);

				AutoPolyLineStart();
				AutoPolyLinePoint(previous, previous, D3DCOLOR_RGBA(255,0,0,255), current);
				AutoPolyLinePoint(previous, current, D3DCOLOR_RGBA(255,0,0,255), next);
				AutoPolyLinePoint(current, next, D3DCOLOR_RGBA(255,0,0,255), next2);
				AutoPolyLinePoint(next, next2, D3DCOLOR_RGBA(255,0,0,255), next3);
				AutoPolyLinePoint(next2, next3, D3DCOLOR_RGBA(255,0,0,255), next4);
				AutoPolyLinePoint(next3, next4, D3DCOLOR_RGBA(255,0,0,255), next5);
				AutoPolyLinePoint(next4, next5, D3DCOLOR_RGBA(255,0,0,255), next5);
				AutoPolyLineFlush();
			}*/
			
			/*if(0 < g_Hook_VClient_RenderView.m_CamPath.GetSize())
			{
				CamPathValue cpv = g_Hook_VClient_RenderView.m_CamPath.GetBegin().GetValue();

				float x = cpv.X * m_WorldToScreenMatrix.m[0][0] + cpv.Y * m_WorldToScreenMatrix.m[0][1] + cpv.Z * m_WorldToScreenMatrix.m[0][2] +m_WorldToScreenMatrix.m[0][3];
				float y = cpv.X * m_WorldToScreenMatrix.m[1][0] + cpv.Y * m_WorldToScreenMatrix.m[1][1] + cpv.Z * m_WorldToScreenMatrix.m[1][2] +m_WorldToScreenMatrix.m[1][3];
				float z = cpv.X * m_WorldToScreenMatrix.m[2][0] + cpv.Y * m_WorldToScreenMatrix.m[2][1] + cpv.Z * m_WorldToScreenMatrix.m[2][2] +m_WorldToScreenMatrix.m[2][3];
				float w = cpv.X * m_WorldToScreenMatrix.m[3][0] + cpv.Y * m_WorldToScreenMatrix.m[3][1] + cpv.Z * m_WorldToScreenMatrix.m[3][2] +m_WorldToScreenMatrix.m[3][3];

				float iw = w ? 1/w : 0;

				Tier0_Msg("pt: %f %f %f %f -> %f %f %f %f\n",x,y,z,w,x*iw,y*iw,z*iw,w*iw);
			}*/
		
			for(CamPathIterator it = g_Hook_VClient_RenderView.m_CamPath.GetBegin(); it != g_Hook_VClient_RenderView.m_CamPath.GetEnd(); ++it)
			{
				CamPathValue cpv = it.GetValue();

				if(last != g_Hook_VClient_RenderView.m_CamPath.GetEnd())
				{
					cameraMightBeSelected = cameraMightBeSelected || last.IsSelected() && it.IsSelected() && last.GetTime() <= curTime && curTime <= it.GetTime();
				}
				last = it;

				double deltaTime = abs(curTime -it.GetTime());

				bool selected = it.IsSelected();

				DWORD colour;

				// determine colour:
				if(deltaTime < 1.0)
				{
					double t = (deltaTime -0.0)/1.0;
					colour = D3DCOLOR_RGBA(
						ValToUCCondInv(255.0*t, selected),
						ValToUCCondInv(255, selected),
						ValToUCCondInv(0, selected),
						(unsigned char)(127*(1.0-t))+128
					);
				}
				else
				if(deltaTime < 2.0)
				{
					double t = (deltaTime -1.0)/1.0;
					colour = D3DCOLOR_RGBA(
						ValToUCCondInv(255, selected),
						ValToUCCondInv(255.0*(1.0-t), selected),
						ValToUCCondInv(0, selected),
						(unsigned char)(64*(1.0-t))+64
					);
				}
				else
				{
					colour = D3DCOLOR_RGBA(
						ValToUCCondInv(255, selected),
						ValToUCCondInv(0, selected),
						ValToUCCondInv(0, selected),
						64
					);
				}

				// x / forward line:

				AutoSingleLine(
					Vector3(cpv.X -c_CampathCrossRadius, cpv.Y, cpv.Z),
					colour,
					Vector3(cpv.X +c_CampathCrossRadius, cpv.Y, cpv.Z),
					colour
				);

				// y / left line:

				AutoSingleLine(
					Vector3(cpv.X, cpv.Y -c_CampathCrossRadius, cpv.Z),
					colour,
					Vector3(cpv.X, cpv.Y +c_CampathCrossRadius, cpv.Z),
					colour
				);

				// z / up line:

				AutoSingleLine(
					Vector3(cpv.X, cpv.Y, cpv.Z -c_CampathCrossRadius),
					colour,
					Vector3(cpv.X, cpv.Y, cpv.Z +c_CampathCrossRadius),
					colour
				);
			}

			AutoSingleLineFlush();
		}

		// Draw wireframe camera:
		if(inCampath && campathCanEval)
		{
			newCScreenInfo[2] = c_CameraPixelWidth;
			m_Device->SetVertexShaderConstantF(48, newCScreenInfo, 1);

			DWORD colourCam = campathEnabled
				? D3DCOLOR_RGBA(
					ValToUCCondInv(255,cameraMightBeSelected),
					ValToUCCondInv(0,cameraMightBeSelected),
					ValToUCCondInv(255,cameraMightBeSelected),
					128)
				: D3DCOLOR_RGBA(
					ValToUCCondInv(255,cameraMightBeSelected),
					ValToUCCondInv(255,cameraMightBeSelected),
					ValToUCCondInv(255,cameraMightBeSelected),
					128);
			DWORD colourCamUp = campathEnabled
				? D3DCOLOR_RGBA(
					ValToUCCondInv(0,cameraMightBeSelected),
					ValToUCCondInv(255,cameraMightBeSelected),
					ValToUCCondInv(0,cameraMightBeSelected),
					128)
				: D3DCOLOR_RGBA(
					ValToUCCondInv(0,cameraMightBeSelected),
					ValToUCCondInv(0,cameraMightBeSelected),
					ValToUCCondInv(0,cameraMightBeSelected),
					128);

			CamPathValue cpv = g_Hook_VClient_RenderView.m_CamPath.Eval(curTime);

			// limit to values as RenderView hook:
			cpv.Fov = max(1,cpv.Fov);
			cpv.Fov = min(179,cpv.Fov);

			double forward[3], right[3], up[3];
			MakeVectors(cpv.Roll, cpv.Pitch, cpv.Yaw, forward, right, up);

			Vector3 vCp(cpv.X, cpv.Y, cpv.Z);
			Vector3 vForward(forward);
			Vector3 vUp(up);
			Vector3 vRight(right);

			//Tier0_Msg("----------------",curTime);
			//Tier0_Msg("currenTime = %f",curTime);
			//Tier0_Msg("vCp = %f %f %f\n", vCp.X, vCp.Y, vCp.Z);

			double a = sin(cpv.Fov * M_PI / 360.0) * c_CameraRadius;
			double b = a;

			int screenWidth, screenHeight;
			g_VEngineClient->GetScreenSize(screenWidth, screenHeight);

			double aspectRatio = screenWidth ? (double)screenHeight / (double)screenWidth : 1.0;

			b *= aspectRatio;

			Vector3 vLU = vCp +(double)c_CameraRadius * vForward -a * vRight +b * vUp;
			Vector3 vRU = vCp +(double)c_CameraRadius * vForward +a * vRight +b * vUp;
			Vector3 vLD = vCp +(double)c_CameraRadius * vForward -a * vRight -b * vUp;
			Vector3 vRD = vCp +(double)c_CameraRadius * vForward +a * vRight -b * vUp;
			Vector3 vMU = vLU +(vRU -vLU)/2;
			Vector3 vMUU = vMU +(double)c_CameraRadius * vUp;

			AutoSingleLine(vCp, colourCam, vLD, colourCam);

			AutoSingleLine(vCp, colourCam, vRD, colourCam);

			AutoSingleLine(vCp, colourCam, vLU, colourCam);

			AutoSingleLine(vCp, colourCam, vRU, colourCam);

			AutoSingleLine(vLD, colourCam, vRD, colourCam);

			AutoSingleLine(vRD, colourCam, vRU, colourCam);

			AutoSingleLine(vRU, colourCam, vLU, colourCam);

			AutoSingleLine(vLU, colourCam, vLD, colourCam);

			AutoSingleLine(vMU, colourCam, vMUU, colourCamUp);

			AutoSingleLineFlush();

			//
			/*

			colourCam = D3DCOLOR_RGBA(255, 0, 0, 255);
			colourCamUp = D3DCOLOR_RGBA(255, 255, 0, 255);

			vCp = vvPos;
			vForward = vvForward;
			vUp = vvUp;
			vRight = vvRight;

			//Tier0_Msg("vCp2 = %f %f %f\n", vCp.X, vCp.Y, vCp.Z);

			vLU = vCp +(double)c_CameraRadius * vForward -a * vRight +b * vUp;
			vRU = vCp +(double)c_CameraRadius * vForward +a * vRight +b * vUp;
			vLD = vCp +(double)c_CameraRadius * vForward -a * vRight -b * vUp;
			vRD = vCp +(double)c_CameraRadius * vForward +a * vRight -b * vUp;
			vMU = vLU +(vRU -vLU)/2;
			vMUU = vMU +(double)c_CameraRadius * vUp;

			AutoSingleLine(vCp, colourCam, vLD, colourCam);

			AutoSingleLine(vCp, colourCam, vRD, colourCam);

			AutoSingleLine(vCp, colourCam, vLU, colourCam);

			AutoSingleLine(vCp, colourCam, vRU, colourCam);

			AutoSingleLine(vLD, colourCam, vRD, colourCam);

			AutoSingleLine(vRD, colourCam, vRU, colourCam);

			AutoSingleLine(vRU, colourCam, vLU, colourCam);

			AutoSingleLine(vLU, colourCam, vLD, colourCam);

			AutoSingleLine(vMU, colourCam, vMUU, colourCamUp);

			AutoSingleLineFlush();
			*/
		}
	}

	// Restore device state:

	m_Device->SetPixelShader(oldPixelShader);
	if(oldPixelShader) oldPixelShader->Release();

	m_Device->SetVertexShader(oldVertexShader);
	if(oldVertexShader) oldVertexShader->Release();

	m_Device->SetStreamSource(0, oldVertexBuffer, oldVertexBufferOffset, oldVertexBufferStride);
	if(oldVertexBuffer) oldVertexBuffer->Release();

	m_Device->SetIndices(oldIndexBuffer);
	if(oldIndexBuffer) oldIndexBuffer->Release();

	m_Device->SetFVF(oldFVF);

	m_Device->SetVertexDeclaration(oldDeclaration);
	if(oldDeclaration) oldDeclaration->Release();

	m_Device->SetVertexShaderConstantF(8, oldCViewProj[0], 4);
	m_Device->SetVertexShaderConstantF(48, oldCScreenInfo, 1);
	m_Device->SetVertexShaderConstantF(49, oldCPlane0, 1);
	m_Device->SetVertexShaderConstantF(50, oldCPlaneN, 1);

	m_Device->SetRenderState(D3DRS_CULLMODE, oldCullMode);
	m_Device->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
	m_Device->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
	m_Device->SetRenderState(D3DRS_BLENDOP, oldBlendOp);
	m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlendEnable);
	m_Device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, oldSeparateAlphaBlendEnable);
	m_Device->SetRenderState(D3DRS_ALPHATESTENABLE, oldAlphaTestEnable);
	m_Device->SetRenderState(D3DRS_ZFUNC, oldZFunc);
	m_Device->SetRenderState(D3DRS_ZWRITEENABLE, oldZWriteEnable);
	m_Device->SetRenderState(D3DRS_ZENABLE, oldZEnable);
	m_Device->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
	m_Device->SetRenderState(D3DRS_SRGBWRITEENABLE, oldSrgbWriteEnable);

	m_SetupEngineViewCalled = false;
}

void CCampathDrawer::OnSetupEngineView()
{
	m_SetupEngineViewCalled = true;
	m_WorldToScreenMatrix = g_VEngineClient->WorldToScreenMatrix();
}

void CCampathDrawer::BuildSingleLine(Vector3 from, Vector3 to, Vertex * pOutVertexData)
{
	Vector3 normal = (to-from).Normalize();
	double length = (to-from).Length() / 8192;
	
	pOutVertexData[1].x = pOutVertexData[0].x = (float)from.X;
	pOutVertexData[3].x = pOutVertexData[2].x = (float)to.X;
	pOutVertexData[1].y = pOutVertexData[0].y = (float)from.Y;
	pOutVertexData[3].y = pOutVertexData[2].y = (float)to.Y;
	pOutVertexData[1].z = pOutVertexData[0].z = (float)from.Z;
	pOutVertexData[3].z = pOutVertexData[2].z = (float)to.Z;

	pOutVertexData[3].t1u = pOutVertexData[2].t1u = pOutVertexData[1].t1u = pOutVertexData[0].t1u = (float)-normal.X;
	pOutVertexData[3].t1v = pOutVertexData[2].t1v = pOutVertexData[1].t1v = pOutVertexData[0].t1v = (float)-normal.Y;
	pOutVertexData[3].t1w = pOutVertexData[2].t1w = pOutVertexData[1].t1w = pOutVertexData[0].t1w = (float)-normal.Z;

	pOutVertexData[3].t2u = pOutVertexData[2].t2u = pOutVertexData[1].t2u = pOutVertexData[0].t2u = (float)normal.X;
	pOutVertexData[3].t2v = pOutVertexData[2].t2v = pOutVertexData[1].t2v = pOutVertexData[0].t2v = (float)normal.Y;
	pOutVertexData[3].t2w = pOutVertexData[2].t2w = pOutVertexData[1].t2w = pOutVertexData[0].t2w = (float)normal.Z;

	pOutVertexData[2].t0u = pOutVertexData[0].t0u = 1.0f;
	pOutVertexData[3].t0u = pOutVertexData[1].t0u = -1.0f;

	pOutVertexData[1].t0v = pOutVertexData[0].t0v = 0.0f;
	pOutVertexData[1].t0w = pOutVertexData[0].t0w = (float)length;
	pOutVertexData[3].t0v = pOutVertexData[2].t0v = (float)length;
	pOutVertexData[3].t0w = pOutVertexData[2].t0w = 0.0f;
}

void CCampathDrawer::BuildSingleLine(DWORD colorFrom, DWORD colorTo, Vertex * pOutVertexData)
{
	pOutVertexData[1].diffuse = pOutVertexData[0].diffuse = colorFrom;
	pOutVertexData[3].diffuse = pOutVertexData[2].diffuse = colorTo;
}

bool CCampathDrawer::LockVertexBuffer()
{
	if(m_VertexBuffer)
	{
		if(!SUCCEEDED(m_VertexBuffer->Lock(0, c_VertexBufferVertexCount * sizeof(Vertex), (void **)&m_LockedVertexBuffer, 0)))
		{
			m_LockedVertexBuffer = 0;
			return false;
		}
		return true;
	}

	UnlockVertexBuffer();

	if(!SUCCEEDED(m_Device->CreateVertexBuffer(
		c_VertexBufferVertexCount * sizeof(Vertex),
		D3DUSAGE_WRITEONLY,
		CCampathDrawer_VertexFVF,
		D3DPOOL_DEFAULT,
		&m_VertexBuffer,
		NULL
	)))
	{
		if(m_VertexBuffer) m_VertexBuffer->Release();
		m_VertexBuffer = 0;
		return false;
	}

	if(m_VertexBuffer)
	{
		if(!SUCCEEDED(m_VertexBuffer->Lock(0, c_VertexBufferVertexCount * sizeof(Vertex), (void **)&m_LockedVertexBuffer, 0)))
		{
			m_LockedVertexBuffer = 0;
			return 0;
		}
		return true;
	}

	return false;
}

void CCampathDrawer::UnlockVertexBuffer()
{
	if(m_VertexBuffer && m_LockedVertexBuffer)
	{
		m_VertexBuffer->Unlock();
		m_LockedVertexBuffer = 0;
	}
}

void CCampathDrawer::UnloadVertexBuffer()
{
	if(m_VertexBuffer) { m_VertexBuffer->Release(); m_VertexBuffer = 0; }
}

void CCampathDrawer::UnloadShader()
{
	UnloadPixelShader();
	UnloadVertexShader();
}

void CCampathDrawer::UnloadPixelShader()
{
	if(!m_PixelShader)
		return;

	m_PixelShader->Release();
	m_PixelShader = 0;
}

void CCampathDrawer::UnloadVertexShader()
{
	if(!m_VertexShader)
		return;

	m_VertexShader->Release();
	m_VertexShader = 0;
}

void CCampathDrawer::RamerDouglasPeucker(TempPoint * start, TempPoint * end, double epsilon)
{
	double dmax = 0;
	TempPoint * index = start;

	for(TempPoint * i = start->nextPt; i && i != end; i = i->nextPt)
	{
		double d = ShortestDistanceToSegment(i, start, end);
		if(d > dmax)
		{
			index = i;
			dmax = d;
		}
	}

	// If max distance is greater than epsilon, recursively simplify
	if ( dmax > epsilon )
	{
		RamerDouglasPeucker(start, index, epsilon);
		RamerDouglasPeucker(index, end, epsilon);
	} else {
		start->nextPt = end;
	}
}

void CCampathDrawer::Reset()
{
	UnloadVertexBuffer();
}

double CCampathDrawer::ShortestDistanceToSegment(TempPoint * pt, TempPoint * start, TempPoint * end)
{
	double ESx = end->y.X - start->y.X;
	double ESy = end->y.Y - start->y.Y;
	double ESz = end->y.Z - start->y.Z;
	double dESdES = ESx*ESx + ESy*ESy + ESz*ESz;
	double t = dESdES ? (
		(pt->y.X-start->y.X)*ESx +(pt->y.Y -start->y.Y)*ESy + (pt->y.Z -start->y.Z)*ESz
	) / dESdES : 0.0;

	if(t <= 0.0)
		return (start->y -pt->y).Length();
	else
	if(1.0 <= t)
		return (pt->y -end->y).Length();

	return (pt->y -(start->y +t*(end->y -start->y))).Length();
}