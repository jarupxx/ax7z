/*
ax7z entry funcs
*/

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <commctrl.h>
#include <shlobj.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <set>
#include "entryFuncs.h"
#include "infcache.h"
#include <sstream>
#include "resource.h"
#include "7z/7zip/Bundles/ax7z/PasswordManager.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "7z/7zip/Bundles/ax7z/SolidCache.h"
#include "version.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include "release.h"
#include "7z/Windows/FileDir.h"

struct NoCaseLess
{
	bool operator()(const std::string &s1, const std::string &s2) const
	{
		return _stricmp(s1.c_str(), s2.c_str()) < 0;
	}
};

//拡張子管理クラス
class ExtManager
{
private:
	typedef std::string Ext;
	struct Info
	{
		std::vector<std::string> methods;
		bool enable;
	};
	std::map<Ext, Info, NoCaseLess> m_mTable;
	std::set<Ext, NoCaseLess> m_sTableUser;
public:
	static const char* const SECTION_NAME;
	static const char* const DEFAULT_EXTENSIONS;
	ExtManager() {}
	typedef std::vector<std::pair<std::string, std::string> > Conf;
	void Init(const Conf& conf);
	bool IsEnable(LPSTR filename) const;
	void SetEnable(const std::string &sExt, bool fEnable)
	{
		m_mTable[sExt].enable = fEnable;
	}
	typedef std::map<Ext, Info, NoCaseLess>::value_type EachValueType;
	template<typename Functor>
	void Each(Functor f) const
	{
		std::for_each(m_mTable.begin(), m_mTable.end(), f);
	}
	void SetDefault();
	void Save(const std::string &sIniFileName) const;
	void Load(const std::string &sIniFileName);
	void SetPluginInfo(std::vector<std::string> &info) const;
	void AddUserExt(const std::string& sExt);
	void RemoveUserExt(const std::string& sExt);
	void SetUserExtensions(const std::string& sExts);
	std::string GetUserExtensions() const;
};

const char* const ExtManager::SECTION_NAME = "ax7z_s";
const char* const ExtManager::DEFAULT_EXTENSIONS = "bin;img;mdf";

void ExtManager::Init(const Conf& conf)
{
	Conf::const_iterator ciEnd = conf.end();
	for(Conf::const_iterator ci = conf.begin(); ci != ciEnd; ++ci) {
		m_mTable[ci->first].enable = true;
		m_mTable[ci->first].methods.push_back(ci->second);
	}
}

bool ExtManager::IsEnable(LPSTR filename) const
{
	char path_buffer[_MAX_PATH * 2];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR * 2];
	char fname[_MAX_FNAME * 2];
	char buf[_MAX_EXT * 2];
	errno_t err;
	strcpy_s(path_buffer, _MAX_PATH * 2, filename);
	err = _splitpath_s(path_buffer, drive, _MAX_DRIVE, dir, _MAX_DIR * 2, fname, _MAX_FNAME * 2, buf, _MAX_EXT * 2);
	if(buf[0] == 0) buf[1] = 0; // guard
	std::string sBuf(buf+1); // skip period
	std::map<Ext, Info, NoCaseLess>::const_iterator ci = m_mTable.find(sBuf);
	if(ci != m_mTable.end()) {
		return ci->second.enable;
	}
	std::set<Ext, NoCaseLess>::const_iterator ciUser = m_sTableUser.find(sBuf);
	if(ciUser != m_sTableUser.end()) {
		return true;
	}
	return false;
}

void ExtManager::SetDefault()
{
	std::map<Ext, Info, NoCaseLess>::iterator it, itEnd = m_mTable.end();
	for(it = m_mTable.begin(); it != itEnd; ++it) {
		it->second.enable = true;
	}
}

void ExtManager::Save(const std::string &sIniFileName) const
{
	if(m_sTableUser.size()) {
		const std::string &sResult = GetUserExtensions();
		WritePrivateProfileString(SECTION_NAME, "user_extensions", sResult.c_str(), sIniFileName.c_str());
	} else {
		WritePrivateProfileString(SECTION_NAME, "user_extensions", NULL, sIniFileName.c_str());
	}

	std::map<Ext, Info, NoCaseLess>::const_iterator ci, ciEnd = m_mTable.end();
	for(ci = m_mTable.begin(); ci != ciEnd; ++ci) {
		WritePrivateProfileString(SECTION_NAME, ci->first.c_str(), ci->second.enable ? "1" : "0", sIniFileName.c_str());
	}
}

void ExtManager::Load(const std::string &sIniFileName)
{
	std::vector<char> vBuf(1024);

	DWORD dwSize;
	do {
		vBuf.resize(vBuf.size() * 2);
		dwSize = GetPrivateProfileString(SECTION_NAME, "user_extensions", DEFAULT_EXTENSIONS, &vBuf[0], vBuf.size(), sIniFileName.c_str());
	} while(dwSize == vBuf.size() - 1);
	std::string sResult(&vBuf[0]);
	SetUserExtensions(sResult);

	std::map<Ext, Info, NoCaseLess>::iterator mi, miEnd = m_mTable.end();
	for(mi = m_mTable.begin(); mi != miEnd; ++mi) {
		mi->second.enable = GetPrivateProfileInt(SECTION_NAME, mi->first.c_str(), 1, sIniFileName.c_str()) != 0;
	}
}

