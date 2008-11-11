//fileInfo�L���b�V���N���X

#ifndef infcache_h
#define infcache_h

#include <windows.h>
#include "critsect.h"
#include "entryFuncs.h"

typedef struct ArcInfo
{
	HLOCAL hinfo;			// fileInfo[]
	char path[MAX_PATH];	// �t�@�C���p�X
} ArcInfo;

#define INFOCACHE_MAX 0x10
class InfoCache
{
public:
	InfoCache();
	~InfoCache();
	void Clear(void); //�L���b�V���N���A
	void Add(char *filepath, HLOCAL *ph); //�L���b�V���ɒǉ��BINFOCACHE_MAX �𒴂���ƌÂ��̂͏����B
	//�L���b�V���ɂ���΃A�[�J�C�u�����R�s�[�B
	int Dupli(char *filepath, HLOCAL *ph, fileInfo *pinfo);
private:
	CriticalSection cs;
	ArcInfo arcinfo[INFOCACHE_MAX];
	int nowno;
	bool GetCache(char *filepath, HLOCAL *ph);
};

// ���C�h������
typedef struct ArcInfoW
{
	HLOCAL hinfo;			// fileInfoW[]
	wchar_t path[MAX_PATH];	// �t�@�C���p�X
} ArcInfoW;

class InfoCacheW
{
public:
	InfoCacheW();
	~InfoCacheW();
	void Clear(void); //�L���b�V���N���A
	void Add(wchar_t *filepath, HLOCAL *ph); //�L���b�V���ɒǉ��BINFOCACHE_MAX �𒴂���ƌÂ��̂͏����B
	//�L���b�V���ɂ���΃A�[�J�C�u�����R�s�[�B
	int Dupli(wchar_t *filepath, HLOCAL *ph, fileInfoW *pinfo);
private:
	CriticalSection cs;
	ArcInfoW arcinfo[INFOCACHE_MAX];
	int nowno;
	bool GetCache(wchar_t *filepath, HLOCAL *ph);
};

#endif
