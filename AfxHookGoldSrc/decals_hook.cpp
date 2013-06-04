#include "stdafx.h"

#include <hlsdk.h>

#include "cmdregister.h"
#include "hl_addresses.h"

#include <shared/detours.h>
#include <shared/StringTools.h>

#include <list>
#include <string>

extern cl_enginefuncs_s *pEngfuncs;

//

struct decal_filter_entry_s {
	char * sz_Mask;
	int i_TargetDecalIndex;
};

struct g_decals_hook_s
{
	std::list<decal_filter_entry_s *> filters;
	bool b_debugprint;
	bool b_ListActive;
} g_decals_hook;

bool g_decals_hook_installed = false;

bool IsFilterEntryMatched( decal_filter_entry_s *pentry, const char * sz_Target )
{
	const char * sz_Mask = pentry->sz_Mask;

	std::list<std::string> maskWords;
	bool leadingWildCard = false;
	bool trailingWildCard = false;

	{
		bool firstMatch = true;
		bool lastWasWildCard = false;
		std::string curWord;
		bool hasWord = false;

		for(const char * curMask = sz_Mask; *curMask; curMask++)
		{
			if('\\' == *curMask)
			{
				curMask++;

				if('*' == *curMask)
				{
					if(firstMatch)
					{
						firstMatch = false;
						leadingWildCard = true;
					}

					if(hasWord)
					{
						maskWords.push_back(curWord);
						hasWord = false;
					}

					lastWasWildCard = true;

					continue;
				}
			}

			firstMatch = false;
			lastWasWildCard = false;
			if(!hasWord) curWord.assign("");
			hasWord = true;
			curWord.push_back(*curMask);
		}

		if(hasWord) maskWords.push_back(curWord);
		trailingWildCard = lastWasWildCard;
	}

	if(0 == maskWords.size())
		return 0 == strlen(sz_Target) || leadingWildCard && trailingWildCard;

	int idx = 0;

	for(std::list<std::string>::iterator it = maskWords.begin(); it != maskWords.end(); it++)
	{
		const char * matchPos = strstr(sz_Target, it->c_str());
		
		if(!matchPos)
			return false;

		if(0 == idx && !leadingWildCard && 0 < matchPos - sz_Target)
			return false;

		if(idx + 1 == maskWords.size() && !trailingWildCard)
		{
			return StringEndsWith(sz_Target, it->c_str());
		}

		idx++;
		sz_Target = matchPos +strlen(it->c_str()); // words don't overlap
	}

	return true;
}

REGISTER_DEBUGCMD_FUNC(test_filtermask)
{
	if(pEngfuncs->Cmd_Argc() != 1+2)
		return;

	decal_filter_entry_s entry;
	entry.sz_Mask = pEngfuncs->Cmd_Argv(1);

	bool bRes = IsFilterEntryMatched(&entry,pEngfuncs->Cmd_Argv(2));

	pEngfuncs->Con_Printf("%s\n", bRes ? "true" : "false" );
}

//
// decal filtering hook:

typedef texture_t * (* UnkGetDecalTexture_t)( int decaltexture );
UnkGetDecalTexture_t detoured_UnkGetDecalTexture = NULL;

// DecalTexture hook:
//   this function is called in a unknown sub function of R_DrawWorld that is
//   called before R_BlendLightmaps. the unknown functions draws out all
//   decals of the map as it seems or s.th. and uses this one to get
//   a decal's texture

texture_t * touring_UnkGetDecalTexture( int decaltexture )
{
	texture_t *tex;
	tex = detoured_UnkGetDecalTexture( decaltexture );

	if( tex )
	{
		const char * texname = tex->name;
		if( g_decals_hook.b_debugprint ) pEngfuncs->Con_Printf("UnkGetDecalTexture( %u ) = 0x%08x, name = %s\n",decaltexture,(DWORD)tex,texname);

		if(  g_decals_hook.b_ListActive )
		{

			std::list<decal_filter_entry_s *>::iterator iterend = g_decals_hook.filters.end();
			for (std::list<decal_filter_entry_s *>::iterator iter = g_decals_hook.filters.begin(); iter != iterend; iter++)
			{
				decal_filter_entry_s *entry = *iter;
				if( IsFilterEntryMatched( entry, texname) )
				{
					// matched, replace the texture( pointer) with the new one:
					tex = detoured_UnkGetDecalTexture( entry->i_TargetDecalIndex );
					break; // first filter in the list always wins
				}
			}

		}

	}
	else if( g_decals_hook.b_debugprint ) pEngfuncs->Con_Printf("UnkGetDecalTexture( %u ) = NULL\n",decaltexture);

	return tex;
}