struct MyConcat2
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "")
			return "*." + s2;
		return s1 + ";" + "*." + s2;
	}
};

void ExtManager::SetPluginInfo(std::vector<std::string> &vsPluginInfo) const
{
	std::map<std::string, std::string> mResmap;
	std::map<Ext, Info, NoCaseLess>::const_iterator ci, ciEnd = m_mTable.end();	
	for(ci = m_mTable.begin(); ci != ciEnd; ++ci) {
		if(!ci->second.enable) continue;
		std::vector<std::string>::const_iterator ci2, ciEnd2 = ci->second.methods.end();
		for(ci2 = ci->second.methods.begin(); ci2 != ciEnd2; ++ci2) {
			if(!mResmap[*ci2].empty()) {
				mResmap[*ci2] += ";";
			}
			mResmap[*ci2] += "*.";
			mResmap[*ci2] += ci->first;
		}
	}

	vsPluginInfo.clear();
	vsPluginInfo.push_back("00AM");
	vsPluginInfo.push_back("7z extract library v0.7 for 7-zip 4.57+ " RELEASE_STR " (C) Makito Miyano / enhanced by Yak!"); 

	std::map<std::string, std::string>::const_iterator ci2, ciEnd2 = mResmap.end();
	for(ci2 = mResmap.begin(); ci2 != ciEnd2; ++ci2) {
		vsPluginInfo.push_back(ci2->second);
		vsPluginInfo.push_back(ci2->first + " files");
	}
	if(m_sTableUser.size()) {
		vsPluginInfo.push_back(std::accumulate(m_sTableUser.begin(), m_sTableUser.end(), std::string(), MyConcat2()));
		vsPluginInfo.push_back("User-defined");
	}
}

void ExtManager::AddUserExt(const std::string& sExt)
{
	m_sTableUser.insert(sExt);
}

void ExtManager::RemoveUserExt(const std::string& sExt)
{
	m_sTableUser.erase(sExt);
}

struct MyConcat
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "") return s2;
		return s1 + ";" + s2;
	}
};

std::string ExtManager::GetUserExtensions() const
{
	return std::accumulate(m_sTableUser.begin(), m_sTableUser.end(), std::string(), MyConcat());
}

void ExtManager::SetUserExtensions(const std::string& sExts)
{
	boost::algorithm::split(m_sTableUser, sExts, boost::algorithm::is_any_of(";"), boost::algorithm::token_compress_on);
	m_sTableUser.erase("");
}

//グローバル変数
static InfoCache infocache; //アーカイブ情報キャッシュクラス
static InfoCacheW infocacheW; //アーカイブ情報キャッシュクラス
static std::string g_sIniFileName; // ini ファイル名
static ExtManager g_extManager;
static bool g_fSharedConf;
HINSTANCE g_hInstance;
int g_nSolidEnable7z = 1;
int g_nSolidEnableRar = 1;

