#include <sdk_compat.h>
#include <paf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/io/fcntl.h>
extern "C" {
#include <ftpvita.h>
}

#include "ftp.h"

// Transfer progress: ftpvita reads and writes files through sceIoRead/sceIoWrite,
// and it is linked statically, so the link wraps those calls (-Wl,--wrap) and counts
// the bytes of the file the current command is about. Everything else passes through.

namespace {

	const char *const k_devices[] = {
		"ux0:", "ur0:", "uma0:", "imc0:", "xmc0:", "gro0:", "grw0:",
		"os0:", "pd0:", "sa0:", "tm0:", "ud0:", "vd0:", "vs0:",
	};

	void *s_netMemory = NULL;
	ftp::Status s_status;
	volatile bool s_running = false;
	volatile bool s_watching = false;   // a RETR/STOR is waiting for its file to open
	volatile SceUID s_fd = -1;          // the file of the current transfer

	void KeepAwake(void *)
	{
		// The console would sleep after its idle timer while a transfer runs.
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	}

	void Basename(const char *arg, char *out, size_t size)
	{
		const char *p = strrchr(arg, '/');
		p = p ? p + 1 : arg;
		size_t i = 0;
		while (p[i] && p[i] != '\r' && p[i] != '\n' && i < size - 1)
		{
			out[i] = p[i];
			i++;
		}
		out[i] = 0;
	}

	void Set(ftp::Activity activity, const char *arg)
	{
		s_status.activity = activity;
		if (arg)
		{
			Basename(arg, s_status.name, sizeof(s_status.name));
		}
		else
		{
			s_status.name[0] = 0;
		}
	}

	// ftpvita (vitasdk build) reports: "Client %i connected, IP: %s port: %i",
	// "\t%i> CMD arg" for every command, "Connection closed by the client %i.".
	void OnLog(const char *msg)
	{
		if (!msg)
		{
			return;
		}
		const char *ip = strstr(msg, "connected, IP: ");
		if (ip)
		{
			ip += 15;
			size_t n = 0;
			while (ip[n] && ip[n] != ' ' && n < sizeof(s_status.client) - 1)
			{
				s_status.client[n] = ip[n];
				n++;
			}
			s_status.client[n] = 0;
			s_status.clients++;
			Set(ftp::Activity_Connected, NULL);
			return;
		}
		if (strstr(msg, "Connection closed by the client") || strstr(msg, "socket aborted"))
		{
			if (s_status.clients > 0)
			{
				s_status.clients--;
			}
			if (s_status.clients == 0)
			{
				Set(ftp::Activity_Waiting, NULL);
			}
			return;
		}

		// command lines: skip "\t<n>> "
		const char *cmd = strstr(msg, "> ");
		if (!cmd)
		{
			return;
		}
		cmd += 2;
		if (strncmp(cmd, "RETR ", 5) == 0 || strncmp(cmd, "STOR ", 5) == 0)
		{
			Set(cmd[0] == 'R' ? ftp::Activity_Sending : ftp::Activity_Receiving, cmd + 5);
			s_status.done = 0;
			s_status.total = 0;
			s_fd = -1;
			s_watching = true;
		}
		else if (strncmp(cmd, "DELE ", 5) == 0)
		{
			Set(ftp::Activity_Deleting, cmd + 5);
		}
		else if (strncmp(cmd, "CWD ", 4) == 0)
		{
			Set(ftp::Activity_Folder, cmd + 4);
		}
		else if (strncmp(cmd, "LIST", 4) == 0 || strncmp(cmd, "NLST", 4) == 0 || strncmp(cmd, "MLSD", 4) == 0)
		{
			Set(ftp::Activity_Listing, NULL);
		}
		else if (strncmp(cmd, "MKD ", 4) == 0)
		{
			Set(ftp::Activity_MakeFolder, cmd + 4);
		}
		else if (strncmp(cmd, "RMD ", 4) == 0)
		{
			Set(ftp::Activity_RemoveFolder, cmd + 4);
		}
		else if (strncmp(cmd, "RNFR ", 5) == 0)
		{
			Set(ftp::Activity_Renaming, cmd + 5);
		}
		else if (s_status.activity == ftp::Activity_Waiting)
		{
			Set(ftp::Activity_Connected, NULL);
		}
	}

