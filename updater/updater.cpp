// This file is part of CaesarIA.
//
// CaesarIA is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// CaesarIA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with CaesarIA.  If not, see <http://www.gnu.org/licenses/>.

#include "updater.hpp"

#include "releasefileset.hpp"
#include "http/mirrordownload.hpp"
#include "http/httprequest.hpp"
#include "constants.hpp"
#include "util.hpp"

#ifdef CAESARIA_PLATFORM_UNIX
  #include <limits.h>
  #include <unistd.h>
  #include <sys/stat.h>
#endif

namespace updater
{

Updater::Updater(const UpdaterOptions& options, vfs::Path executable) :
	_options(options),
	_downloadManager(new DownloadManager),
	_executable( executable ), // convert that file to lower to be sure
	_updatingUpdater(false)
{
	// Set up internet connectivity
	HttpConnectionPtr http( new HttpConnection() );
	http->drop();
	_conn = http;

	// Assign the proxy settings to the connection
	_options.CheckProxy(_conn);

	_ignoreList.insert("stable_info.txt");
	_ignoreList.insert("update_info.txt");
	_ignoreList.insert("mirrors.txt");

	MirrorDownload::InitRandomizer();

#ifdef CAESARIA_PLATFORM_WIN
	if( _executable.getExtension().empty() )
	{
		Logger::warning( "Adding EXE extension to executable: " + _executable.toString() );
		_executable = _executable.toString() + ".exe";
	}
#endif
}

void Updater::setBinaryAsExecutable()
{
#ifdef CAESARIA_PLATFORM_WIN
	vfs::Path executableName = "caesaria.exe";
#else
	vfs::Path executableName = "caesaria.linux";
#endif

	vfs::Path path2exe = getTargetDir()/executableName;
	if( path2exe.isExist() )
	{
		// set the executable bit on binary
		_markFileAsExecutable( path2exe );
	}
}

void Updater::_markFileAsExecutable(vfs::Path path )
{
#ifdef CAESARIA_PLATFORM_LINUX
	Logger::warning( "Marking file as executable: " + path.toString() );

	struct stat mask;
	stat(path.toString().c_str(), &mask);

	mask.st_mode |= S_IXUSR|S_IXGRP|S_IXOTH;

	if (chmod(path.toString().c_str(), mask.st_mode) == -1)
	{
		Logger::warning( "Could not mark file as executable: " + path.toString() );
	}
#endif
}

void Updater::CleanupPreviousSession()
{
	// Remove batch file from previous run
	vfs::NFile::remove( getTargetDir()/UPDATE_UPDATER_BATCH_FILE );
}

bool Updater::isMirrorsNeedUpdate()
{
	vfs::Directory folder = getTargetDir();
	vfs::Path mirrorPath = folder/CAESARIA_MIRRORS_INFO;

	if( !mirrorPath.isExist() )
	{
		// No mirrors file
		Logger::update( "No mirrors file present on this machine.");
		return true;
	}

	// File exists, check options
	if( _options.isSet("keep-mirrors") )
	{
		Logger::update( "Skipping mirrors update (--keep-mirrors is set)." );
		return false;
	}

	// Update by default
	return true;
}

void Updater::updateMirrors()
{
	std::string mirrorsUrl = CAESARIA_MAIN_SERVER;
	mirrorsUrl += CAESARIA_MIRRORS_INFO;

	//Logger::warning( StringHelper::format( 0xff, "Downloading mirror list from %s...", mirrorsUrl.c_str() ) ); // grayman - fixed

	vfs::Directory folder = getTargetDir();
	vfs::Path mirrorPath = folder/CAESARIA_MIRRORS_INFO;

	HttpRequestPtr request = _conn->createRequest( mirrorsUrl, mirrorPath.toString());

	request->Perform();

	if (request->GetStatus() == HttpRequest::OK)
	{
		// Load the mirrors from the file
		loadMirrors();
	}
	else
	{
		Logger::warning( " Mirrors download failed: %s", request->GetErrorMessage().c_str() );
	}
}

void Updater::loadMirrors()
{
	vfs::Directory folder = getTargetDir();
	vfs::Path mirrorPath = folder.getFilePath( CAESARIA_MIRRORS_INFO );

	// Load the tdm_mirrors.txt into an INI file
	IniFilePtr mirrorsIni = IniFile::ConstructFromFile(mirrorPath);

	// Interpret the info and build the mirror list
	_mirrors = MirrorList( mirrorsIni );

	//Logger::warning( "Found %d mirrors.", _mirrors.size() );
}

std::size_t Updater::GetNumMirrors()
{
	return _mirrors.size();
}

void Updater::GetStableVersionFromServer()
{
	PerformSingleMirroredDownload(STABLE_VERSION_FILE);

	// Parse this file
	vfs::Directory folder = getTargetDir();
	IniFilePtr releaseIni = IniFile::ConstructFromFile(folder.getFilePath( STABLE_VERSION_FILE ) );

	if (releaseIni == NULL)
	{
		throw FailureException("Could not download current version info file from server.");
	}

	// Build the release file set
	_latestRelease = ReleaseFileSet::LoadFromIniFile( releaseIni );
}

void Updater::GetVersionInfoFromServer()
{
	Logger::warning( " Downloading version information...");

	PerformSingleMirroredDownload(UPDATE_VERSION_FILE);

	// Parse this downloaded file
	vfs::Directory folder = getTargetDir();
	IniFilePtr versionInfo = IniFile::ConstructFromFile( folder.getFilePath( UPDATE_VERSION_FILE ) );

	if (versionInfo == NULL) 
	{
		Logger::warning( "Cannot find downloaded version info file: %s", folder.getFilePath(UPDATE_VERSION_FILE).toString().c_str() );
		return;
	}

	_releaseVersions.LoadFromIniFile( versionInfo );
	//_updatePackages.LoadFromIniFile( versionInfo );
}

void Updater::NotifyFileProgress(vfs::Path file, CurFileInfo::Operation op, double fraction)
{
	if (_fileProgressCallback != NULL)
	{
		CurFileInfo info;
		info.operation = op;
		info.file = file;
		info.progressFraction = fraction;

		_fileProgressCallback->OnFileOperationProgress(info);
	}
}

void Updater::DetermineLocalVersion()
{
	_pureLocalVersion.clear();
	_fileVersions.clear();
	_localVersions.clear();
	_applicableDifferentialUpdates.clear();

	Logger::warning( " Trying to determine installed CaesarIA version...");

	std::size_t totalItems = 0;

	// Get the total count of version information items, for calculating the progress
	for (ReleaseVersions::const_iterator v = _releaseVersions.begin(); v != _releaseVersions.end(); ++v)
	{
		totalItems += v->second.size();
	}

	std::size_t curItem = 0;

	for (ReleaseVersions::const_iterator v = _releaseVersions.begin(); v != _releaseVersions.end(); ++v)
	{
		Logger::warning( "Trying to match against version: %s", v->first.c_str() );

		const ReleaseFileSet& set = v->second;

		// Will be true on first mismatching file
		bool mismatch = false;

		for (ReleaseFileSet::const_iterator f = set.begin(); f != set.end(); ++f)
		{
			NotifyFileProgress(f->second.file, CurFileInfo::Check, static_cast<double>(curItem) / totalItems);

			curItem++;
			
			vfs::Directory folder = getTargetDir();
			vfs::Path candidate = folder.getFilePath( f->second.file );

			if( StringHelper::isEquale( candidate.getBasename().toString(), _executable.getBasename().toString(), StringHelper::equaleIgnoreCase ) )
			{
				Logger::warning( "Ignoring updater executable: %s.", candidate.toString().c_str() );
				continue;
			}

			if( !candidate.isExist() )
			{
				Logger::warning( "File %s is missing.", candidate.toString().c_str() );
				mismatch = true;
				continue;
			}

			if (f->second.localChangesAllowed) 
			{
				Logger::warning( "File %s exists, local changes are allowed, skipping.", candidate.toString().c_str() );
				continue;
			}

			std::size_t candidateFilesize = vfs::NFile::getSize( candidate );

			if (candidateFilesize != f->second.filesize)
			{
				Logger::warning( "File %s has mismatching size, expected %d but found %d.",
												 candidate.toString().c_str(),
												 f->second.filesize,
												 candidateFilesize );
				mismatch = true;
				continue;
			}

			// Calculate the CRC of this file
			unsigned int crc = CRC::GetCrcForFile( candidate );

			if (crc != f->second.crc)
			{
				Logger::warning( "File %s has mismatching CRC, expected %x but found %x.",
													candidate.toString().c_str(),
													f->second.crc,
													crc );
				mismatch = true;
				continue;
			}

			// The file is matching - record this version
			Logger::warning( "File %s is matching version %s.", candidate.toString().c_str(), v->first.c_str() );

			_fileVersions[candidate.toString()] = v->first;
		}

		// All files passed the check?
		if (!mismatch) 
		{
			_pureLocalVersion = v->first;
			Logger::warning( " Local installation matches version: " + _pureLocalVersion );
		}
	}

	// Sum up the totals for all files, each file has exactly one version
	for (FileVersionMap::const_iterator i = _fileVersions.begin(); i != _fileVersions.end(); ++i)
	{
		// sum up the totals for this version
		const std::string& version = i->second;
		VersionTotal& total = _localVersions.insert(LocalVersionBreakdown::value_type(version, VersionTotal())).first->second;

		total.numFiles++;
		total.filesize += vfs::NFile::getSize( i->first );
	}

	Logger::warning( "The local files are matching %d different versions.", _localVersions.size() );

	if (_fileProgressCallback != NULL)
	{
		_fileProgressCallback->OnFileOperationFinish();
	}

	if (_pureLocalVersion.empty())
	{
		Logger::warning( " Could not determine local version.");
	}

	if (!_localVersions.empty())
	{
		for (LocalVersionBreakdown::const_iterator i = _localVersions.begin(); i != _localVersions.end(); ++i)
		{
			const std::string& version = i->first;

			Logger::warning( "Files matching version %s: %d (size: %s)",
											version.c_str(),
											i->second.numFiles,
											Util::GetHumanReadableBytes(i->second.filesize).c_str() );

			// Check if this differential update is wise, from an economic point of view
			/*UpdatePackageInfo::const_iterator package = _updatePackages.find(version);

			if( package != _updatePackages.end())
			{
				Logger::warning( "Some files match version " + version + ", a differential update is available for that version.");

				UpdatePackageInfo::const_iterator package = _updatePackages.find(version);

				assert(package != _updatePackages.end());

				if (package->second.filesize < i->second.filesize)
				{
					Logger::warning( "The differential package size is smaller than the total size of files needing it, this is good.");
					_applicableDifferentialUpdates.insert(version);
				}
				else
				{
					Logger::warning( "The differential package size is larger than the total size of files needing it, will not download that.");
				}
			}
			else*/
			{
				Logger::warning( "Some files match version %s, but no differential update is available for that version.", version.c_str() );
			}
		}
	}
}

bool Updater::DifferentialUpdateAvailable()
{
	// Check applicable differential updates
	if (!_applicableDifferentialUpdates.empty())
	{
		return true;
	}

	Logger::warning( "No luck, differential updates don't seem to be applicable.");

	return false;
}

std::string Updater::GetNewestVersion()
{
	for (ReleaseVersions::reverse_iterator i = _releaseVersions.rbegin();
		 i != _releaseVersions.rend(); ++i)
	{
		return i->first;
	}

	return "";
}

std::string Updater::GetDeterminedLocalVersion()
{
	return _pureLocalVersion;
}

DownloadPtr Updater::PrepareMirroredDownload(const std::string& remoteFile)
{
	AssertMirrorsNotEmpty();

	vfs::Directory dir = getTargetDir();
	vfs::Path targetPath =  dir.getFilePath( remoteFile );

	// Remove target path first
	vfs::NFile::remove( targetPath );

	// Create a mirrored download
	DownloadPtr download(new MirrorDownload(_conn, _mirrors, remoteFile, targetPath));

	return download;
}

void Updater::PerformSingleMirroredDownload(const std::string& remoteFile)
{
	// Create a mirrored download
	DownloadPtr download = PrepareMirroredDownload(remoteFile);

	// Perform and wait for completion
	PerformSingleMirroredDownload(download);
}

void Updater::PerformSingleMirroredDownload(const std::string& remoteFile, std::size_t requiredSize, unsigned int requiredCrc)
{
	DownloadPtr download = PrepareMirroredDownload(remoteFile);

	download->EnableCrcCheck(true);
	download->EnableFilesizeCheck(true);
	download->SetRequiredCrc(requiredCrc);
	download->SetRequiredFilesize(requiredSize);

	// Perform and wait for completion
	PerformSingleMirroredDownload(download);
}

void Updater::PerformSingleMirroredDownload(const DownloadPtr& download)
{
	int downloadId = _downloadManager->AddDownload(download);

	while (_downloadManager->HasPendingDownloads())
	{
		//boost::this_thread::interruption_point();

		_downloadManager->ProcessDownloads();

		NotifyDownloadProgress();

		for (int i = 0; i < 50; ++i)
		{
			//boost::this_thread::interruption_point();
			Util::Wait(10);
		}
	}

	if( _downloadProgressCallback.isValid() )
	{
		_downloadProgressCallback->OnDownloadFinish();
	}

	_downloadManager->RemoveDownload(downloadId);
}

vfs::Directory Updater::getTargetDir()
{
	// Use the target directory 
	if (_options.isSet("targetdir") && !_options.Get("targetdir").empty())
	{
		return vfs::Path( _options.Get("targetdir") );
	}

	// Get the current path
	vfs::Path targetPath = vfs::Directory::getCurrent();

	return targetPath;
}

void Updater::CheckLocalFiles()
{
	_downloadQueue.clear();

	// Get the current path
	vfs::Directory targetDir = getTargetDir();

	Logger::warning( "Checking target folder: " + targetDir.toString() );

	std::size_t count = 0;
	for (ReleaseFileSet::const_iterator i = _latestRelease.begin(); i != _latestRelease.end(); ++i)
	{
		if (_fileProgressCallback != NULL)
		{
			CurFileInfo info;
			info.operation = CurFileInfo::Check;
			info.file = i->second.file;
			info.progressFraction = static_cast<double>(count) / _latestRelease.size();

			_fileProgressCallback->OnFileOperationProgress(info);
		}

		Logger::warning( "Checking for file: " + i->second.file.toString() + "...");
		if (!CheckLocalFile(targetDir, i->second))
		{
			// A member is missing or out of date, mark the archive for download
			_downloadQueue.insert(*i);
		}

		count++;
	}

	if (_fileProgressCallback != NULL)
	{
		_fileProgressCallback->OnFileOperationFinish();
	}

	if (NewUpdaterAvailable())
	{
		// Remove all download packages from the queue, except the one containing the updater
		RemoveAllPackagesExceptUpdater();
	}
}

bool Updater::CheckLocalFile(vfs::Path installPath, const ReleaseFile& releaseFile)
{
	//boost::this_thread::interruption_point();

	vfs::Path localFile = vfs::Directory( installPath ).getFilePath( releaseFile.file );

	Logger::warning( " Checking for file " + releaseFile.file.toString() + ": ");

	if( localFile.isExist() )
	{
		// File exists, check ignore list
		if (_ignoreList.find( StringHelper::localeLower( releaseFile.file.toString()) ) != _ignoreList.end())
		{
			Logger::warning( "OK, file will not be updated. ");
			return true; // ignore this file
		}

		// Compare file size
		std::size_t fileSize = vfs::NFile::getSize( localFile );

		if (fileSize != releaseFile.filesize)
		{
			Logger::warning( "SIZE MISMATCH" );
			return false;
		}

		// Size is matching, check CRC
		unsigned int existingCrc = CRC::GetCrcForFile(localFile);

		if (existingCrc == releaseFile.crc)
		{
			Logger::warning( "OK");
			return true;
		}
		else
		{
			Logger::warning( "CRC MISMATCH");
			return false;
		}
	}
	else
	{
		Logger::warning( "MISSING");
		return false;
	}
}

bool Updater::LocalFilesNeedUpdate()
{
	return !_downloadQueue.empty();
}

void Updater::PrepareUpdateStep(std::string prefix)
{
	vfs::Directory targetdir = getTargetDir();

	// Create a download for each of the files
    for( ReleaseFileSet::iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); ++i )
	{
		DownloadPtr download(new MirrorDownload(_conn, _mirrors, i->second.file.toString(), targetdir.getFilePath(prefix+i->second.file.toString() ) )) ;
        download->EnableCrcCheck( true );
        download->EnableFilesizeCheck( true );
        download->SetRequiredCrc( i->second.crc );
        download->SetRequiredFilesize( i->second.filesize );
		i->second.downloadId = _downloadManager->AddDownload(download);
	}
}