bool InstallHook_UnkGetDecalTexture()
{
	if (!detoured_UnkGetDecalTexture && (HL_ADDR_GET(UnkGetDecalTexture)!=NULL) && !g_decals_hook_installed)
	{
		g_decals_hook_installed = true; // only excute this code once

		g_decals_hook.b_debugprint = false;
		g_decals_hook.b_ListActive = false;

		detoured_UnkGetDecalTexture = (UnkGetDecalTexture_t) DetourApply((BYTE *)HL_ADDR_GET(UnkGetDecalTexture), (BYTE *)touring_UnkGetDecalTexture, (int)HL_ADDR_GET(UnkGetDecalTexture_DSZ));
	}

	return detoured_UnkGetDecalTexture != NULL;
}

//
// list handling functions

decal_filter_entry_s * GetFilterEntryByMask( std::list<decal_filter_entry_s *> *pfilters, char *  sz_Mask )
{
	decal_filter_entry_s * ret = NULL;

	std::list<decal_filter_entry_s *>::iterator iterend = pfilters->end();
	for (std::list<decal_filter_entry_s *>::iterator iter = pfilters->begin(); iter != iterend; iter++)
	{
		if( !strcmp( (*iter)->sz_Mask, sz_Mask ) )
		{
			ret = *iter;
			break;
		}
	}

	return ret;
}

void RemoveFilterEntry( std::list<decal_filter_entry_s *> *pfilters, decal_filter_entry_s *pentry )
{
	pfilters->remove(pentry);
	free(pentry->sz_Mask);
	free(pentry);

	g_decals_hook.b_ListActive = !(pfilters->empty());
}

void CreateFilterEntry( std::list<decal_filter_entry_s *> *pfilters, const char * sz_Mask, const int i_TargetDecalIndex )
{
	decal_filter_entry_s *entry = (decal_filter_entry_s *)malloc(sizeof(decal_filter_entry_s));

	entry->sz_Mask = (char *)malloc(sizeof(char)*(strlen(sz_Mask)+1));
	strcpy(entry->sz_Mask,sz_Mask);
	entry->i_TargetDecalIndex = i_TargetDecalIndex;

	pfilters->push_back(entry);

	g_decals_hook.b_ListActive = true;
}

//
// advanced decalfilter functions for the user

void DecalFilter_Add(char * sz_Mask, char * sz_Decalname)
{
	if(!pEngfuncs->GetEntityByIndex(0))
	{
		pEngfuncs->Con_Printf("Error: Game must be running in order for this feature to work.\n");
		return;
	}

	// get's the decal index and also causes the game to cache the texture:
	// (if sz_Decalname is not a valid decalname, the game also supplies a default decal)
	int i_decalindex = pEngfuncs->pEfxAPI->Draw_DecalIndex(
			pEngfuncs->pEfxAPI->Draw_DecalIndexFromName( sz_Decalname )
	);

	// check if there is already an entry:
	decal_filter_entry_s *entry = GetFilterEntryByMask( &(g_decals_hook.filters), sz_Mask );
	if( entry )
	{
		// replace old entry:
		entry->i_TargetDecalIndex = i_decalindex;
	} else {
		// create new entry:
		CreateFilterEntry( &(g_decals_hook.filters), sz_Mask, i_decalindex );
	}
}

void DecalFilter_Remove(char * sz_Mask)
{
	decal_filter_entry_s *entry = GetFilterEntryByMask( &(g_decals_hook.filters), sz_Mask );
	if( entry )
	{
		RemoveFilterEntry(&(g_decals_hook.filters), entry);
	} else pEngfuncs->Con_Printf("Mask entry not in list.\n");
}

void DecalFilter_Debug(bool bDebug)
{
	g_decals_hook.b_debugprint = bDebug;
}