#ifndef _UNICODE
bool g_IsNT = false;
static inline bool IsItWindowsNT()
{
	OSVERSIONINFO versionInfo;
	versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
	if (!::GetVersionEx(&versionInfo)) 
		return false;
	return (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
}
#endif

void SetParamDefault()
{
	g_extManager.SetDefault();
	g_extManager.SetUserExtensions(ExtManager::DEFAULT_EXTENSIONS);
	g_nSolidEnable7z = 1;
	g_nSolidEnableRar = 1;

	SolidCache& sc = SolidCache::GetInstance();

	sc.SetMaxLookAhead(-1);
	sc.SetMaxMemory(100);
	sc.SetMaxDisk(1024);
	sc.SetPurgeMemory(10);
	sc.SetPurgeDisk(10);
//	char buf[2048];
//	GetTempPath(sizeof(buf), buf);
//	sc.SetCacheFolder(buf);

	g_fSharedConf = false;
}

std::string GetIniFileName()
{
	return g_sIniFileName;
}

void LoadFromIni()
{
	SetParamDefault();

	std::string sIniFileName = GetIniFileName();

	g_extManager.Load(sIniFileName);

	SolidCache& sc = SolidCache::GetInstance();

	g_nSolidEnable7z = GetPrivateProfileInt(ExtManager::SECTION_NAME, "solid7z", g_nSolidEnable7z, sIniFileName.c_str());
	g_nSolidEnableRar = GetPrivateProfileInt(ExtManager::SECTION_NAME, "solidrar", g_nSolidEnableRar, sIniFileName.c_str());
	sc.SetMaxLookAhead(GetPrivateProfileInt(ExtManager::SECTION_NAME, "lookahead", sc.GetMaxLookAhead(), sIniFileName.c_str()));
	sc.SetMaxMemory(GetPrivateProfileInt(ExtManager::SECTION_NAME, "memory", sc.GetMaxMemory(), sIniFileName.c_str()));
	sc.SetMaxDisk(GetPrivateProfileInt(ExtManager::SECTION_NAME, "disk", sc.GetMaxDisk(), sIniFileName.c_str()));
	sc.SetPurgeMemory(GetPrivateProfileInt(ExtManager::SECTION_NAME, "purge_memory", sc.GetPurgeMemory(), sIniFileName.c_str()));
	sc.SetPurgeDisk(GetPrivateProfileInt(ExtManager::SECTION_NAME, "purge_disk", sc.GetPurgeDisk(), sIniFileName.c_str()));
	char buf[2048], buf2[2048];
	GetTempPath(sizeof(buf2), buf2);
	GetPrivateProfileString(ExtManager::SECTION_NAME, "folder", buf2, buf, sizeof(buf), sIniFileName.c_str());
	sc.SetCacheFolder(buf);

	std::vector<char> vBuf(1024);

	DWORD dwSize;
	do {
		vBuf.resize(vBuf.size() * 2);
		dwSize = GetPrivateProfileString(ExtManager::SECTION_NAME, "topmost", "false", &vBuf[0], vBuf.size(), sIniFileName.c_str());
	} while(dwSize == vBuf.size() - 1);
	std::string sResult(&vBuf[0]);
	PasswordManager::Get().SetTopMost(sResult == "true");
	int nShared = GetPrivateProfileInt(ExtManager::SECTION_NAME, "shared", -1, sIniFileName.c_str());
	if(nShared != -1) g_fSharedConf = nShared;
}

bool SaveToIni()
{
	std::string sIniFileName = GetIniFileName();

	g_extManager.Save(sIniFileName);

	char buf[2048];
	WritePrivateProfileString(ExtManager::SECTION_NAME, "solid7z", g_nSolidEnable7z ? "1" : "0", sIniFileName.c_str());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "solidrar", g_nSolidEnableRar ? "1" : "0", sIniFileName.c_str());

	SolidCache& sc = SolidCache::GetInstance();
	wsprintf(buf, "%d", sc.GetMaxLookAhead());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "lookahead", buf, sIniFileName.c_str());
	wsprintf(buf, "%d", sc.GetMaxMemory());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "memory", buf, sIniFileName.c_str());
	wsprintf(buf, "%d", sc.GetMaxDisk());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "disk", buf, sIniFileName.c_str());
	wsprintf(buf, "%d", sc.GetPurgeMemory());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "purge_memory", buf, sIniFileName.c_str());
	wsprintf(buf, "%d", sc.GetPurgeDisk());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "purge_disk", buf, sIniFileName.c_str());
	WritePrivateProfileString(ExtManager::SECTION_NAME, "folder", sc.GetCacheFolder().c_str(), sIniFileName.c_str());

	WritePrivateProfileString(ExtManager::SECTION_NAME, "topmost", PasswordManager::Get().GetTopMost() ? "true" : "false", sIniFileName.c_str());

	int nResult = 1;
	nResult &= WritePrivateProfileString(ExtManager::SECTION_NAME, "topmost", PasswordManager::Get().GetTopMost() ? "true" : "false", sIniFileName.c_str());
	if(g_fSharedConf)
		nResult &= WritePrivateProfileString(ExtManager::SECTION_NAME, "shared", "1", sIniFileName.c_str());

	return nResult;
}

std::string GetSharedIniFileName(HANDLE hModule)
{
	std::vector<char> vModulePath(1024);
	size_t nLen = GetModuleFileName((HMODULE)hModule, &vModulePath[0], (DWORD)vModulePath.size());
	vModulePath.resize(nLen + 1);

	std::string sResult(&vModulePath[0]);
	sResult += ".ini";
	return sResult;
}

void MakePersonalIniFolder()
{
	char p[MAX_PATH];
	SHGetFolderPath(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, p);
	std::string s(p);
	s += "\\ax7z";
	CreateDirectory(s.c_str(), 0);
}

std::string GetPersonalIniFileName(HANDLE hModule)
{
	char p[MAX_PATH];
	SHGetFolderPath(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, p);
	std::string s(p);
	s += "\\ax7z";
	std::vector<char> vModulePath(1024);
	size_t nLen = GetModuleFileName((HMODULE)hModule, &vModulePath[0], (DWORD)vModulePath.size());
	vModulePath.resize(nLen + 1);
	std::vector<char>::reverse_iterator it = find(vModulePath.rbegin(), vModulePath.rend(), '\\');
	s += "\\";
	s += std::string(&*it.base());
	s += ".ini";
	return s;
}

