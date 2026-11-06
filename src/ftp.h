#ifndef _FM_FTP_H_
#define _FM_FTP_H_

#include <sdk_compat.h>
#include <paf.h>

// FTP server (ftpvita, as in FMVita), shared over every mounted device.
namespace ftp {

	enum Activity
	{
		Activity_Waiting,     // server up, no command yet
		Activity_Connected,   // client logged in
		Activity_Sending,     // RETR: the client downloads from the Vita
		Activity_Receiving,   // STOR: the client uploads to the Vita
		Activity_Deleting,
		Activity_Listing,
		Activity_Folder,
		Activity_MakeFolder,
		Activity_RemoveFolder,
		Activity_Renaming,
		Activity_Sent,        // a download from the Vita finished
		Activity_Received,    // an upload to the Vita finished
		Activity_Deleted
	};

	struct Status
	{
		char ip[16];
		uint32_t port;
		Activity activity;
		char name[256];       // file or folder the activity is about
		uint64_t done;        // bytes moved by the current transfer
		uint64_t total;       // file size when known (downloads), 0 otherwise
		char client[16];      // IP of the last client that connected
		int32_t clients;
	};

	/** Starts the server. Fails with a negative error, for example without Wi-Fi. */
	int32_t Start();
	int32_t EnsureNetwork();

	void Stop();

	bool IsRunning();

	/** Snapshot of what the server is doing, for the status page. */
	void GetStatus(Status *out);
}

#endif
