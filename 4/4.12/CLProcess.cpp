#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "CLProcess.h"
#include "CLExecutiveFunctionProvider.h"
#include "CLLogger.h"

using namespace std;

#define LENGTH_OF_PROCESSID 25
#define LENGTH_OF_PATH 256

CLProcess::CLProcess(CLExecutiveFunctionProvider *pExecutiveFunctionProvider) : CLExecutive(pExecutiveFunctionProvider)
{
	m_bProcessCreated = false;
	m_bWaitForDeath = false;
	m_bExecSuccess = true;
}

CLProcess::CLProcess(CLExecutiveFunctionProvider *pExecutiveFunctionProvider, bool bWaitForDeath) : CLExecutive(pExecutiveFunctionProvider)
{
	m_bProcessCreated = false;
	m_bWaitForDeath = bWaitForDeath;
	m_bExecSuccess = true;
}

CLProcess::~CLProcess()
{
}

CLStatus CLProcess::Run(void *pstrCmdLine)
{
	if(m_bProcessCreated)
		return CLStatus(-1, 0);

	m_ProcessID = vfork();
	if(m_ProcessID == 0)
	{
		m_ProcessID = getpid();

		CloseFileDescriptor();

		m_pExecutiveFunctionProvider->RunExecutiveFunction(pstrCmdLine);

		m_bExecSuccess = false;

		_exit(0);
	}
	else if(m_ProcessID == -1)
	{
		CLLogger::WriteLogMsg("In CLProcess::Run(), vfork error", errno);
		delete this;
		return CLStatus(-1, 0);
	}
	else
	{
		if(!m_bExecSuccess)
		{
			waitpid(m_ProcessID, 0, 0);
			delete this;
			return CLStatus(-1, 0);
		}

		m_bProcessCreated = true;

		if(!m_bWaitForDeath)
			delete this;

		return CLStatus(0, 0);
	}
}

CLStatus CLProcess::WaitForDeath()
{
	if(!m_bWaitForDeath)
		return CLStatus(-1, 0);

	if(!m_bProcessCreated)
		return CLStatus(-1, 0);

	if(waitpid(m_ProcessID, 0, 0) == -1)
	{
		CLLogger::WriteLogMsg("In CLProcess::WaitForDeath(), waitpid error", errno);
		return CLStatus(-1, errno);
	}

	delete this;

	return CLStatus(0, 0);
}

CLStatus CLProcess::CloseFileDescriptor()
{
	string strPath = "/proc/";

	char id[LENGTH_OF_PROCESSID];
	snprintf(id, LENGTH_OF_PROCESSID, "%d", m_ProcessID);

	strPath += id;
	strPath += "/fd";

	string strPath1 = strPath;

	strPath += "/";

	DIR *pDir = opendir(strPath.c_str());
	if(pDir == 0)
	{
		CLLogger::WriteLogMsg("In CLProcess::CloseFileDescriptor(), opendir error", 0);
		return CLStatus(-1, 0);
	}

	while(struct dirent *pDirent = readdir(pDir))
	{
		char captial = pDirent->d_name[0];
		if((captial == '.') || (captial == '0') || (captial == '1') || (captial == '2'))
			continue;

		int fd = atoi(pDirent->d_name);
		if(fd != 0)
		{
			if(close(fd) == -1)
			{
				string errormsg = "In CLProcess::CloseFileDescriptor(), close error, file: ";
				errormsg += pDirent->d_name;
				CLLogger::WriteLogMsg(errormsg.c_str(), errno);
			}
		}
	}

	if(closedir(pDir) == -1)
	{
		CLLogger::WriteLogMsg("In CLProcess::CloseFileDescriptor(), closedir error", errno);
		return CLStatus(-1, 0);
	}

	return CLStatus(0, 0);
}