void Updater::PerformUpdateStep()
{
	// Wait until the download is done
	while (_downloadManager->HasPendingDownloads())
	{
		// For catching terminations
		//boost::this_thread::interruption_point();

		_downloadManager->ProcessDownloads();

		if (_downloadManager->HasFailedDownloads())
		{
			throw FailureException("Could not download from any mirror.");
		}

		NotifyDownloadProgress();

		NotifyFullUpdateProgress();

		Util::Wait(100);
	}

	if (_downloadManager->HasFailedDownloads())
	{
		throw FailureException("Could not download from any mirror.");
	}

	if (_downloadProgressCallback.isValid())
	{
		_downloadProgressCallback->OnDownloadFinish();
	}
}

void Updater::NotifyFullUpdateProgress()
{
	if (_downloadProgressCallback == NULL)
	{
		return;
	}

	std::size_t totalDownloadSize = GetTotalDownloadSize();
	std::size_t totalBytesDownloaded = 0;

	for (ReleaseFileSet::iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); ++i)
	{
		//boost::this_thread::interruption_point();

		if (i->second.downloadId == -1)
		{
			continue;
		}

		DownloadPtr download = _downloadManager->GetDownload(i->second.downloadId);

		if (download == NULL) continue;

		if (download->GetStatus() == Download::SUCCESS)
		{
			totalBytesDownloaded += i->second.filesize;
		}
		else if (download->GetStatus() == Download::IN_PROGRESS)
		{
			totalBytesDownloaded += download->GetDownloadedBytes();
		}
	}

	OverallDownloadProgressInfo info;

	if (totalBytesDownloaded > totalDownloadSize)
	{
		totalBytesDownloaded = totalDownloadSize;
	}

	info.updateType = OverallDownloadProgressInfo::Full;
	info.totalDownloadSize = totalDownloadSize;
	info.bytesLeftToDownload = totalDownloadSize - totalBytesDownloaded;
	info.downloadedBytes = totalBytesDownloaded;
	info.progressFraction = static_cast<double>(totalBytesDownloaded) / totalDownloadSize;
	info.filesToDownload = _downloadQueue.size();
	
	_downloadProgressCallback->OnOverallProgress(info);
}

