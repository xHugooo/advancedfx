#pragma once

#include <shared/bvhimport.h>

class CCamImport
{
public:
	CCamImport();
	~CCamImport();

	bool IsActive();

	bool LoadMotionFile(wchar_t const * fileName);
	void CloseMotionFile();

	// outformat: see BvhImport::BvhChannel_t
	// return: truew if successful
	bool GetCamPositon(float fTimeOfs, float outCamdata[6]);

	float GetBaseTime();

	void SetBaseTime(float fBaseTime);

private:
	float m_BaseTime;
	BvhImport m_BvhImport;
};

extern CCamImport g_CamImport;