void SetIniFileName(HANDLE hModule)
{
	std::string sPersonal(GetPersonalIniFileName(hModule));
	struct __stat64 st;
	if(!_stat64(sPersonal.c_str(), &st)) { // If personal ini file found, always used
		g_sIniFileName = sPersonal;
		g_fSharedConf = false;
		return;
	}
	std::string sShared(GetSharedIniFileName(hModule));
	if(!_stat64(sShared.c_str(), &st)) { // Found
		int nShared = GetPrivateProfileInt(ExtManager::SECTION_NAME, "shared", -1, sShared.c_str());
		if(nShared == 1) {
			g_sIniFileName = sShared;
			g_fSharedConf = true;
			return;
		}
		char buf[1024];
		wsprintf(buf,
			"The %s.spi.ini file is found where the %s.spi file exists.\n"
			"To support multiple users, the default place of the %s.spi.ini file is changed to your AppData folder.\n"
			"\n"
			"Do you want to move the %s.spi.ini file to your AppData folder?\n"
			"If you can not understand what it is, please select NO.",
			ExtManager::SECTION_NAME, ExtManager::SECTION_NAME, ExtManager::SECTION_NAME, ExtManager::SECTION_NAME);
		int nRet = MessageBox(NULL, buf,
			ExtManager::SECTION_NAME, MB_TASKMODAL | MB_ICONWARNING | MB_YESNO);
		if(nRet == IDNO) {
			WritePrivateProfileString(ExtManager::SECTION_NAME, "shared", "1", sShared.c_str());
			g_sIniFileName = sShared;
			g_fSharedConf = true;
			return;
		} else {
			MakePersonalIniFolder();
			if(!CopyFile(sShared.c_str(), sPersonal.c_str(), TRUE)) {
				MessageBox(NULL, "Copy of the ini file is failed. Use the old ini file.", ExtManager::SECTION_NAME, MB_TASKMODAL | MB_ICONWARNING | MB_OK);
				g_sIniFileName = sShared;
				g_fSharedConf = true;
				return;
			}
			if(!DeleteFile(sShared.c_str())) {
				MessageBox(NULL, "Delete of the old ini file is failed. Leave as it is.", ExtManager::SECTION_NAME, MB_TASKMODAL | MB_ICONWARNING | MB_OK);
			}
		}
	}
	g_sIniFileName = sPersonal;
	g_fSharedConf = false;
}

void handle_exception(const std::string &s)
{
	try {
		throw;
	} catch (std::exception &e) {
		OutputDebugPrintf("%s: exception %s thrown", s.c_str(), e.what());
	} catch (...) {
		OutputDebugPrintf("%s: unknown exception thrown", s.c_str());
	}
}

/* エントリポイント */
BOOL APIENTRY SpiEntryPoint(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	try {
		bool bInitPath = false;
		INITCOMMONCONTROLSEX ice = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
		switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
#ifndef _UNICODE
			g_IsNT = IsItWindowsNT();
#endif
			InitCommonControlsEx(&ice);
			{
				extern void GetFormats(ExtManager::Conf &res);
				ExtManager::Conf v;
				GetFormats(v);
				g_extManager.Init(v);
			}
			SetIniFileName(hModule);
			LoadFromIni();
			bInitPath = true;
			break;
		case DLL_THREAD_ATTACH:
			SetIniFileName(hModule);
			LoadFromIni();
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
		}
	} catch (...) {
		handle_exception(__FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
/* エントリポイント */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	int a = sizeof(fileInfoW);
	switch (ul_reason_for_call) {
		case DLL_PROCESS_DETACH:
			infocache.Clear();
			infocacheW.Clear();
			break;
	}
	g_hInstance = (HINSTANCE)hModule;
	return SpiEntryPoint(hModule, ul_reason_for_call, lpReserved);
}

/***************************************************************************
 * SPI関数
 ***************************************************************************/
//---------------------------------------------------------------------------
int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	try {
		std::vector<std::string> vsPluginInfo;

		g_extManager.SetPluginInfo(vsPluginInfo);

		if (infono < 0 || infono >= (int)vsPluginInfo.size()) {
			return 0;
		}

		lstrcpyn(buf, vsPluginInfo[infono].c_str(), buflen);

		return lstrlen(buf);
	} catch (...) {
		handle_exception(__FUNCTION__);
		return 0;
	}
}

static bool CheckFileExtension(const char* pFileName, const char* pExtension)
{
	int nExtensionLen = strlen(pExtension);
	int nFileNameLen = strlen(pFileName);

	// ピリオドを入れてファイル名本体が存在するか?
	if (nFileNameLen <= nExtensionLen + 1) {
		return false;
	}

	return (_strnicmp(pFileName + nFileNameLen - nExtensionLen, pExtension, nExtensionLen) == 0);
}
static bool CheckFileExtensionW(const wchar_t* pFileName, const wchar_t* pExtension)
{
	int nExtensionLen = wcslen(pExtension);
	int nFileNameLen = wcslen(pFileName);

	// ピリオドを入れてファイル名本体が存在するか?
	if (nFileNameLen <= nExtensionLen + 1) {
		return false;
	}

	return (_wcsnicmp(pFileName + nFileNameLen - nExtensionLen, pExtension, nExtensionLen) == 0);
}

//---------------------------------------------------------------------------
int __stdcall IsSupported(LPSTR filename, DWORD dw)
{
	try {
		// 現時点では名前のみで判断
		return g_extManager.IsEnable(filename);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return 0;
	}
}

int __stdcall IsSupportedW(LPWSTR filename, DWORD dw)
{
	// Unicode 未サポート
	return 0;
}