void Updater::NotifyDownloadProgress()
{
	int curDownloadId = _downloadManager->GetCurrentDownloadId();
	
	if(curDownloadId != -1 && _downloadProgressCallback.isValid())
	{
		DownloadPtr curDownload = _downloadManager->GetCurrentDownload();

		CurDownloadInfo info;

		info.file = curDownload->GetFilename();
		info.progressFraction = curDownload->GetProgressFraction();
		info.downloadSpeed = curDownload->GetDownloadSpeed();
		info.downloadedBytes = curDownload->GetDownloadedBytes();

		MirrorDownloadPtr mirrorDownload = curDownload.as<MirrorDownload>();

		if (mirrorDownload != NULL)
		{
			info.mirrorDisplayName = mirrorDownload->GetCurrentMirrorName();
		}

		_downloadProgressCallback->OnDownloadProgress(info);
	}
}

void Updater::PrepareUpdateBatchFile()
{
	vfs::Path temporaryUpdater = TEMP_FILE_PREFIX + _executable.toString();
	// Create a new batch file in the target location
	vfs::Directory targetdir = getTargetDir();
	_updateBatchFile =  targetdir.getFilePath( UPDATE_UPDATER_BATCH_FILE );

	Logger::warning( "Preparing CaesarIA update batch file in " + _updateBatchFile.toString() );

	std::ofstream batch(_updateBatchFile.toString().c_str());

	vfs::Path tempUpdater = temporaryUpdater.getBasename();
	vfs::Path updater = _executable.getBasename();

	// Append the current set of command line arguments to the new instance
	std::string arguments;

	for (std::vector<std::string>::const_iterator i = _options.GetRawCmdLineArgs().begin();
		 i != _options.GetRawCmdLineArgs().end(); ++i)
	{
		arguments += " " + *i;
	}

#ifdef CAESARIA_PLATFORM_WIN
	batch << "@ping 127.0.0.1 -n 2 -w 1000 > nul" << std::endl; // # hack equivalent to Wait 2
	batch << "@copy " << tempUpdater.toString() << " " << updater.toString() << " >nul" << std::endl;
	batch << "@del " << tempUpdater.toString() << std::endl;
	batch << "@echo CaesarIA Updater executable has been updated." << std::endl;

	batch << "@echo Re-launching CaesarIA Updater executable." << std::endl << std::endl;

	batch << "@start " << updater.toString() << " " << arguments;
#else // POSIX
	// grayman - accomodate spaces in pathnames
	tempUpdater = targetdir.getFilePath( tempUpdater );
	updater = targetdir.getFilePath( updater );

	batch << "#!/bin/bash" << std::endl;
	batch << "echo \"Upgrading CaesarIA Updater executable...\"" << std::endl;
	batch << "cd \"" << getTargetDir().toString() << "\"" << std::endl;
	batch << "sleep 2s" << std::endl;
	batch << "mv -f \"" << tempUpdater.toString() << "\" \"" << updater.toString() << "\"" << std::endl;
	batch << "chmod +x \"" << updater.toString() << "\"" << std::endl;
	batch << "echo \"CaesarIA Updater executable has been updated.\"" << std::endl;
	batch << "echo \"Re-launching CaesarIA Updater executable.\"" << std::endl;

	batch << "\"" << updater.toString() << "\" " << arguments;
#endif

	batch.close();

#ifdef CAESARIA_PLATFORM_UNIX
	// Mark the shell script as executable in *nix
	_markFileAsExecutable(_updateBatchFile);
#endif
}