	bool EndsWithName(const char *path)
	{
		size_t n = strlen(s_status.name), p = strlen(path);
		return n && p >= n && strcmp(path + p - n, s_status.name) == 0;
	}

	int32_t InitNetwork()
	{
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED)
		{
			sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
		}

		if (sceNetShowNetstat() == SCE_NET_ERROR_ENOTINIT)
		{
			const int32_t size = 1024 * 1024;
			s_netMemory = malloc(size);
			SceNetInitParam param;
			param.memory = s_netMemory;
			param.size = size;
			param.flags = 0;
			int32_t ret = sceNetInit(&param);
			if (ret < 0)
			{
				return ret;
			}
		}

		sceNetCtlInit();

		int32_t state = 0;
		sceNetCtlInetGetState(&state);
		return state == SCE_NETCTL_STATE_CONNECTED ? 0 : SCE_NET_ERROR_ENOTCONN;
	}
}

extern "C" {

	SceUID __real_sceIoOpen(const char *file, int flags, SceMode mode);
	int __real_sceIoRead(SceUID fd, void *data, SceSize size);
	int __real_sceIoWrite(SceUID fd, const void *data, SceSize size);
	int __real_sceIoClose(SceUID fd);

	SceUID __wrap_sceIoOpen(const char *file, int flags, SceMode mode)
	{
		SceUID fd = __real_sceIoOpen(file, flags, mode);
		if (s_watching && fd >= 0 && file && EndsWithName(file))
		{
			s_watching = false;
			s_fd = fd;
			if (s_status.activity == ftp::Activity_Sending)
			{
				SceIoStat stat;
				if (sceIoGetstat(file, &stat) >= 0)
				{
					s_status.total = stat.st_size;
				}
			}
		}
		return fd;
	}

	int __wrap_sceIoRead(SceUID fd, void *data, SceSize size)
	{
		int ret = __real_sceIoRead(fd, data, size);
		if (fd == s_fd && ret > 0)
		{
			s_status.done += ret;
		}
		return ret;
	}

	int __wrap_sceIoWrite(SceUID fd, const void *data, SceSize size)
	{
		int ret = __real_sceIoWrite(fd, data, size);
		if (fd == s_fd && ret > 0)
		{
			s_status.done += ret;
		}
		return ret;
	}

	int __wrap_sceIoClose(SceUID fd)
	{
		if (fd == s_fd)
		{
			s_fd = -1;
			if (s_status.total)
			{
				s_status.done = s_status.total;
			}
		}
		return __real_sceIoClose(fd);
	}
}

int32_t ftp::Start()
{
	if (s_running)
	{
		return 0;
	}

	int32_t ret = InitNetwork();
	if (ret < 0)
	{
		return ret;
	}

	sce_paf_memset(&s_status, 0, sizeof(s_status));
	unsigned short int port = 0;
	ret = ftpvita_init(s_status.ip, &port);
	if (ret < 0)
	{
		return ret;
	}
	s_status.port = port;
	s_status.activity = Activity_Waiting;

	ftpvita_set_info_log_cb(OnLog);
	ftpvita_set_debug_log_cb(OnLog);
	for (uint32_t i = 0; i < sizeof(k_devices) / sizeof(k_devices[0]); i++)
	{
		SceIoStat stat;
		if (sceIoGetstat(k_devices[i], &stat) >= 0)
		{
			ftpvita_add_device(k_devices[i]);
		}
	}

	paf::common::MainThreadCallList::Register(KeepAwake, NULL);
	s_running = true;
	return 0;
}

void ftp::Stop()
{
	if (!s_running)
	{
		return;
	}
	paf::common::MainThreadCallList::Unregister(KeepAwake, NULL);
	ftpvita_fini();
	s_running = false;
	s_watching = false;
	s_fd = -1;
}

bool ftp::IsRunning()
{
	return s_running;
}

void ftp::GetStatus(Status *out)
{
	sce_paf_memcpy(out, &s_status, sizeof(s_status));
}