//---------------------------------------------------------------------------
//アーカイブ情報をキャッシュする
int GetArchiveInfoCache(char *filename, long len, HLOCAL *phinfo, fileInfo *pinfo)
{
	int ret = infocache.Dupli(filename, phinfo, pinfo);
	if (ret != SPI_NO_FUNCTION) return ret;

	//キャッシュに無い
	HLOCAL hinfo;
	ret = GetArchiveInfoEx(filename, len, &hinfo);
	if (ret != SPI_ALL_RIGHT) return ret;

	//キャッシュ
	infocache.Add(filename, &hinfo);

	if (phinfo != NULL) {
		UINT size = LocalSize(hinfo);
		/* 出力用のメモリの割り当て */
		*phinfo = LocalAlloc(LMEM_FIXED, size);
		if (*phinfo == NULL) {
			return SPI_NO_MEMORY;
		}

		memcpy(*phinfo, (void*)hinfo, size);
	} else {
		fileInfo *ptmp = (fileInfo *)hinfo;
		if (pinfo->filename[0] != '\0') {
			for (;;) {
				if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
				// complete path relative to archive root
				char path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
				strcpy_s(path, ptmp->path);
				size_t len = strlen(path);
				if(len && path[len-1] != '/' && path[len-1] != '\\') // need delimiter
					strcat_s(path, "\\");
				strcat_s(path, ptmp->filename);
				if (lstrcmpi(path, pinfo->filename) == 0) break;
				ptmp++;
			}
		} else {
			for (;;) {
				if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
				if (ptmp->position == pinfo->position) break;
				ptmp++;
			}
		}
		*pinfo = *ptmp;
	}
	return SPI_ALL_RIGHT;
}
int GetArchiveInfoCacheW(wchar_t *filename, long len, HLOCAL *phinfo, fileInfoW *pinfo)
{
	int ret = infocacheW.Dupli(filename, phinfo, pinfo);
	if (ret != SPI_NO_FUNCTION) return ret;

	//キャッシュに無い
	HLOCAL hinfo;
	ret = GetArchiveInfoWEx(filename, len, &hinfo);
	if (ret != SPI_ALL_RIGHT) return ret;

	//キャッシュ
	infocacheW.Add(filename, &hinfo);

	if (phinfo != NULL) {
		UINT size = LocalSize(hinfo);
		/* 出力用のメモリの割り当て */
		*phinfo = LocalAlloc(LMEM_FIXED, size);
		if (*phinfo == NULL) {
			return SPI_NO_MEMORY;
		}

		memcpy(*phinfo, (void*)hinfo, size);
	} else {
		fileInfoW *ptmp = (fileInfoW *)hinfo;
		if (pinfo->filename[0] != L'\0') {
			for (;;) {
				if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
				// complete path relative to archive root
				wchar_t path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
				wcscpy_s(path, ptmp->path);
				size_t len = wcslen(path);
				if(len && path[len-1] != L'/' && path[len-1] != L'\\') // need delimiter
					wcscat_s(path, L"\\");
				wcscat_s(path, ptmp->filename);
				if (_wcsicmp(path, pinfo->filename) == 0) break;
				ptmp++;
			}
		} else {
			for (;;) {
				if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
				if (ptmp->position == pinfo->position) break;
				ptmp++;
			}
		}
		*pinfo = *ptmp;
	}
	return SPI_ALL_RIGHT;
}
//---------------------------------------------------------------------------
int __stdcall GetArchiveInfo(LPSTR buf, long len, unsigned int flag, HLOCAL *lphInf)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		*lphInf = NULL;
		return GetArchiveInfoCache(buf, len, lphInf, NULL);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}
int __stdcall GetArchiveInfoW(LPWSTR buf, long len, unsigned int flag, HLOCAL *lphInf)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		*lphInf = NULL;
		return GetArchiveInfoCacheW(buf, len, lphInf, NULL);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}

//---------------------------------------------------------------------------
int __stdcall GetFileInfo
(LPSTR buf, long len, LPSTR filename, unsigned int flag, struct fileInfo *lpInfo)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		lstrcpy(lpInfo->filename, filename);

		return GetArchiveInfoCache(buf, len, NULL, lpInfo);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}

int __stdcall GetFileInfoW
(LPWSTR buf, long len, LPWSTR filename, unsigned int flag, struct fileInfoW *lpInfo)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		wcscpy_s(lpInfo->filename, filename);

		return GetArchiveInfoCacheW(buf, len, NULL, lpInfo);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}