void Updater::CleanupUpdateStep()
{
	for (ReleaseFileSet::iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); ++i)
	{
		_downloadManager->RemoveDownload(i->second.downloadId);
	}

	_downloadQueue.clear();
}

void Updater::AssertMirrorsNotEmpty()
{
	if (_mirrors.empty())
	{
		throw FailureException("No mirror information, cannot continue.");
	}
}

std::size_t Updater::GetNumFilesToBeUpdated()
{
	return _downloadQueue.size();
}

std::size_t Updater::GetTotalDownloadSize()
{
	std::size_t totalSize = 0;

	for (ReleaseFileSet::iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); ++i)
	{
		totalSize += i->second.filesize;
	}

	return totalSize;
}

std::size_t Updater::GetTotalBytesDownloaded()
{
	return _conn->GetBytesDownloaded();
}

void Updater::SetDownloadProgressCallback(DownloadProgressPtr callback)
{
	_downloadProgressCallback = callback;
}

void Updater::ClearDownloadProgressCallback()
{
	_downloadProgressCallback = DownloadProgressPtr();
}

void Updater::SetFileOperationProgressCallback(FileOperationProgressPtr callback)
{
	_fileProgressCallback = callback;
}

void Updater::ClearFileOperationProgressCallback()
{
	_fileProgressCallback = FileOperationProgressPtr();
}