void DecalFilter_List()
{
	std::list<decal_filter_entry_s *>::iterator iterend = g_decals_hook.filters.end();
	for (std::list<decal_filter_entry_s *>::iterator iter = g_decals_hook.filters.begin(); iter != iterend; iter++)
	{
		decal_filter_entry_s *entry = *iter;
		pEngfuncs->Con_Printf("%s -> %u\n",entry->sz_Mask,entry->i_TargetDecalIndex);
	}
	pEngfuncs->Con_Printf("-- end of list --\n");
}

void DecalFilter_Clear()
{
	while( ! g_decals_hook.filters.empty() )
		RemoveFilterEntry( &(g_decals_hook.filters), g_decals_hook.filters.front() );
}

void DecalFilter_PrintHelp()
{
	pEngfuncs->Con_Printf(
		"" PREFIX "decalfilter <command>\n"
		"where <command> is one of these:\n"
		"\t" "add <mask> <decalname> - forces all decaltextures to use the texture of decal <decalname> from decals.wad instead\n"
		"\t" "remove <mask> - removes a filtering mask again\n"
		"\t" "list - lists maks currently active\n"
		"\t" "debug 0|1 - prints incoming decal texture requests from the game to the console\n"
		"\t" "clearall - removes all filters\n"
		"<mask> - string to match, where \\* = wildcard and \\\\ = \\\n"
		"The first filter in the list always wins. For more information  HLAE online manual.\n"
	);
}

REGISTER_CMD_FUNC(decalfilter)
{
	if(!InstallHook_UnkGetDecalTexture())
	{
		pEngfuncs->Con_Printf("Error: Failed to install hook.\n");
		return;
	}

	bool bShowHelp = true;

	int argc = pEngfuncs->Cmd_Argc();

	if( argc >= 1+1 )
	{
		char * command = pEngfuncs->Cmd_Argv(1);
		if(!stricmp(command,"clearall"))
		{
			DecalFilter_Clear();
			bShowHelp = false;
		}
		else if(!stricmp(command,"list"))
		{
			DecalFilter_List();
			bShowHelp = false;
		}
		else if(!stricmp(command,"debug") && argc == 1+2)
		{
			DecalFilter_Debug(0 != atoi(pEngfuncs->Cmd_Argv(2)));
			bShowHelp = false;
		}
		else if(!stricmp(command,"remove") && argc == 1+2)
		{
			DecalFilter_Remove( pEngfuncs->Cmd_Argv(2) );
			bShowHelp = false;
		}
		else if(!stricmp(command,"add") && argc == 1+3)
		{
			DecalFilter_Add(  pEngfuncs->Cmd_Argv(2),  pEngfuncs->Cmd_Argv(3) );
			bShowHelp = false;
		}

	}

	if( bShowHelp ) DecalFilter_PrintHelp();
}


//
// simplified noadverts command for the user

REGISTER_DEBUGCVAR(noadverts_newdecal, "{littleman", 0);
REGISTER_DEBUGCVAR(noadverts_mask, "{^IGA_\\*", 0);

REGISTER_CMD_FUNC(noadverts)
{
	if(!InstallHook_UnkGetDecalTexture())
	{
		pEngfuncs->Con_Printf("Error: Failed to install hook.\n");
		return;
	}

	if( pEngfuncs->Cmd_Argc() != 2)
	{
		pEngfuncs->Con_Printf("" PREFIX "noadverts 0|1 - disable / enable removal\n\n");
		return;
	}

	bool bNoAds = 0 != atoi(pEngfuncs->Cmd_Argv(1));

	decal_filter_entry_s *entry = GetFilterEntryByMask( &(g_decals_hook.filters), noadverts_mask->string );
	if( entry )
	{
		RemoveFilterEntry(&(g_decals_hook.filters), entry);
	}
	if( bNoAds )
	{
		if(!pEngfuncs->GetEntityByIndex(0))
		{
			pEngfuncs->Con_Printf("Error: Game must be running in order for this feature to work.\n");
			return;
		}

		CreateFilterEntry(
			&(g_decals_hook.filters),
			noadverts_mask->string,
			pEngfuncs->pEfxAPI->Draw_DecalIndex(
				pEngfuncs->pEfxAPI->Draw_DecalIndexFromName( noadverts_newdecal->string )
			)
		);
	}
}