//---------------------------------------------------------------------------
int __stdcall GetFile(LPSTR src, long len,
			   LPSTR dest, unsigned int flag,
			   SPI_PROGRESS lpPrgressCallback, long lData)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		fileInfo info;
		info.filename[0] = '\0';
		info.position = len;
		int ret = GetArchiveInfoCache(src, 0, NULL, &info);
		if (ret != SPI_ALL_RIGHT) {
			return ret;
		}

		int nRet;
		if ((flag & 0x700) == 0) {
			//ファイルへの出力の場合
			std::string s = dest;
			s += "\\";
			s += info.filename;
			nRet = GetFileEx(src, NULL, s.c_str(), &info, lpPrgressCallback, lData);
		} else {
			// メモリへの出力の場合
			nRet = GetFileEx(src, (HLOCAL *)dest, NULL, &info, lpPrgressCallback, lData);
		}
		return nRet;
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}
int __stdcall GetFileW(LPWSTR src, long len,
			   LPWSTR dest, unsigned int flag,
			   SPI_PROGRESS lpPrgressCallback, long lData)
{
	try {
		//メモリ入力には対応しない
		if ((flag & 7) != 0) return SPI_NO_FUNCTION;

		fileInfoW info;
		info.filename[0] = L'\0';
		info.position = len;
		int ret = GetArchiveInfoCacheW(src, 0, NULL, &info);
		if (ret != SPI_ALL_RIGHT) {
			return ret;
		}

		int nRet;
		if ((flag & 0x700) == 0) {
			//ファイルへの出力の場合
			std::wstring s = dest;
			s += L"\\";
			s += info.filename;
			nRet = GetFileWEx(src, NULL, s.c_str(), &info, lpPrgressCallback, lData);
		} else {
			// メモリへの出力の場合
			nRet = GetFileWEx(src, (HLOCAL *)dest, NULL, &info, lpPrgressCallback, lData);
		}
		return nRet;
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}

static std::string Get7zPath(HKEY hkParent)
{
	HKEY hkey;
	if(RegOpenKey(hkParent, "Software\\7-zip", &hkey) == ERROR_SUCCESS) {
		std::vector<BYTE> buf(4096);
		DWORD dwSize = buf.size();
		if(RegQueryValueEx(hkey, "Path", 0, 0, &buf[0], &dwSize) == ERROR_SUCCESS) {
			buf.resize(dwSize ? dwSize - 1 : 0);
			return std::string(buf.begin(), buf.end());
		}
	}
	return std::string();
}

static HINSTANCE LoadLibrary7z()
{
	HINSTANCE h = LoadLibrary("7z.dll");
	if(h) return h;
	std::string s = Get7zPath(HKEY_CURRENT_USER) + "7z.dll";
	h = LoadLibrary(s.c_str());
	if(h) return h;
	s = Get7zPath(HKEY_LOCAL_MACHINE) + "7z.dll";
	h = LoadLibrary(s.c_str());
	return h;
}

static std::string Find7zPath()
{
	HINSTANCE h = LoadLibrary7z();
	if(!h) return std::string();
	std::string s(4096, '\0');
	DWORD dwLen = GetModuleFileName(h, &s[0], s.size());
	s.resize(dwLen);
	FreeLibrary(h);
	return s;
}
//---------------------------------------------------------------------------
LRESULT CALLBACK AboutDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	try {
		switch (msg) {
			case WM_INITDIALOG:
			{
OutputDebugPrintf("AboutDlgProc: 7z DLL path %s", Find7zPath().c_str());
				yak::util::windows::VersionResource vr(Find7zPath().c_str());
				std::string s = vr.GetValue(yak::util::windows::VersionResource::FILE_VERSION);
OutputDebugPrintf("AboutDlgProc: 7z DLL version %s", s.c_str());
				SendDlgItemMessage(hDlgWnd, IDC_7ZVERSION, WM_SETTEXT, 0, (LPARAM)s.c_str());
				return FALSE;
			}
			case WM_COMMAND:
				switch (LOWORD(wp)) {
					case IDOK:
						EndDialog(hDlgWnd, IDOK);
						break;
					case IDCANCEL:
						EndDialog(hDlgWnd, IDCANCEL);
						break;
					default:
						return FALSE;
				}
			default:
				return FALSE;
		}
		return TRUE;
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}

//---------------------------------------------------------------------------
static void UpdateSolidDialogItem(HWND hDlgWnd)
{
	SendDlgItemMessage(hDlgWnd, IDC_SOLID_7Z_CHECK, BM_SETCHECK, (WPARAM)g_nSolidEnable7z, 0L);
	SendDlgItemMessage(hDlgWnd, IDC_SOLID_RAR_CHECK, BM_SETCHECK, (WPARAM)g_nSolidEnableRar, 0L);

	SolidCache& sc = SolidCache::GetInstance();
	char buf[2048];
	wsprintf(buf, "%d", sc.GetMaxLookAhead());
	SendDlgItemMessage(hDlgWnd, IDC_MAX_LOOKAHEAD_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
	wsprintf(buf, "%d", sc.GetMaxMemory());
	SendDlgItemMessage(hDlgWnd, IDC_MAX_MEMORY_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
	wsprintf(buf, "%d", sc.GetMaxDisk());
	SendDlgItemMessage(hDlgWnd, IDC_MAX_DISK_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
	wsprintf(buf, "%d", sc.GetPurgeMemory());
	SendDlgItemMessage(hDlgWnd, IDC_PURGE_MEMORY_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
	wsprintf(buf, "%d", sc.GetPurgeDisk());
	SendDlgItemMessage(hDlgWnd, IDC_PURGE_DISK_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
	SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_SETTEXT, 0, (LPARAM)sc.GetCacheFolder().c_str());
}

static bool IsChecked2(HWND hDlgWnd, int nID, int &nVar)
{
	int nOldVar = nVar;
	if (IsDlgButtonChecked(hDlgWnd, nID) == BST_CHECKED) {
		nVar = 1;
	} else {
		nVar = 0;
	}
	return (nVar != nOldVar);
}

static bool GetIntValue(HWND hDlgWnd, int nID, int (SolidCache::*mfGetter)() const, int (SolidCache::*mfSetter)(int))
{
	char buf[2048];
	SendDlgItemMessage(hDlgWnd, nID, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
	int nTempVar = atoi(buf);
	if(nTempVar != (SolidCache::GetInstance().*mfGetter)()) {
		(SolidCache::GetInstance().*mfSetter)(nTempVar);
		return true;
	}
	return false;
}

static bool UpdateSolidValue(HWND hDlgWnd)
{
	bool fChanged = false;
	fChanged |= IsChecked2(hDlgWnd, IDC_SOLID_7Z_CHECK, g_nSolidEnable7z);
	fChanged |= IsChecked2(hDlgWnd, IDC_SOLID_RAR_CHECK, g_nSolidEnableRar);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_LOOKAHEAD_EDIT, &SolidCache::GetMaxLookAhead, &SolidCache::SetMaxLookAhead);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_MEMORY_EDIT, &SolidCache::GetMaxMemory, &SolidCache::SetMaxMemory);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_DISK_EDIT, &SolidCache::GetMaxDisk, &SolidCache::SetMaxDisk);
	fChanged |= GetIntValue(hDlgWnd, IDC_PURGE_MEMORY_EDIT, &SolidCache::GetPurgeMemory, &SolidCache::SetPurgeMemory);
	fChanged |= GetIntValue(hDlgWnd, IDC_PURGE_DISK_EDIT, &SolidCache::GetPurgeDisk, &SolidCache::SetPurgeDisk);
	char buf[2048];
	SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
	if(lstrcmp(buf, SolidCache::GetInstance().GetCacheFolder().c_str())) {
		fChanged = true;
		SolidCache::GetInstance().SetCacheFolder(buf);
	}

	return fChanged;
}

static bool IsExistentFolder(HWND hwnd, const std::string &sPath)
{
	DWORD dwAttr = GetFileAttributes(sPath.c_str());
	if(dwAttr == -1) {
		std::string sText = "Directory: " + sPath + " is not found, create?";
		if(MessageBox(hwnd, sText.c_str(), ExtManager::SECTION_NAME, MB_YESNO) == IDYES) {
			if(NWindows::NFile::NDirectory::CreateComplexDirectory(sPath.c_str())) {
				return true;
			} else {
				std::string sText = "Directory: " + sPath + " can't be created.";
				MessageBox(hwnd, sText.c_str(), ExtManager::SECTION_NAME, MB_ICONERROR | MB_OK);
			}
		}
	} else {
		if(dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
			return true;
		} else {
			std::string sText = "Path: " + sPath + " already exists and is not a directory.";
			MessageBox(hwnd, sText.c_str(), ExtManager::SECTION_NAME, MB_ICONERROR | MB_OK);
		}
	}
	return false;
}

LRESULT CALLBACK SolidConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
			UpdateSolidDialogItem(hDlgWnd);
			EnableWindow(GetDlgItem(hDlgWnd, IDC_MAX_LOOKAHEAD_EDIT), FALSE);
			return FALSE;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDOK:
				{
					char buf[2048];
					SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
					if (IsExistentFolder(hDlgWnd, buf)) {
						if (UpdateSolidValue(hDlgWnd)) {
								SaveToIni();
						}
						EndDialog(hDlgWnd, IDOK);
					}
					break;
				}
				case IDCANCEL:
					EndDialog(hDlgWnd, IDCANCEL);
					break;
				case IDC_BROWSE_BUTTON:
				{
					char buf[MAX_PATH];
					HRESULT hr = CoInitialize(NULL);
					BROWSEINFO bwi = { hDlgWnd, NULL, buf, "Specify cache folder", BIF_RETURNONLYFSDIRS };
					LPITEMIDLIST piid = SHBrowseForFolder(&bwi);
					if(piid) {
						SHGetPathFromIDList(piid, buf);
						SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
						CoTaskMemFree(piid);
					}
					if(SUCCEEDED(hr)) CoUninitialize();
				}
					break;
				case IDC_CLEAR_BUTTON:
					if(MessageBox(hDlgWnd, "Do you want to clear cache contents?", "ax7z_s.spi confirmation", MB_YESNO) == IDYES)
						SolidCache::GetInstance().Clear();
					break;
				default:
					return FALSE;
			}
	}
	return FALSE;
}