bool Updater::NewUpdaterAvailable()
{
	if(_options.isSet("noselfupdate"))
	{
		return false; // no self-update overrides everything
	}

	Logger::warning( "Looking for executable " + _executable.toString() + " in download queue.");

	vfs::Path myPath = vfs::Directory::getApplicationDir()/_executable;
	ByteArray crcData = vfs::NFile::open( myPath ).readAll();
	unsigned int fileSize = vfs::NFile::getSize( myPath );

	unsigned int crc = crcData.crc32( 0 );
	
	// Is this the updater?
	for (ReleaseFileSet::const_iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); ++i)
	{
		if( i->second.isUpdater(_executable.toString() ) )
		{
			if( i->second.crc != crc && i->second.filesize != fileSize  )
			{
				Logger::warning( "The updater binary needs to be updated.");
				return true;
			}

			return false;
		}
	}

	Logger::warning( "Didn't find executable name " + _executable.toString() + " in download queue.");

	return false;
}

void Updater::RemoveAllPackagesExceptUpdater()
{
	Logger::warning("Removing all packages, except the one containing the updater");

	for( ReleaseFileSet::iterator i = _downloadQueue.begin(); i != _downloadQueue.end(); /* in-loop */ )
	{
		if (i->second.isUpdater(_executable.toString()))
		{
			// This package contains the updater, keep it
			++i;
		}
		else
		{
			// The inner loop didn't find the executable, remove that package
			_downloadQueue.erase(i++);
		}
	}
}

