#ifdef WIN32
#define GAME_LAUNCH_NAME "./WolfTech.x64.exe"
#else
#define GAME_LAUNCH_NAME "./WolfTech.x86_64"
#endif
#ifndef GAME_LAUNCH_NAME
#error Please define your game exe name.
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
typedef PROCESS_INFORMATION ProcessType;
typedef HANDLE PipeType;
#define NULLPIPE NULL
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
typedef pid_t ProcessType;
typedef int PipeType;
#define NULLPIPE -1
#endif

#include "steam/steam_api.h"

static ISteamMatchmaking *GSteamMatchmaking = NULL;
static CSteamID GCurrentLobby;
static ISteamFriends *GSteamFriends = NULL;

static const uint32_t steam_app_id = 1379630u;

#define DEBUGPIPE 1
#if DEBUGPIPE
#define dbgpipe printf
#else
static inline void dbgpipe(const char *fmt, ...) {}
#endif

#ifdef WIN32
// maximum mumber of lines the output console should have
static const WORD MAX_CONSOLE_LINES = 500;
#endif

/* platform-specific mainline calls this. */
static int mainline(void);

/* Windows and Unix implementations of this stuff below. */
static void fail(const char *err);
static bool writePipe(PipeType fd, const void *buf, const unsigned int _len);
static int readPipe(PipeType fd, void *buf, const unsigned int _len);
static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
                        PipeType *pPipeChildRead, PipeType *pPipeChildWrite);
static void closePipe(PipeType fd);
static bool setEnvVar(const char *key, const char *val);
static bool launchChild(ProcessType *pid);
static int closeProcess(ProcessType *pid);

static void SteamParentLog(const char *msg)
{
	FILE *f = fopen("steam_parent_debug.txt", "a");
	if (!f) return;

	fprintf(f, "%s\n", msg);
	fclose(f);
}

#ifdef WIN32
bool ReleaseConsole()
{
    bool result = true;
    FILE* fp;

    // Just to be safe, redirect standard IO to NUL before releasing.

    // Redirect STDIN to NUL
    if (freopen_s(&fp, "NUL:", "r", stdin) != 0)
        result = false;
    else
        setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT to NUL
    if (freopen_s(&fp, "NUL:", "w", stdout) != 0)
        result = false;
    else
        setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR to NUL
    if (freopen_s(&fp, "NUL:", "w", stderr) != 0)
        result = false;
    else
        setvbuf(stderr, NULL, _IONBF, 0);

    // Detach from console
    if (!FreeConsole())
        result = false;

    return result;
}

bool RedirectConsoleIO()
{
    bool result = true;
    FILE* fp;

    // Redirect STDIN if the console has an input handle
    if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONIN$", "r", stdin) != 0)
            result = false;
        else
            setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT if the console has an output handle
    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0)
            result = false;
        else
            setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR if the console has an error handle
    if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0)
            result = false;
        else
            setvbuf(stderr, NULL, _IONBF, 0);

    // Make C++ standard streams point to console as well.
    std::ios::sync_with_stdio(true);

    // Clear the error state for each of the C++ standard streams.
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();

    return result;
}

void AdjustConsoleBuffer(int16_t minLength)
{
    // Set the screen buffer to be big enough to scroll some text
    CONSOLE_SCREEN_BUFFER_INFO conInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &conInfo);
    if (conInfo.dwSize.Y < minLength)
        conInfo.dwSize.Y = minLength;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), conInfo.dwSize);
}

bool CreateNewConsole(int16_t minLength)
{
    bool result = false;

    // Release any current console and redirect IO to NUL
    ReleaseConsole();

    // Attempt to create new console
    if (AllocConsole())
    {
        AdjustConsoleBuffer(minLength);
        result = RedirectConsoleIO();
    }

    return result;
}

static void fail(const char *err)
{
    MessageBoxA(NULL, err, "ERROR", MB_ICONERROR | MB_OK);
    ExitProcess(1);
} // fail

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
    const DWORD len = (DWORD) _len;
    DWORD bw = 0;
    return ((WriteFile(fd, buf, len, &bw, NULL) != 0) && (bw == len));
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
    const DWORD len = (DWORD) _len;
    DWORD br = 0;
    return ReadFile(fd, buf, len, &br, NULL) ? (int) br : -1;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
                        PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
    SECURITY_ATTRIBUTES pipeAttr;

    pipeAttr.nLength = sizeof (pipeAttr);
    pipeAttr.lpSecurityDescriptor = NULL;
    pipeAttr.bInheritHandle = TRUE;
    if (!CreatePipe(pPipeParentRead, pPipeChildWrite, &pipeAttr, 0))
        return 0;

    pipeAttr.nLength = sizeof (pipeAttr);
    pipeAttr.lpSecurityDescriptor = NULL;
    pipeAttr.bInheritHandle = TRUE;
    if (!CreatePipe(pPipeChildRead, pPipeParentWrite, &pipeAttr, 0))
    {
        CloseHandle(*pPipeParentRead);
        CloseHandle(*pPipeChildWrite);
        return 0;
    } // if

    return 1;
} // createPipes