//---------------------------------------------------------------------------
struct MyConcat3
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "") return s2;
		return s1 + ", " + s2;
	}
};

struct ListUpdater
{
	ListUpdater(HWND hwnd, std::vector<std::string> &v) : hwnd(hwnd), v(v) {}
	HWND hwnd;
	std::vector<std::string> &v;
	void operator()(const ExtManager::EachValueType& value)
	{
		std::string sLine("*.");
		sLine += value.first;
		if(value.first.size() < 8)
			sLine += std::string(8 - value.first.size(), ' ');
		sLine += std::accumulate(value.second.methods.begin(), value.second.methods.end(), std::string(), MyConcat3());
		LRESULT lResult = SendMessage(hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<const void*>(sLine.c_str())));
		v.push_back(value.first);
		SendMessage(hwnd, LB_SETSEL, value.second.enable, lResult);
	}
};

void UpdateDialogItem(HWND hDlgWnd)
{
	std::vector<std::string> *pvMap = static_cast<std::vector<std::string>*>(reinterpret_cast<void*>(GetWindowLongPtr(hDlgWnd, DWLP_USER)));

	pvMap->clear();
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_RESETCONTENT, 0L, 0L);

	g_extManager.Each(ListUpdater(GetDlgItem(hDlgWnd, IDC_EXTENSION_LIST), *pvMap));
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SETCARETINDEX, 0, 0);

	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(static_cast<const void*>(g_extManager.GetUserExtensions().c_str())));

	SendDlgItemMessage(hDlgWnd, IDC_TOPMOST_CHECK, BM_SETCHECK, PasswordManager::Get().GetTopMost() ? BST_CHECKED : BST_UNCHECKED, 0);

	SendDlgItemMessage(hDlgWnd, IDC_PERUSERCONF_CHECK, BM_SETCHECK, !g_fSharedConf ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool UpdateValue(HWND hDlgWnd)
{
	std::vector<std::string> *pvMap = static_cast<std::vector<std::string>*>(reinterpret_cast<void*>(GetWindowLongPtr(hDlgWnd, DWLP_USER)));

	int nNum = pvMap->size();
	for(int i = 0; i < nNum; ++i) {
		g_extManager.SetEnable((*pvMap)[i], SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_GETSEL, i, 0) > 0);
	}

	LRESULT lLen = SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_GETTEXTLENGTH, 0, 0);
	std::vector<char> vBuf(lLen+1);
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_GETTEXT, lLen+1, reinterpret_cast<LPARAM>(&vBuf[0]));
	g_extManager.SetUserExtensions(&vBuf[0]);

	PasswordManager::Get().SetTopMost(SendDlgItemMessage(hDlgWnd, IDC_TOPMOST_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED);

	bool fSharedConf = g_fSharedConf;
	std::string sIniFileName = g_sIniFileName;
	if(SendDlgItemMessage(hDlgWnd, IDC_PERUSERCONF_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED) {
		g_fSharedConf = false;
		g_sIniFileName = GetPersonalIniFileName(g_hInstance);
		MakePersonalIniFolder();
	} else {
		g_fSharedConf = true;
		g_sIniFileName = GetSharedIniFileName(g_hInstance);
	}
	if(fSharedConf != g_fSharedConf) {
		if(SaveToIni()) {
			DeleteFile(sIniFileName.c_str());
		} else {
			MessageBox(NULL, "Writing ini file is failed. Keep the old ini file location.", ExtManager::SECTION_NAME, MB_TASKMODAL | MB_ICONWARNING | MB_OK);
			g_fSharedConf = fSharedConf;
			g_sIniFileName = sIniFileName;
		}
	}

	return true;
}

LRESULT CALLBACK ConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	try {
		switch (msg) {
			case WM_INITDIALOG:
				SetWindowLongPtr(hDlgWnd, DWLP_USER, lp);
				UpdateDialogItem(hDlgWnd);
				return TRUE;
			case WM_COMMAND:
				switch (LOWORD(wp)) {
					case IDOK:
						if (UpdateValue(hDlgWnd)) {
							SaveToIni();
							EndDialog(hDlgWnd, IDOK);
						}
						break;
					case IDCANCEL:
						EndDialog(hDlgWnd, IDCANCEL);
						break;
					case IDC_DEFAULT_BUTTON:
						SetParamDefault();
						UpdateDialogItem(hDlgWnd);
						break;
					case IDC_SOLID_BUTTON:
						DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_SOLID_CONFIG_DIALOG), hDlgWnd, (DLGPROC)SolidConfigDlgProc);
						break;
					case IDC_SELECT_ALL_BUTTON:
						SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SELITEMRANGE, TRUE, MAKELPARAM(0, 0xFFFFU));
						break;
					case IDC_UNSELECT_ALL_BUTTON:
						SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 0xFFFFU));
						break;
					default:
						return FALSE;
				}
			default:
				return FALSE;
		}
		return TRUE;
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}

int __stdcall ConfigurationDlg(HWND parent, int fnc)
{
	try {
		if (fnc == 0) {
			//about
			DialogBox((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_ABOUT_DIALOG), parent, (DLGPROC)AboutDlgProc);
		} else {
			std::vector<std::string> vMap;
			DialogBoxParam((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), parent, (DLGPROC)ConfigDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(&vMap)));
		}
		return 0;
	} catch(...) {
		handle_exception(__FUNCTION__);
		return 0;
	}
}

int __stdcall ExtractSolidArchive(LPCWSTR filename, SPI_OnWriteCallback pCallback)
{
	try {
		return ExtractSolidArchiveEx(filename, pCallback);
	} catch(...) {
		handle_exception(__FUNCTION__);
		return SPI_OTHER_ERROR;
	}
}
