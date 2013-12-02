#pragma once

// Copyright (c) advancedfx.org
//
// Last changes:
// 2013-08-30 dominik.matrixstorm.com
//
// First changes:
// 2007-09-13T13:37Z dominik.matrixstorm.com

#include <shared/vcpp/AfxAddr.h>


typedef AfxAddr HlAddress_t;
#define HL_ADDR_GET(name) AFXADDR_GET(name)
#define HL_ADDR_SET(name,value) AFXADDR_SET(name,value)


AFXADDR_DECL(CL_EmitEntities)
AFXADDR_DECL(CL_EmitEntities_DSZ)
AFXADDR_DECL(CL_ParseServerMessage_CmdRead)
AFXADDR_DECL(CL_ParseServerMessage_CmdRead_DSZ)
AFXADDR_DECL(ClientFunctionTable)
AFXADDR_DECL(CmdTools_Ofs1)
AFXADDR_DECL(CmdTools_Ofs2)
AFXADDR_DECL(CmdTools_Ofs3)
AFXADDR_DECL(DTOURSZ_GetSoundtime)
AFXADDR_DECL(DTOURSZ_Mod_LeafPVS)
AFXADDR_DECL(DTOURSZ_R_DrawEntitiesOnList)
AFXADDR_DECL(DTOURSZ_R_DrawParticles)
AFXADDR_DECL(DTOURSZ_R_DrawViewModel)
AFXADDR_DECL(DTOURSZ_R_PolyBlend)
AFXADDR_DECL(DTOURSZ_R_RenderView)
AFXADDR_DECL(DTOURSZ_SND_PickChannel)
AFXADDR_DECL(DTOURSZ_S_PaintChannels)
AFXADDR_DECL(DTOURSZ_S_TransferPaintBuffer)
AFXADDR_DECL(GetSoundtime)
AFXADDR_DECL(Host_Frame)
AFXADDR_DECL(Host_Frame_DSZ)
AFXADDR_DECL(Mod_LeafPVS)
AFXADDR_DECL(R_DrawEntitiesOnList)
AFXADDR_DECL(R_DrawParticles)
AFXADDR_DECL(R_DrawSkyBoxEx)
AFXADDR_DECL(R_DrawSkyBoxEx_DSZ)
AFXADDR_DECL(R_DrawViewModel)
AFXADDR_DECL(R_PolyBlend)
AFXADDR_DECL(R_PushDlights)
AFXADDR_DECL(R_RenderView)
AFXADDR_DECL(SND_PickChannel)
AFXADDR_DECL(S_PaintChannels)
AFXADDR_DECL(S_TransferPaintBuffer)
AFXADDR_DECL(UnkDrawHudIn)
AFXADDR_DECL(UnkDrawHudInCall)
AFXADDR_DECL(UnkDrawHudInContinue)
AFXADDR_DECL(UnkDrawHudOut)
AFXADDR_DECL(UnkDrawHudOutCall)
AFXADDR_DECL(UnkDrawHudOutContinue)
AFXADDR_DECL(UnkGetDecalTexture)
AFXADDR_DECL(UnkGetDecalTexture_DSZ)
AFXADDR_DECL(clientDll)
AFXADDR_DECL(cstrike_CHudDeathNotice_Draw)
AFXADDR_DECL(cstrike_CHudDeathNotice_Draw_DSZ)
AFXADDR_DECL(cstrike_CHudDeathNotice_MsgFunc_DeathMsg)
AFXADDR_DECL(cstrike_CHudDeathNotice_MsgFunc_DeathMsg_DSZ)
AFXADDR_DECL(cstrike_EV_CreateSmoke)
AFXADDR_DECL(cstrike_EV_CreateSmoke_DSZ)
AFXADDR_DECL(cstrike_MsgFunc_DeathMsg)
AFXADDR_DECL(cstrike_MsgFunc_DeathMsg_DSZ)
AFXADDR_DECL(cstrike_UnkCrosshairFn)
AFXADDR_DECL(cstrike_UnkCrosshairFn_DSZ)
AFXADDR_DECL(cstrike_UnkCrosshairFn_add_fac)
AFXADDR_DECL(cstrike_UnkCrosshairFn_mul_fac)
AFXADDR_DECL(cstrike_rgDeathNoticeList)
AFXADDR_DECL(hlExe)
AFXADDR_DECL(hwDll)
AFXADDR_DECL(msg_readcount)
AFXADDR_DECL(net_message)
AFXADDR_DECL(p_cl_enginefuncs_s)
AFXADDR_DECL(p_engine_studio_api_s)
AFXADDR_DECL(p_playermove_s)
AFXADDR_DECL(paintbuffer)
AFXADDR_DECL(paintedtime)
AFXADDR_DECL(r_refdef)
AFXADDR_DECL(shm)
AFXADDR_DECL(skytextures)
AFXADDR_DECL(soundtime)

void Addresses_InitHlExe(AfxAddr hlExe);
void Addresses_InitHwDll(AfxAddr hwDll);
void Addresses_InitClientDll(AfxAddr clientDll);