void Updater::RestartUpdater()
{
	Logger::warning( "Preparing restart...");	
	PrepareUpdateBatchFile();

#ifdef CAESARIA_PLATFORM_WIN
	if (!_updateBatchFile.toString().empty())
	{
		Logger::warning( "Update batch file pending, launching process.");
		
		// Spawn a new process

		// Create a tdmlauncher process, setting the working directory to the target directory
		STARTUPINFOA siStartupInfo;
		PROCESS_INFORMATION piProcessInfo;

		memset(&siStartupInfo, 0, sizeof(siStartupInfo));
		memset(&piProcessInfo, 0, sizeof(piProcessInfo));

		siStartupInfo.cb = sizeof(siStartupInfo);

		vfs::Directory parentPath = _updateBatchFile.getDir();

		Logger::warning( "Starting batch file " + _updateBatchFile.toString() + " in " + parentPath.toString() );

		BOOL success = CreateProcessA( NULL, (LPSTR) _updateBatchFile.toString().c_str(), NULL, NULL,  false, 0, NULL,
																	 parentPath.toString().c_str(), &siStartupInfo, &piProcessInfo);

		if (!success)
		{
			LPVOID lpMsgBuf;

			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						  NULL,
						  GetLastError(),
						  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						  (LPTSTR) &lpMsgBuf,
						  0,
						  NULL);

			throw FailureException( "Could not start new process: " + std::string((LPCSTR)lpMsgBuf));
			
			LocalFree(lpMsgBuf);
		}
		else
		{
			Logger::warning( "Process started");
            exit(0);
		}
	}
#else

	if (!_updateBatchFile.toString().empty())
	{
		Logger::warning( "Relaunching CaesarIA updater via shell script " + _updateBatchFile.toString() );

		// Perform the system command in a fork
		//int r = fork();
		//if( r >= 0 )
		{
			// Don't wait for the subprocess to finish
			system((_updateBatchFile.toString() + " &").c_str());
			exit(EXIT_SUCCESS);
			return;
		}

		Logger::warning( "Process spawned.");

		// Done here too
		return;
	}
#endif
}

void Updater::PostUpdateCleanup()
{
	vfs::Directory pdir =  getTargetDir();
	vfs::Entries dir = pdir.getEntries();
	for( vfs::Entries::ConstItemIt i = dir.begin(); i != dir.end(); i++)
	{
		if( StringHelper::startsWith( i->name.toString(), TEMP_FILE_PREFIX) )
		{
			vfs::Path p = i->fullName;
			vfs::NFile::remove( p );
		}		
	}

	// grayman #3514 - Remove DLL file in case the user is updating an existing installation.
	// Also remove leftover updater file.
}

void Updater::CancelDownloads()
{
	_downloadManager->ClearDownloads();
}

} // namespace
