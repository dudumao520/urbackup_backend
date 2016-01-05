/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011-2016 Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Affero General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "FileServ.h"
#include "map_buffer.h"
#include "../stringtools.h"
#include "../Interface/Server.h"
#include "CClientThread.h"
#include "../Interface/ThreadPool.h"
#include <algorithm>
#include "CUDPThread.h"
#include "PipeSessions.h"

IMutex *FileServ::mutex=NULL;
std::vector<std::string> FileServ::identities;
bool FileServ::pause=false;
std::map<std::string, std::string> FileServ::script_output_names;
IFileServ::ITokenCallbackFactory* FileServ::token_callback_factory = NULL;


FileServ::FileServ(bool *pDostop, const std::string &pServername, THREADPOOL_TICKET serverticket, bool use_fqdn)
	: servername(pServername), serverticket(serverticket)
{
	dostop=pDostop;
	if(servername.empty())
	{
		servername=getSystemServerName(use_fqdn);
	}
}

FileServ::~FileServ(void)
{
	delete dostop;
}

void FileServ::shareDir(const std::string &name, const std::string &path, const std::string& identity)
{
	add_share_path(name, path, identity);
}

void FileServ::removeDir(const std::string &name, const std::string& identity)
{
	remove_share_path(name, identity);
}

void FileServ::stopServer(void)
{
	*dostop=true;
	Server->getThreadPool()->waitFor(serverticket);
}

std::string FileServ::getShareDir(const std::string &name, const std::string& identity)
{
	return map_file(name, identity);
}

void FileServ::addIdentity(const std::string &pIdentity)
{
	IScopedLock lock(mutex);
	if(std::find(identities.begin(), identities.end(), pIdentity)
		== identities.end())
	{
		identities.push_back(pIdentity);
	}
}

void FileServ::init_mutex(void)
{
	mutex=Server->createMutex();
}

void FileServ::destroy_mutex(void)
{
	Server->destroy(mutex);
}

bool FileServ::checkIdentity(const std::string &pIdentity)
{
	IScopedLock lock(mutex);
	for(size_t i=0;i<identities.size();++i)
	{
		if(identities[i]==pIdentity)
		{
			return true;
		}
	}
	return false;
}

void FileServ::setPause(bool b)
{
	pause=b;
}

bool FileServ::getPause(void)
{
	return pause;
}

bool FileServ::isPause(void)
{
	return pause;
}

std::string FileServ::getServerName(void)
{
	return servername;
}

void FileServ::runClient(IPipe *cp, std::vector<char>* extra_buffer)
{
	CClientThread cc(cp, NULL, extra_buffer);
	cc();
}

bool FileServ::removeIdentity( const std::string &pIdentity )
{
	IScopedLock lock(mutex);
	std::vector<std::string>::iterator it = std::find(identities.begin(), identities.end(), pIdentity);
	if(it!=identities.end())
	{
		identities.erase(it);
		return true;
	}
	else
	{
		return false;
	}
}

bool FileServ::getExitInformation(const std::string& cmd, std::string& stderr_data, int& exit_code)
{
	SExitInformation exit_info = PipeSessions::getExitInformation(map_file(cmd, std::string()));

	if(exit_info.created==0)
	{
		return false;
	}
	else
	{
		stderr_data = exit_info.outdata;
		exit_code = exit_info.rc;

		return true;
	}
}

void FileServ::addScriptOutputFilenameMapping(const std::string& script_output_fn, const std::string& script_fn)
{
	IScopedLock lock(mutex);

	script_output_names[script_output_fn] = script_fn;
}

std::string FileServ::mapScriptOutputNameToScript(const std::string& script_fn)
{
	IScopedLock lock(mutex);

	std::map<std::string, std::string>::iterator it = script_output_names.find(script_fn);
	if(it!=script_output_names.end())
	{
		return it->second;
	}
	else
	{
		return script_fn;
	}
}

void FileServ::registerMetadataCallback( const std::string &name, const std::string& identity, IMetadataCallback* callback)
{
	PipeSessions::registerMetadataCallback(name, identity, callback);
}

void FileServ::removeMetadataCallback( const std::string &name, const std::string& identity )
{
	PipeSessions::removeMetadataCallback(name, identity);
}

void FileServ::registerTokenCallbackFactory( IFileServ::ITokenCallbackFactory* callback_factory )
{
	IScopedLock lock(mutex);

	token_callback_factory = callback_factory;
}

IFileServ::ITokenCallback* FileServ::newTokenCallback()
{
	IScopedLock lock(mutex);

	if(token_callback_factory==NULL)
	{
		return NULL;
	}

	return token_callback_factory->getTokenCallback();
}

bool FileServ::hasActiveMetadataTransfers(const std::string& sharename, const std::string& server_token)
{
	return PipeSessions::isShareActive(sharename, server_token);
}