static void closePipe(PipeType fd)
{
    CloseHandle(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{
    return (SetEnvironmentVariableA(key, val) != 0);
} // setEnvVar

/*
static bool launchChild(ProcessType *pid)
{
    TCHAR name[MAX_PATH] = _T(GAME_LAUNCH_NAME);
    _tprintf(_T("%ls\n"), name);

    STARTUPINFOW cif = {0};
    cif.cb = sizeof(STARTUPINFO);

    return (CreateProcessW(name, NULL, NULL, NULL, true, 
        0, NULL, NULL, &cif, pid) != 0);
} // launchChild
*/

// pass args from steam to game for windows
static bool launchChild(ProcessType* pid)
{
    TCHAR gamePath[MAX_PATH] = _T(GAME_LAUNCH_NAME);
    _tprintf(_T("%ls\n"), gamePath);

    LPTSTR originalCmdLine = GetCommandLine();
    TCHAR finalCmdLine[4096] = {0};

    _tcscpy(finalCmdLine, _T("\""));
    _tcscat(finalCmdLine, gamePath);
    _tcscat(finalCmdLine, _T("\""));

    LPTSTR args = NULL;
    if (originalCmdLine[0] == _T('\"')) {
        args = _tcschr(originalCmdLine + 1, _T('\"'));
        if (args != NULL) {
            args = _tcschr(args + 1, _T(' '));
        }
    } else {
        args = _tcschr(originalCmdLine, _T(' '));
    }

    if (args != NULL) {
        _tcscat(finalCmdLine, args);
    }

    _tprintf(_T("Command line: %ls\n"), finalCmdLine);

    STARTUPINFOW cif = {0};
    cif.cb = sizeof(STARTUPINFO);

    return CreateProcessW(NULL, finalCmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &cif, pid);
} // launchChild

static int closeProcess(ProcessType *pid)
{
    CloseHandle(pid->hProcess);
    CloseHandle(pid->hThread);
    return 0;
} // closeProcess

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
    // CreateNewConsole(1024); // uncomment for debug console
    mainline();
    ExitProcess(0);
    return 0;  // just in case.
} // WinMain


#else  // everyone else that isn't Windows.

static void fail(const char *err)
{
    // !!! FIXME: zenity or something.
    fprintf(stderr, "%s\n", err);
    _exit(1);
} // fail

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
    const ssize_t len = (ssize_t) _len;
    ssize_t bw;
    while (((bw = write(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
    return (bw == len);
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
    const ssize_t len = (ssize_t) _len;
    ssize_t br;
    while (((br = read(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
    return (int) br;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
                        PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
    int fds[2];
    if (pipe(fds) == -1)
        return 0;
    *pPipeParentRead = fds[0];
    *pPipeChildWrite = fds[1];

    if (pipe(fds) == -1)
    {
        close(*pPipeParentRead);
        close(*pPipeChildWrite);
        return 0;
    } // if

    *pPipeChildRead = fds[0];
    *pPipeParentWrite = fds[1];

    return 1;
} // createPipes

static void closePipe(PipeType fd)
{
    close(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{
    return (setenv(key, val, 1) != -1);
} // setEnvVar

static int GArgc = 0;
static char **GArgv = NULL;

static bool launchChild(ProcessType *pid)
{
    *pid = fork();
    if (*pid == -1)   // failed
        return false;
    else if (*pid != 0)  // we're the parent
        return true;  // we'll let the pipe fail if this didn't work.

    // we're the child.
    GArgv[0] = strdup("./" GAME_LAUNCH_NAME);
    execvp(GArgv[0], GArgv);
    // still here? It failed! Terminate, closing child's ends of the pipes.
    _exit(1);
} // launchChild

static int closeProcess(ProcessType *pid)
{
    int rc = 0;
    while ((waitpid(*pid, &rc, 0) == -1) && (errno == EINTR)) { /*spin*/ }
    if (!WIFEXITED(rc))
        return 1;  // oh well.
    return WEXITSTATUS(rc);
} // closeProcess

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    GArgc = argc;
    GArgv = argv;
    return mainline();
} // main

#endif


// THE ACTUAL PROGRAM.

class SteamBridge;

static ISteamUserStats *GSteamStats = NULL;
static ISteamUtils *GSteamUtils = NULL;
static ISteamUser *GSteamUser = NULL;
static AppId_t GAppID = 0;
static uint64 GUserID = 0;
static SteamBridge *GSteamBridge = NULL;

static ISteamNetworkingSockets *GSteamNetworkingSockets = NULL;
static HSteamListenSocket GP2PListenSocket = k_HSteamListenSocket_Invalid;

// Multi-peer P2P connection tracking: a dedicated/listen server can have
// several clients connected at once, each its own HSteamNetConnection.
#define MAX_P2P_CONNECTIONS 32

struct P2PConnEntry
{
	uint64 steamID;
	HSteamNetConnection conn;
	bool inUse;
};

static P2PConnEntry GP2PConns[MAX_P2P_CONNECTIONS];

static HSteamNetConnection P2P_FindConnBySteamID(uint64 steamID)
{
	for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
	{
		if (GP2PConns[i].inUse && GP2PConns[i].steamID == steamID)
			return GP2PConns[i].conn;
	}
	return k_HSteamNetConnection_Invalid;
}

static int P2P_FindIndexByHandle(HSteamNetConnection conn)
{
	for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
	{
		if (GP2PConns[i].inUse && GP2PConns[i].conn == conn)
			return i;
	}
	return -1;
}

// Returns false if the table is full; caller should close the connection.
static bool P2P_TrackConn(uint64 steamID, HSteamNetConnection conn)
{
	for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
	{
		if (!GP2PConns[i].inUse)
		{
			GP2PConns[i].steamID = steamID;
			GP2PConns[i].conn = conn;
			GP2PConns[i].inUse = true;
			return true;
		}
	}
	return false;
}

static void P2P_UntrackByHandle(HSteamNetConnection conn)
{
	int idx = P2P_FindIndexByHandle(conn);
	if (idx >= 0)
		GP2PConns[idx].inUse = false;
}

class SteamBridge
{
public:
    SteamBridge(PipeType _fd);

    STEAM_CALLBACK(SteamBridge, OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived);
    STEAM_CALLBACK(SteamBridge, OnUserStatsStored, UserStatsStored_t, m_CallbackUserStatsStored);

    STEAM_CALLBACK(SteamBridge, OnLobbyEnter, LobbyEnter_t, m_CallbackLobbyEnter);
    STEAM_CALLBACK(SteamBridge, OnLobbyDataUpdate, LobbyDataUpdate_t, m_CallbackLobbyDataUpdate);
    STEAM_CALLBACK(SteamBridge, OnNetConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t, m_CallbackNetConnStatusChanged);

    void OnLobbyCreated(LobbyCreated_t *pCallback, bool bIOFailure);
    void OnLobbyMatchList(LobbyMatchList_t *pCallback, bool bIOFailure);

    CCallResult<SteamBridge, LobbyCreated_t> m_CallResultLobbyCreated;
    CCallResult<SteamBridge, LobbyMatchList_t> m_CallResultLobbyMatchList;

private:
    PipeType fd;
};

typedef enum ShimCmd
{
	SHIMCMD_BYE,
	SHIMCMD_PUMP,
	SHIMCMD_REQUESTSTATS,
	SHIMCMD_STORESTATS,
	SHIMCMD_SETACHIEVEMENT,
	SHIMCMD_GETACHIEVEMENT,
	SHIMCMD_RESETSTATS,
	SHIMCMD_SETSTATI,
	SHIMCMD_GETSTATI,
	SHIMCMD_SETSTATF,
	SHIMCMD_GETSTATF,
	SHIMCMD_RESTARTAPP,
	SHIMCMD_SETRICHPRESENCE,

	SHIMCMD_LOBBY_CREATE,
	SHIMCMD_LOBBY_LIST,
	SHIMCMD_LOBBY_JOIN,
	SHIMCMD_LOBBY_LEAVE,
	SHIMCMD_LOBBY_SETDATA,
	SHIMCMD_LOBBY_INVITE,

	// Steam P2P net transport (per-frame game traffic).
	SHIMCMD_NET_LISTEN,
	SHIMCMD_NET_CONNECT,
	SHIMCMD_NET_SEND,
	SHIMCMD_NET_CLOSE,
} ShimCmd;

#define SHIM_LEN_ESCAPE 0xFF
#define SHIM_MAX_NET_PACKET 2048

typedef enum ShimEvent
{
	SHIMEVENT_BYE,
	SHIMEVENT_STATSRECEIVED,
	SHIMEVENT_STATSSTORED,
	SHIMEVENT_SETACHIEVEMENT,
	SHIMEVENT_GETACHIEVEMENT,
	SHIMEVENT_RESETSTATS,
	SHIMEVENT_SETSTATI,
	SHIMEVENT_GETSTATI,
	SHIMEVENT_SETSTATF,
	SHIMEVENT_GETSTATF,
	SHIMEVENT_APPRESTARTED,
	SHIMEVENT_SETRICHPRESENCE,

	SHIMEVENT_LOBBY_CREATED,
	SHIMEVENT_LOBBY_LIST,
	SHIMEVENT_LOBBY_JOINED,
	SHIMEVENT_LOBBY_DATA,
	SHIMEVENT_LOBBY_CHAT,
	SHIMEVENT_LOBBY_INVITE,
	SHIMEVENT_LOBBY_OWNER,	// uvalue = owning steamID of the current lobby

	// Steam P2P net transport (per-frame game traffic).
	SHIMEVENT_NET_CONNECTED,
	SHIMEVENT_NET_DISCONNECTED,
	SHIMEVENT_NET_DATA,
} ShimEvent;

static bool write1ByteCmd(PipeType fd, const uint8 b1)
{
    const uint8 buf[] = { 1, b1 };
    return writePipe(fd, buf, sizeof (buf));
} // write1ByteCmd

static bool write2ByteCmd(PipeType fd, const uint8 b1, const uint8 b2)
{
    const uint8 buf[] = { 2, b1, b2 };
    return writePipe(fd, buf, sizeof (buf));
} // write2ByteCmd

static bool write3ByteCmd(PipeType fd, const uint8 b1, const uint8 b2, const uint8 b3)
{
    const uint8 buf[] = { 3, b1, b2, b3 };
    return writePipe(fd, buf, sizeof (buf));
} // write3ByteCmd


static inline bool writeBye(PipeType fd)
{
    dbgpipe("Parent sending SHIMEVENT_BYE().\n");
    return write1ByteCmd(fd, SHIMEVENT_BYE);
} // writeBye

static inline bool writeStatsReceived(PipeType fd, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_STATSRECEIVED(%sokay).\n", okay ? "" : "!");
    return write2ByteCmd(fd, SHIMEVENT_STATSRECEIVED, okay ? 1 : 0);
} // writeStatsReceived

static inline bool writeStatsStored(PipeType fd, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_STATSSTORED(%sokay).\n", okay ? "" : "!");
    return write2ByteCmd(fd, SHIMEVENT_STATSSTORED, okay ? 1 : 0);
} // writeStatsStored

static bool writeAchievementSet(PipeType fd, const char *name, const bool enable, const bool okay)
{
    uint8 buf[256];
    uint8 *ptr = buf+1;
    dbgpipe("Parent sending SHIMEVENT_SETACHIEVEMENT('%s', %senable, %sokay).\n", name, enable ? "" : "!", okay ? "" : "!");
    *(ptr++) = (uint8) SHIMEVENT_SETACHIEVEMENT;
    *(ptr++) = enable ? 1 : 0;
    *(ptr++) = okay ? 1 : 0;
    strcpy((char *) ptr, name);
    ptr += strlen(name) + 1;
    buf[0] = (uint8) ((ptr-1) - buf);
    return writePipe(fd, buf, buf[0] + 1);
} // writeAchievementSet

static bool writeAchievementGet(PipeType fd, const char *name, const int status, const uint64 time)
{
    uint8 buf[256];
    uint8 *ptr = buf+1;
    dbgpipe("Parent sending SHIMEVENT_GETACHIEVEMENT('%s', status %d, time %llu).\n", name, status, (unsigned long long) time);
    *(ptr++) = (uint8) SHIMEVENT_GETACHIEVEMENT;
    *(ptr++) = (uint8) status;
    memcpy(ptr, &time, sizeof (time));
    ptr += sizeof (time);
    strcpy((char *) ptr, name);
    ptr += strlen(name) + 1;
    buf[0] = (uint8) ((ptr-1) - buf);
    return writePipe(fd, buf, buf[0] + 1);
} // writeAchievementGet

static inline bool writeResetStats(PipeType fd, const bool alsoAch, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_RESETSTATS(%salsoAchievements, %sokay).\n", alsoAch ? "" : "!", okay ? "" : "!");
    return write3ByteCmd(fd, SHIMEVENT_RESETSTATS, alsoAch ? 1 : 0, okay ? 1 : 0);
} // writeResetStats

static bool writeStatThing(PipeType fd, const ShimEvent ev, const char *name, const void *val, const size_t vallen, const bool okay)
{
    uint8 buf[256];
    uint8 *ptr = buf+1;
    *(ptr++) = (uint8) ev;
    *(ptr++) = okay ? 1 : 0;
    memcpy(ptr, val, vallen);
    ptr += vallen;
    strcpy((char *) ptr, name);
    ptr += strlen(name) + 1;
    buf[0] = (uint8) ((ptr-1) - buf);
    return writePipe(fd, buf, buf[0] + 1);
} // writeStatThing

static bool writeRestartResult(PipeType fd, const ShimEvent ev, const bool result)
{
	uint8 buf[3];
	uint8 *ptr = buf + 1;

	*(ptr++) = (uint8)ev;
	*(ptr++) = result ? 1 : 0;

	buf[0] = (uint8)((ptr - 1) - buf);
	return writePipe(fd, buf, buf[0] + 1);
}

static bool writeSetRichPresenceResult(PipeType fd, const ShimEvent ev, const bool result)
{
	uint8 buf[3];
	uint8 *ptr = buf + 1;

	*(ptr++) = (uint8)ev;
	*(ptr++) = result ? 1 : 0;

	buf[0] = (uint8)((ptr - 1) - buf);
	return writePipe(fd, buf, buf[0] + 1);
}

static bool writeLobbyEvent(PipeType fd, const ShimEvent ev, const bool okay, const uint64 lobbyID, const char *text)
{
	uint8 buf[256];
	uint8 *ptr = buf + 1;
	uint8 *end = buf + sizeof(buf);

	*(ptr++) = (uint8)ev;
	*(ptr++) = okay ? 1 : 0;

	if (ptr + sizeof(lobbyID) > end) {
		return false;
	}

	memcpy(ptr, &lobbyID, sizeof(lobbyID));
	ptr += sizeof(lobbyID);

	if (!text) {
		text = "";
	}

	size_t textLen = strlen(text) + 1;
	if (ptr + textLen > end) {
		textLen = (size_t)(end - ptr);

		if (textLen == 0) {
			return false;
		}

		memcpy(ptr, text, textLen - 1);
		ptr += textLen - 1;
		*(ptr++) = '\0';
	} else {
		memcpy(ptr, text, textLen);
		ptr += textLen;
	}

	buf[0] = (uint8)((ptr - 1) - buf);
	return writePipe(fd, buf, buf[0] + 1);
}

// SHIMEVENT_NET_CONNECTED / NET_DISCONNECTED: ev + steamID.
static bool writeNetConnEvent(PipeType fd, const ShimEvent ev, const uint64 steamID)
{
	uint8 buf[1 + 1 + sizeof(uint64)];
	uint8 *ptr = buf + 1;

	*(ptr++) = (uint8)ev;
	memcpy(ptr, &steamID, sizeof(steamID));
	ptr += sizeof(steamID);

	buf[0] = (uint8)((ptr - 1) - buf);
	return writePipe(fd, buf, buf[0] + 1);
}

/* 
SHIMEVENT_NET_DATA: ev + steamID + raw packet bytes. Payload can exceed 254 bytes (up to SHIM_MAX_NET_PACKET), 
so this uses the escape-length framing instead of the plain single-byte length the other writers use.
*/
static bool writeNetDataEvent(PipeType fd, const uint64 steamID, const void *data, const int len)
{
	static uint8 buf[3 + 1 + sizeof(uint64) + SHIM_MAX_NET_PACKET];
	uint8 *ptr;
	int totalLen;
	int hdrlen;

	if (len < 0 || len > SHIM_MAX_NET_PACKET) return false;

	totalLen = 1 + (int)sizeof(uint64) + len;  // ev byte + steamID + payload

	if (totalLen < SHIM_LEN_ESCAPE)
	{
		buf[0] = (uint8) totalLen;
		hdrlen = 1;
	}
	else
	{
		buf[0] = SHIM_LEN_ESCAPE;
		buf[1] = (uint8) (totalLen & 0xFF);
		buf[2] = (uint8) ((totalLen >> 8) & 0xFF);
		hdrlen = 3;
	}

	ptr = buf + hdrlen;
	*(ptr++) = (uint8) SHIMEVENT_NET_DATA;
	memcpy(ptr, &steamID, sizeof(steamID));
	ptr += sizeof(steamID);
	memcpy(ptr, data, len);
	ptr += len;

	return writePipe(fd, buf, (unsigned int)(ptr - buf));
}

static inline bool writeSetStatI(PipeType fd, const char *name, const int32 val, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_SETSTATI('%s', val %d, %sokay).\n", name, (int) val, okay ? "" : "!");
    return writeStatThing(fd, SHIMEVENT_SETSTATI, name, &val, sizeof (val), okay);
} // writeSetStatI

static inline bool writeSetStatF(PipeType fd, const char *name, const float val, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_SETSTATF('%s', val %f, %sokay).\n", name, val, okay ? "" : "!");
    return writeStatThing(fd, SHIMEVENT_SETSTATF, name, &val, sizeof (val), okay);
} // writeSetStatF

static inline bool writeGetStatI(PipeType fd, const char *name, const int32 val, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_GETSTATI('%s', val %d, %sokay).\n", name, (int) val, okay ? "" : "!");
    return writeStatThing(fd, SHIMEVENT_GETSTATI, name, &val, sizeof (val), okay);
} // writeGetStatI

static inline bool writeGetStatF(PipeType fd, const char *name, const float val, const bool okay)
{
    dbgpipe("Parent sending SHIMEVENT_GETSTATF('%s', val %f, %sokay).\n", name, val, okay ? "" : "!");
    return writeStatThing(fd, SHIMEVENT_GETSTATF, name, &val, sizeof (val), okay);
} // writeGetStatF

static inline bool writeRestartApp(PipeType fd, const bool result)
{
    dbgpipe("Parent sending SHIMEVENT_APPRESTARTED(%sokay).\n", result ? "" : "!");
    return writeRestartResult(fd, SHIMEVENT_APPRESTARTED, result);
} // writeRestartApp

static inline bool writeSetRichPresence(PipeType fd, const bool result)
{
    dbgpipe("Parent sending SHIMEVENT_SETSTATUS(%sokay).\n", result ? "" : "!");
    return writeSetRichPresenceResult(fd, SHIMEVENT_SETRICHPRESENCE, result);
} // writeSetRichPresence


SteamBridge::SteamBridge(PipeType _fd)
    : m_CallbackUserStatsReceived(this, &SteamBridge::OnUserStatsReceived)
    , m_CallbackUserStatsStored(this, &SteamBridge::OnUserStatsStored)
    , m_CallbackLobbyEnter(this, &SteamBridge::OnLobbyEnter)
    , m_CallbackLobbyDataUpdate(this, &SteamBridge::OnLobbyDataUpdate)
    , m_CallbackNetConnStatusChanged(this, &SteamBridge::OnNetConnectionStatusChanged)
    , fd(_fd)
{
}

void SteamBridge::OnUserStatsReceived(UserStatsReceived_t *pCallback)
{
	if (GAppID != pCallback->m_nGameID) return;
	if (GUserID != pCallback->m_steamIDUser.ConvertToUint64()) return;
    writeStatsReceived(fd, pCallback->m_eResult == k_EResultOK);
} // SteamBridge::OnUserStatsReceived

void SteamBridge::OnUserStatsStored(UserStatsStored_t *pCallback)
{
	if (GAppID != pCallback->m_nGameID) return;
    writeStatsStored(fd, pCallback->m_eResult == k_EResultOK);
} // SteamBridge::OnUserStatsStored

void SteamBridge::OnLobbyCreated(LobbyCreated_t *pCallback, bool bIOFailure)
{
	const bool okay = (!bIOFailure && pCallback->m_eResult == k_EResultOK);


SteamParentLog("PARENT: OnLobbyCreated called");

	if (!okay) {
		dbgpipe("Lobby create failed\n");
		writeLobbyEvent(fd, SHIMEVENT_LOBBY_CREATED, false, 0, "");
		return;
	}

	GCurrentLobby = CSteamID(pCallback->m_ulSteamIDLobby);

	if (GSteamMatchmaking && GCurrentLobby.IsValid()) {
		GSteamMatchmaking->SetLobbyData(GCurrentLobby, "game", "wolftech");
		GSteamMatchmaking->SetLobbyData(GCurrentLobby, "name", "WolfTech Lobby");

		/*
		Temporary.
		This is fine for local testing only.
		Later replace with real public address or Steam networking.
		*/
		GSteamMatchmaking->SetLobbyData(GCurrentLobby, "connect", "127.0.0.1:27960");

        
	}

	dbgpipe("Lobby created: %llu\n", GCurrentLobby.ConvertToUint64());

	writeLobbyEvent(
		fd,
		SHIMEVENT_LOBBY_CREATED,
		true,
		GCurrentLobby.ConvertToUint64(),
		""
	);
}

void SteamBridge::OnLobbyMatchList(LobbyMatchList_t *pCallback, bool bIOFailure)
{
	if (bIOFailure) {
		dbgpipe("Lobby list failed\n");
		writeLobbyEvent(fd, SHIMEVENT_LOBBY_LIST, false, 0, "");
		return;
	}

	dbgpipe("Lobbies found: %u\n", pCallback->m_nLobbiesMatching);

	for (uint32 i = 0; i < pCallback->m_nLobbiesMatching; i++) {
		CSteamID lobby = GSteamMatchmaking->GetLobbyByIndex(i);

		const char *name = GSteamMatchmaking->GetLobbyData(lobby, "name");
		const char *map = GSteamMatchmaking->GetLobbyData(lobby, "map");

		char display[128];

		if (name && name[0] && map && map[0]) {
			snprintf(display, sizeof(display), "%s [%s]", name, map);
		} else if (name && name[0]) {
			snprintf(display, sizeof(display), "%s", name);
		} else {
			snprintf(display, sizeof(display), "WolfTech Lobby");
		}

		dbgpipe("Lobby %u: %llu %s\n",
			i,
			lobby.ConvertToUint64(),
			display);

		writeLobbyEvent(
			fd,
			SHIMEVENT_LOBBY_LIST,
			true,
			lobby.ConvertToUint64(),
			display
		);
	}

	/*
	Terminator event.
	Game side can treat lobbyID 0 as "list finished".
	*/
	writeLobbyEvent(fd, SHIMEVENT_LOBBY_LIST, true, 0, "DONE");
}

void SteamBridge::OnLobbyEnter(LobbyEnter_t *pCallback)
{
	GCurrentLobby = CSteamID(pCallback->m_ulSteamIDLobby);

	const char *connect = "";

	if (GSteamMatchmaking && GCurrentLobby.IsValid()) {
		connect = GSteamMatchmaking->GetLobbyData(GCurrentLobby, "connect");
	}

	dbgpipe("Entered lobby: %llu connect=%s\n",
		GCurrentLobby.ConvertToUint64(),
		connect ? connect : "");

	writeLobbyEvent(
		fd,
		SHIMEVENT_LOBBY_JOINED,
		true,
		GCurrentLobby.ConvertToUint64(),
		connect ? connect : ""
	);

	// Let the child know who owns this lobby, and automatically start
	// listening (if we're the owner/host) or connecting out (if we're a
	// joining client) - for the common case, game code doesn't need to
	// call steamNetListen()/steamNetConnect() itself at all.
	if (GSteamMatchmaking && GCurrentLobby.IsValid() && GSteamNetworkingSockets)
	{
		const CSteamID ownerID = GSteamMatchmaking->GetLobbyOwner(GCurrentLobby);
		const uint64 ownerSteamID = ownerID.ConvertToUint64();

		writeNetConnEvent(fd, SHIMEVENT_LOBBY_OWNER, ownerSteamID);

		if (ownerSteamID == GUserID)
		{
			if (GP2PListenSocket == k_HSteamListenSocket_Invalid)
			{
				GP2PListenSocket = GSteamNetworkingSockets->CreateListenSocketP2P(0, 0, NULL);
				dbgpipe("OnLobbyEnter: we own this lobby, listening (socket=%u)\n",
					(unsigned int) GP2PListenSocket);
			}
		}
		else if (P2P_FindConnBySteamID(ownerSteamID) == k_HSteamNetConnection_Invalid)
		{
			SteamNetworkingIdentity identity;
			identity.SetSteamID64(ownerSteamID);
			const HSteamNetConnection conn = GSteamNetworkingSockets->ConnectP2P(identity, 0, 0, NULL);
			dbgpipe("OnLobbyEnter: connecting to lobby owner %llu (conn=%u)\n",
				(unsigned long long) ownerSteamID, (unsigned int) conn);
		}
	}
}

void SteamBridge::OnLobbyDataUpdate(LobbyDataUpdate_t *pCallback)
{
	const uint64 lobbyID = pCallback->m_ulSteamIDLobby;

	dbgpipe("Lobby data updated: %llu\n", lobbyID);

	writeLobbyEvent(
		fd,
		SHIMEVENT_LOBBY_DATA,
		true,
		lobbyID,
		""
	);
}

void SteamBridge::OnNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pCallback)
{
	const uint64 peerSteamID = pCallback->m_info.m_identityRemote.GetSteamID64();

	switch (pCallback->m_info.m_eState)
	{
		case k_ESteamNetworkingConnectionState_Connecting:
		{
			// Only auto-accept inbound connections on our listen socket
			// (i.e. we're the host). Outbound connections we initiated
			// via ConnectP2P go through FindingRoute -> Connected instead.
			if (pCallback->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid)
			{
				if (GSteamNetworkingSockets->AcceptConnection(pCallback->m_hConn) != k_EResultOK)
				{
					dbgpipe("Failed to accept incoming P2P connection from %llu\n",
						(unsigned long long) peerSteamID);
					GSteamNetworkingSockets->CloseConnection(pCallback->m_hConn, 0, "accept failed", false);
				}
			}
			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
		{
			if (!P2P_TrackConn(peerSteamID, pCallback->m_hConn))
			{
				dbgpipe("P2P connection table full, rejecting %llu\n",
					(unsigned long long) peerSteamID);
				GSteamNetworkingSockets->CloseConnection(pCallback->m_hConn, 0, "server full", false);
				break;
			}

			dbgpipe("P2P connected: %llu\n", (unsigned long long) peerSteamID);
			writeNetConnEvent(fd, SHIMEVENT_NET_CONNECTED, peerSteamID);
			break;
		}

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		{
			// Only report+untrack if we'd actually gotten to Connected;
			// a failed outbound ConnectP2P attempt never got tracked.
			const bool wasTracked = (P2P_FindIndexByHandle(pCallback->m_hConn) >= 0);

			P2P_UntrackByHandle(pCallback->m_hConn);
			GSteamNetworkingSockets->CloseConnection(pCallback->m_hConn, 0, NULL, false);

			dbgpipe("P2P disconnected: %llu (wasTracked=%d)\n",
				(unsigned long long) peerSteamID, wasTracked ? 1 : 0);
			writeNetConnEvent(fd, SHIMEVENT_NET_DISCONNECTED, peerSteamID);
			break;
		}

		default:
			break;
	}
}


// Called once per SHIMCMD_PUMP, right alongside SteamAPI_RunCallbacks():
// drains inbound game packets on every tracked P2P connection and forwards
// each one to the child as a SHIMEVENT_NET_DATA.
static void PumpP2PMessages(PipeType fd)
{
	SteamNetworkingMessage_t *msgs[8];

	if (!GSteamNetworkingSockets)
		return;

	for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
	{
		if (!GP2PConns[i].inUse)
			continue;

		for (;;)
		{
			const int n = GSteamNetworkingSockets->ReceiveMessagesOnConnection(
				GP2PConns[i].conn, msgs, 8);

			if (n <= 0)
				break;

			for (int j = 0; j < n; j++)
			{
				writeNetDataEvent(fd, GP2PConns[i].steamID, msgs[j]->m_pData, msgs[j]->m_cbSize);
				msgs[j]->Release();
			}

			if (n < 8)
				break;  // fewer than requested: nothing more queued right now.
		}
	}
}

static bool processCommand(const uint8 *buf, unsigned int buflen, PipeType fd)
{
    if (buflen == 0)
        return true;

    const ShimCmd cmd = (ShimCmd) *(buf++);
    buflen--;

    #if DEBUGPIPE
    if (false) {}
    #define PRINTGOTCMD(x) else if (cmd == x) printf("Parent got " #x ".\n")
    PRINTGOTCMD(SHIMCMD_BYE);
    PRINTGOTCMD(SHIMCMD_PUMP);
    PRINTGOTCMD(SHIMCMD_REQUESTSTATS);
    PRINTGOTCMD(SHIMCMD_STORESTATS);
    PRINTGOTCMD(SHIMCMD_SETACHIEVEMENT);
    PRINTGOTCMD(SHIMCMD_GETACHIEVEMENT);
    PRINTGOTCMD(SHIMCMD_RESETSTATS);
    PRINTGOTCMD(SHIMCMD_SETSTATI);
    PRINTGOTCMD(SHIMCMD_GETSTATI);
    PRINTGOTCMD(SHIMCMD_SETSTATF);
    PRINTGOTCMD(SHIMCMD_GETSTATF);
    PRINTGOTCMD(SHIMCMD_RESTARTAPP);
    PRINTGOTCMD(SHIMCMD_SETRICHPRESENCE);
    PRINTGOTCMD(SHIMCMD_LOBBY_CREATE);
    PRINTGOTCMD(SHIMCMD_LOBBY_LIST);
    PRINTGOTCMD(SHIMCMD_LOBBY_JOIN);
    PRINTGOTCMD(SHIMCMD_LOBBY_LEAVE);
    PRINTGOTCMD(SHIMCMD_LOBBY_SETDATA);
    PRINTGOTCMD(SHIMCMD_LOBBY_INVITE);
    PRINTGOTCMD(SHIMCMD_NET_LISTEN);
    PRINTGOTCMD(SHIMCMD_NET_CONNECT);
    PRINTGOTCMD(SHIMCMD_NET_CLOSE);
    /* NET_SEND happens every frame; too noisy to log here. */
#undef PRINTGOTCMD
    else printf("Parent got unknown shimcmd %d.\n", (int) cmd);
    #endif

    switch (cmd)
    {
        case SHIMCMD_PUMP:
            SteamAPI_RunCallbacks();
            PumpP2PMessages(fd);
            break;

        case SHIMCMD_BYE:
            writeBye(fd);
            return false;

        case SHIMCMD_REQUESTSTATS:
            // Steamworks SDK 1.61 removed ISteamUserStats::RequestCurrentStats().
            // Stats and achievements are now synchronized by the Steam client before launch.
            // Keep the shim command for compatibility with existing game code.
            writeStatsReceived(fd, GSteamStats != NULL);
            break;

        case SHIMCMD_STORESTATS:
            if ((!GSteamStats) || (!GSteamStats->StoreStats()))
                writeStatsStored(fd, false);
            // callback later.
            break;

        case SHIMCMD_SETACHIEVEMENT:
            if (buflen >= 2)
            {
                const bool enable = (*(buf++) != 0);
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                if (!GSteamStats)
                {
                    writeAchievementSet(fd, name, enable, false);
                    printf("set ach 1");
                }
                else if (enable && !GSteamStats->SetAchievement(name))
                {
                    writeAchievementSet(fd, name, enable, false);
                    printf("set ach 2");
                }
                else if (!enable && !GSteamStats->ClearAchievement(name))
                {
                    writeAchievementSet(fd, name, enable, false);
                    printf("set ach 3");
                }
                else
                {
                    writeAchievementSet(fd, name, enable, true);
                    printf("set ach 4");
                }
            } // if
            break;

        case SHIMCMD_GETACHIEVEMENT:
            if (buflen)
            {
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                bool ach = false;
	            uint32 t = 0;
                if ((GSteamStats) && (GSteamStats->GetAchievementAndUnlockTime(name, &ach, &t)))
                    writeAchievementGet(fd, name, ach ? 1 : 0, t);
	            else
                    writeAchievementGet(fd, name, 2, 0);
            } // if
            break;

        case SHIMCMD_RESETSTATS:
            if (buflen)
            {
                const bool alsoAch = (*(buf++) != 0);
                writeResetStats(fd, alsoAch, (GSteamStats) && (GSteamStats->ResetAllStats(alsoAch)));
            } // if
            break;

        case SHIMCMD_SETSTATI:
            if (buflen >= 5)
            {
                const int32 val = *((int32 *) buf);
                buf += sizeof (int32);
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                writeSetStatI(fd, name, val, (GSteamStats) && (GSteamStats->SetStat(name, val)));
            } // if
            break;

        case SHIMCMD_GETSTATI:
            if (buflen)
            {
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                int32 val = 0;
                if ((GSteamStats) && (GSteamStats->GetStat(name, &val)))
                    writeGetStatI(fd, name, val, true);
                else
                    writeGetStatI(fd, name, 0, false);
            } // if
            break;

        case SHIMCMD_SETSTATF:
            if (buflen >= 5)
            {
                const float val = *((float *) buf);
                buf += sizeof (float);
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                writeSetStatF(fd, name, val, (GSteamStats) && (GSteamStats->SetStat(name, val)));
            } // if
            break;

        case SHIMCMD_GETSTATF:
            if (buflen)
            {
                const char *name = (const char *) buf;   // !!! FIXME: buffer overflow possible.
                float val = 0;
                if ((GSteamStats) && (GSteamStats->GetStat(name, &val)))
                    writeGetStatF(fd, name, val, true);
                else
                    writeGetStatF(fd, name, 0.0f, false);
            } // if
            break;
            
#ifdef RELEASE
        case SHIMCMD_RESTARTAPP:
            if (buflen == 5)
            {
                int appId = *(unsigned int*)buf;
                bool result = SteamAPI_RestartAppIfNecessary(appId);

                writeRestartApp(fd, result);
            }
            break;
#endif

        case SHIMCMD_SETRICHPRESENCE:
            if (buflen > 4)
            {
                char key[64], value[64];
                
                strcpy(key, (char*)buf);
                buf += strlen(key) + 1;

                strcpy(value, (char*)buf);
                buf += strlen(value) + 1;

                bool result = SteamFriends()->SetRichPresence(key, value);

                dbgpipe("SetRichPresence(%s, %s)\n", key, value);

                writeSetRichPresence(fd, result);
            }
            break;

        case SHIMCMD_LOBBY_CREATE:
        {

            SteamParentLog("PARENT: got SHIMCMD_LOBBY_CREATE");

            if (!GSteamMatchmaking || buflen < 1)
            {
                writeLobbyEvent(fd, SHIMEVENT_LOBBY_CREATED, false, 0, "");
                break;
            }

            int maxPlayers = *(buf++);

            if (maxPlayers < 1)
            {
                maxPlayers = 1;
            }
            else if (maxPlayers > 250)
            {
                maxPlayers = 250;
            }

            SteamAPICall_t call = GSteamMatchmaking->CreateLobby(k_ELobbyTypePublic, maxPlayers);
            GSteamBridge->m_CallResultLobbyCreated.Set(call, GSteamBridge, &SteamBridge::OnLobbyCreated);
            break;
        }

        case SHIMCMD_LOBBY_LIST:
        {
            if (!GSteamMatchmaking)
            {
                writeLobbyEvent(fd, SHIMEVENT_LOBBY_LIST, false, 0, "");
                break;
            }

            GSteamMatchmaking->AddRequestLobbyListStringFilter("game", "wolftech", k_ELobbyComparisonEqual);
            GSteamMatchmaking->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);

            SteamAPICall_t call = GSteamMatchmaking->RequestLobbyList();
            GSteamBridge->m_CallResultLobbyMatchList.Set(call, GSteamBridge, &SteamBridge::OnLobbyMatchList);
            break;
        }

        case SHIMCMD_LOBBY_JOIN:
        {
            if (!GSteamMatchmaking || buflen < sizeof(uint64))
            {
                writeLobbyEvent(fd, SHIMEVENT_LOBBY_JOINED, false, 0, "");
                break;
            }

            uint64 lobbyID = 0;
            memcpy(&lobbyID, buf, sizeof(lobbyID));

            GSteamMatchmaking->JoinLobby(CSteamID(lobbyID));
            break;
        }

        case SHIMCMD_LOBBY_LEAVE:
        {
            if (GSteamMatchmaking && GCurrentLobby.IsValid())
            {
                GSteamMatchmaking->LeaveLobby(GCurrentLobby);
                GCurrentLobby.Clear();
            }

            break;
        }

        case SHIMCMD_LOBBY_SETDATA:
        {
            if (!GSteamMatchmaking || !GCurrentLobby.IsValid())
            {
                break;
            }

            const char *key = (const char *)buf;
            size_t keyLen = strlen(key) + 1;

            if (keyLen >= buflen)
            {
                break;
            }

            buf += keyLen;
            buflen -= (unsigned int)keyLen;

            const char *value = (const char *)buf;

            GSteamMatchmaking->SetLobbyData(GCurrentLobby, key, value);

            dbgpipe("Lobby set data: %s=%s\n", key, value);
            break;
        }

        case SHIMCMD_LOBBY_INVITE:
        {
            uint64 lobbyID = 0;

            if (!GSteamFriends || !GSteamMatchmaking)
            {
                writeLobbyEvent(fd, SHIMEVENT_LOBBY_INVITE, false, 0, "");
                break;
            }

            if (buflen >= sizeof(uint64))
            {
                memcpy(&lobbyID, buf, sizeof(lobbyID));
            }

            if (lobbyID == 0 && GCurrentLobby.IsValid())
            {
                lobbyID = GCurrentLobby.ConvertToUint64();
            }

            if (lobbyID == 0)
            {
                writeLobbyEvent(fd, SHIMEVENT_LOBBY_INVITE, false, 0, "");
                break;
            }

            GSteamFriends->ActivateGameOverlayInviteDialog(CSteamID(lobbyID));

            writeLobbyEvent(fd, SHIMEVENT_LOBBY_INVITE, true, lobbyID, "");
            break;
        }
        case SHIMCMD_NET_LISTEN:
        {
            if (!GSteamNetworkingSockets)
            {
                dbgpipe("SHIMCMD_NET_LISTEN: no ISteamNetworkingSockets.\n");
                break;
            }

            if (GP2PListenSocket != k_HSteamListenSocket_Invalid)
            {
                dbgpipe("SHIMCMD_NET_LISTEN: already listening.\n");
                break;
            }

            GP2PListenSocket = GSteamNetworkingSockets->CreateListenSocketP2P(0, 0, NULL);
            dbgpipe("SHIMCMD_NET_LISTEN: listen socket = %u\n", (unsigned int) GP2PListenSocket);
            break;
        }

        case SHIMCMD_NET_CONNECT:
        {
            uint64 steamID = 0;

            if (!GSteamNetworkingSockets || buflen < sizeof(uint64))
            {
                writeNetConnEvent(fd, SHIMEVENT_NET_DISCONNECTED, 0);
                break;
            }

            memcpy(&steamID, buf, sizeof(steamID));

            if (P2P_FindConnBySteamID(steamID) != k_HSteamNetConnection_Invalid)
            {
                dbgpipe("SHIMCMD_NET_CONNECT: already have a connection to %llu\n",
                    (unsigned long long) steamID);
                break;
            }

            SteamNetworkingIdentity identity;
            identity.SetSteamID64(steamID);

            const HSteamNetConnection conn = GSteamNetworkingSockets->ConnectP2P(identity, 0, 0, NULL);
            if (conn == k_HSteamNetConnection_Invalid)
            {
                dbgpipe("SHIMCMD_NET_CONNECT: ConnectP2P failed for %llu\n",
                    (unsigned long long) steamID);
                writeNetConnEvent(fd, SHIMEVENT_NET_DISCONNECTED, steamID);
            }
            // On success we don't report NET_CONNECTED yet - that happens
            // once OnNetConnectionStatusChanged sees k_ESteamNetworkingConnectionState_Connected.
            break;
        }

        case SHIMCMD_NET_SEND:
        {
            uint64 steamID = 0;

            if (!GSteamNetworkingSockets || buflen < sizeof(uint64))
                break;

            memcpy(&steamID, buf, sizeof(steamID));
            buf += sizeof(uint64);
            buflen -= (unsigned int) sizeof(uint64);

            const HSteamNetConnection conn = P2P_FindConnBySteamID(steamID);
            if (conn == k_HSteamNetConnection_Invalid)
                break;  // peer not connected (yet, or anymore) - drop, same as a lost UDP packet.

            GSteamNetworkingSockets->SendMessageToConnection(
                conn, buf, buflen, k_nSteamNetworkingSend_UnreliableNoNagle, NULL);
            break;
        }

        case SHIMCMD_NET_CLOSE:
        {
            uint64 steamID = 0;

            if (buflen >= sizeof(uint64))
                memcpy(&steamID, buf, sizeof(steamID));

            if (!GSteamNetworkingSockets)
                break;

            if (steamID == 0)
            {
                // Close everything, e.g. on shutdown.
                for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
                {
                    if (GP2PConns[i].inUse)
                    {
                        GSteamNetworkingSockets->CloseConnection(GP2PConns[i].conn, 0, NULL, false);
                        GP2PConns[i].inUse = false;
                    }
                }

                if (GP2PListenSocket != k_HSteamListenSocket_Invalid)
                {
                    GSteamNetworkingSockets->CloseListenSocket(GP2PListenSocket);
                    GP2PListenSocket = k_HSteamListenSocket_Invalid;
                }
            }
            else
            {
                const HSteamNetConnection conn = P2P_FindConnBySteamID(steamID);
                if (conn != k_HSteamNetConnection_Invalid)
                {
                    GSteamNetworkingSockets->CloseConnection(conn, 0, NULL, false);
                    P2P_UntrackByHandle(conn);
                }
            }
            break;
        }
    } // switch

    return true;  // keep going.
} // processCommand

/* Mirrors steamshim_child.c's parseShimHeader: buf[0]==SHIM_LEN_ESCAPE means
 the real length follows as a little-endian uint16 in buf[1..2], so
 SHIMCMD_NET_SEND can carry full game packets past the old 254-byte cap.
 Returns hdrlen==0 if we don't have enough bytes yet to know.
*/
static int parseShimCmdHeader(const uint8 *buf, const int br, int *outCmdlen)
{
    if (br < 1)
        return 0;

    if (buf[0] != SHIM_LEN_ESCAPE)
    {
        *outCmdlen = (int) buf[0];
        return 1;
    }

    if (br < 3)
        return 0;

    *outCmdlen = ((int) buf[1]) | (((int) buf[2]) << 8);
    return 3;
} // parseShimCmdHeader

// header(3) + cmd byte(1) + payload, largest command is SHIMCMD_NET_SEND.
static void processCommands(PipeType pipeParentRead, PipeType pipeParentWrite)
{
    bool quit = false;
    static uint8 buf[3 + 1 + SHIM_MAX_NET_PACKET];
    int br;

    // this read blocks.
    while (!quit && ((br = readPipe(pipeParentRead, buf, sizeof (buf))) > 0))
    {
        while (br > 0)
        {
            int cmdlen = 0;
            int hdrlen = parseShimCmdHeader(buf, br, &cmdlen);

            if (hdrlen && ((br - hdrlen) >= cmdlen))
            {
                if (!processCommand(buf+hdrlen, cmdlen, pipeParentWrite))
                {
                    quit = true;
                    break;
                } // if

                const int consumed = hdrlen + cmdlen;
                br -= consumed;
                if (br > 0)
                    memmove(buf, buf+consumed, br);
            } // if
            else  // get more data.
            {
                const int morebr = readPipe(pipeParentRead, buf+br, sizeof (buf) - br);
                if (morebr <= 0)
                {
                    quit = true;  // uhoh.
                    break;
                } // if
                br += morebr;
            } // else
        } // while
    } // while
} // processCommands

static bool setEnvironmentVars(PipeType pipeChildRead, PipeType pipeChildWrite)
{
    char buf[64];
    snprintf(buf, sizeof (buf), "%llu", (unsigned long long) pipeChildRead);
    if (!setEnvVar("STEAMSHIM_READHANDLE", buf))
        return false;

    snprintf(buf, sizeof (buf), "%llu", (unsigned long long) pipeChildWrite);
    if (!setEnvVar("STEAMSHIM_WRITEHANDLE", buf))
        return false;

    return true;
} // setEnvironmentVars

static bool initSteamworks(PipeType fd)
{
#ifdef RELEASE
    if (SteamAPI_RestartAppIfNecessary(steam_app_id))
    {
        exit(0);
    }
#endif

    // this can fail for many reasons:
    //  - you forgot a steam_appid.txt in the current working directory.
    //  - you don't have Steam running
    //  - you don't own the game listed in steam_appid.txt
    if (!SteamAPI_Init())
        return 0;

    GSteamStats = SteamUserStats();
    GSteamUtils = SteamUtils();
    GSteamUser = SteamUser();

    GAppID = GSteamUtils ? GSteamUtils->GetAppID() : 0;
	GUserID = GSteamUser ? GSteamUser->GetSteamID().ConvertToUint64() : 0;
    GSteamBridge = new SteamBridge(fd);

    SteamFriends()->SetRichPresence("steam_display", "#status_mainmenu");
    GSteamMatchmaking = SteamMatchmaking();
    GSteamNetworkingSockets = SteamNetworkingSockets();

    return 1;
} // initSteamworks

static void deinitSteamworks(void)
{
    if (GSteamNetworkingSockets)
    {
        for (int i = 0; i < MAX_P2P_CONNECTIONS; i++)
        {
            if (GP2PConns[i].inUse)
            {
                GSteamNetworkingSockets->CloseConnection(GP2PConns[i].conn, 0, NULL, false);
                GP2PConns[i].inUse = false;
            }
        }

        if (GP2PListenSocket != k_HSteamListenSocket_Invalid)
        {
            GSteamNetworkingSockets->CloseListenSocket(GP2PListenSocket);
            GP2PListenSocket = k_HSteamListenSocket_Invalid;
        }
    }

    SteamAPI_Shutdown();
    delete GSteamBridge;
    GSteamBridge = NULL;
    GSteamStats = NULL;
    GSteamUtils= NULL;
    GSteamUser = NULL;
    GSteamMatchmaking = NULL;
    GSteamNetworkingSockets = NULL;
    GCurrentLobby.Clear();
} // deinitSteamworks

static int mainline(void)
{
    PipeType pipeParentRead = NULLPIPE;
    PipeType pipeParentWrite = NULLPIPE;
    PipeType pipeChildRead = NULLPIPE;
    PipeType pipeChildWrite = NULLPIPE;
    ProcessType childPid;

    dbgpipe("Parent starting mainline.\n");

    if (!createPipes(&pipeParentRead, &pipeParentWrite, &pipeChildRead, &pipeChildWrite))
        fail("Failed to create application pipes");
    else if (!initSteamworks(pipeParentWrite))
        fail("Failed to initialize Steamworks");
    else if (!setEnvironmentVars(pipeChildRead, pipeChildWrite))
        fail("Failed to set environment variables");
    else if (!launchChild(&childPid))
        fail("Failed to launch application");

    // Close the ends of the pipes that the child will use; we don't need them.
    closePipe(pipeChildRead);
    closePipe(pipeChildWrite);
    pipeChildRead = pipeChildWrite = NULLPIPE;

    dbgpipe("Parent in command processing loop.\n");

    // Now, we block for instructions until the pipe fails (child closed it or
    //  terminated/crashed).
    processCommands(pipeParentRead, pipeParentWrite);

    dbgpipe("Parent shutting down.\n");

    // Close our ends of the pipes.
    writeBye(pipeParentWrite);
    closePipe(pipeParentRead);
    closePipe(pipeParentWrite);

    deinitSteamworks();

    dbgpipe("Parent waiting on child process.\n");

    // Wait for the child to terminate, close the child process handles.
    const int retval = closeProcess(&childPid);

    dbgpipe("Parent exiting mainline (child exit code %d).\n", retval);

    return retval;
} // mainline

// end of steamshim_parent.cpp ...

