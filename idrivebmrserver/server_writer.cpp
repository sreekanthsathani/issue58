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
#include "server_writer.h"
#include "../Interface/Mutex.h"
#include "../Interface/Condition.h"
#include "../Interface/Server.h"
#include "../fsimageplugin/IVHDFile.h"
#include "../fsimageplugin/IFSImageFactory.h"
#include "../stringtools.h"
#include "../idrivebmrcommon/os_functions.h"
#include "server_log.h"
#include "server_cleanup.h"
#include "ClientMain.h"
#include "zero_hash.h"

extern IFSImageFactory *image_fak;
const size_t free_space_lim=1000*1024*1024; //1000MB
const uint64 filebuf_lim=1000*1024*1024; //1000MB
const unsigned int sha_size=32;

ServerVHDWriter::ServerVHDWriter(IVHDFile *pVHD, unsigned int blocksize, unsigned int nbufs,
		int pClientid, bool use_tmpfiles, int64 mbr_offset, IFile* hashfile, int64 vhd_blocksize,
	logid_t logid, int64 drivesize)
 : mbr_offset(mbr_offset), do_trim(false), hashfile(hashfile), vhd_blocksize(vhd_blocksize), do_make_full(false),
   logid(logid), drivesize(drivesize)
{
	filebuffer=use_tmpfiles;

	clientid=pClientid;
	vhd=pVHD;
	if(filebuffer)
	{
		bufmgr=new CBufMgr2(nbufs, sizeof(FileBufferVHDItem)+blocksize);
	}
	else
	{
		bufmgr=new CBufMgr2(nbufs, blocksize);
	}

	if(filebuffer)
	{
		filebuf=new CFileBufMgr(false);
		filebuf_writer=new ServerFileBufferWriter(this, blocksize);
		filebuf_writer_ticket=Server->getThreadPool()->execute(filebuf_writer, "image buffered writer");
		currfile=filebuf->getBuffer();
		currfile_size=0;
	}

	mutex=Server->createMutex();
	vhd_mutex=Server->createMutex();
	cond=Server->createCondition();
	exit=false;
	exit_now=false;
	has_error=false;
	written=free_space_lim;
}

ServerVHDWriter::~ServerVHDWriter(void)
{
	delete bufmgr;

	if(filebuffer)
	{
		delete filebuf_writer;
		delete filebuf;
	}

	Server->destroy(mutex);
	Server->destroy(vhd_mutex);
	Server->destroy(cond);
}

void ServerVHDWriter::operator()(void)
{
	{
		while(!exit_now)
		{
			BufferVHDItem item;
			bool has_item=false;
			bool do_exit;
			{
				IScopedLock lock(mutex);
				if(tqueue.empty() && exit==false)
				{
					cond->wait(&lock);
				}
				do_exit=exit;
				if(!tqueue.empty())
				{
					item=tqueue.front();
					tqueue.pop();
					has_item=true;
				}
			}
			if(has_item)
			{
				if(!has_error)
				{
					if(!filebuffer)
					{
						writeVHD(item.pos, item.buf, item.bsize);
					}
					else
					{
						FileBufferVHDItem *fbi;
						FileBufferVHDItem local_fbi;
						if (item.buf != NULL)
						{
							fbi = (FileBufferVHDItem*)(item.buf - sizeof(FileBufferVHDItem));
							fbi->type = 0;
						}
						else
						{
							fbi = &local_fbi;
							fbi->type = 1;
						}
							
						fbi->pos=item.pos;
						fbi->bsize = item.bsize;
						if (item.buf != NULL)
						{
							writeRetry(currfile, (char*)fbi, sizeof(FileBufferVHDItem) + item.bsize);
							currfile_size += item.bsize + sizeof(FileBufferVHDItem);
						}
						else
						{
							writeRetry(currfile, (char*)fbi, sizeof(FileBufferVHDItem));
							currfile_size += sizeof(FileBufferVHDItem);
						}
						

						if(currfile_size>filebuf_lim)
						{
							filebuf_writer->writeBuffer(currfile);
							currfile=filebuf->getBuffer();
							currfile_size=0;
						}
					}
				}

				freeBuffer(item.buf);
			}
			else if(do_exit)
			{
				break;
			}

			if(!filebuffer && written>=free_space_lim/2)
			{
				written=0;
				checkFreeSpaceAndCleanup();
			}
		}
	}
	if(filebuffer)
	{
		filebuf_writer->writeBuffer(currfile);

		if(!exit_now)
			filebuf_writer->doExit();
		else
			filebuf_writer->doExitNow();

		Server->getThreadPool()->waitFor(filebuf_writer_ticket);
	}

	if(do_trim)
	{
		trimmed_bytes=0;
		ServerLogger::Log(logid, "Starting trimming image file (if possible)", LL_DEBUG);
		if(!vhd->trimUnused(mbr_offset, vhd_blocksize, this))
		{
			ServerLogger::Log(logid, "Trimming file failed. Image backup may use too much storage.", LL_DEBUG);
		}
		else
		{
			ServerLogger::Log(logid, "Trimmed "+PrettyPrintBytes(trimmed_bytes), LL_DEBUG);
		}
	}

	if(do_make_full)
	{
		ServerLogger::Log(logid, "Converting incremental image file to full image file...", LL_INFO);

		if(!vhd->makeFull(mbr_offset, this))
		{
			ServerLogger::Log(logid, "Covnerting incremental image to full image failed", LL_ERROR);
		}
	}

	if(!vhd->finish())
	{
		checkFreeSpaceAndCleanup();
		if(!vhd->finish())
		{
			ServerLogger::Log(logid, "FATAL: Writing failed after cleanup", LL_ERROR);
			has_error=true;
		}
	}

	image_fak->destroyVHDFile(vhd);
}

void ServerVHDWriter::checkFreeSpaceAndCleanup(void)
{
	std::string p;
	{
		IScopedLock lock(vhd_mutex);
		p=ExtractFilePath(vhd->getFilename());
	}
	int64 fs=os_free_space(os_file_prefix(p));
	if(fs!=-1 && fs <= free_space_lim )
	{
		Server->Log("Not enough free space. Waiting for cleanup...");
		if(!cleanupSpace())
		{
			Server->Log("Not enough free space.", LL_WARNING);
		}
	}
}

bool ServerVHDWriter::writeVHD(uint64 pos, char *buf, unsigned int bsize)
{
	IScopedLock lock(vhd_mutex);

	if (buf == NULL)
	{
		int64 unused_end = pos + bsize;
		if (pos<drivesize && unused_end>drivesize)
		{
			Server->Log("Trim beyond drivesize (drivesize: " + convert(drivesize) + " trim to " + convert(pos + bsize)+"). Trimming less...", LL_INFO);
			unused_end = drivesize;
		}

		if (!vhd->setUnused(pos, unused_end))
		{
			ServerLogger::Log(logid, "Error setting unused area (from byte "+convert(pos)+" to byte "+convert(pos + bsize)+"). "+os_last_error_str(), LL_ERROR);
			has_error = true;
			return false;
		}
		else
		{
			return true;
		}
	}

	vhd->Seek(pos);
	bool b=vhd->Write(buf, bsize)!=0;
	written+=bsize;
	if(!b)
	{
		std::string errstr;
		int64 errcode = 0;
		int retry=3;
		for(int i=0;i<retry;++i)
		{
			Server->wait(100);
			Server->Log("Retrying writing to VHD file...");
			vhd->Seek(pos);
			if(vhd->Write(buf, bsize)==0)
			{
				errstr = os_last_error_str();
				errcode = os_last_error();
				Server->Log("Writing to VHD file failed");
			}
			else
			{
				return true;
			}
		}

		std::string p=ExtractFilePath(vhd->getFilename());
		int64 fs=os_free_space(os_file_prefix(p));
		if(fs!=-1 && fs <= free_space_lim )
		{
			Server->Log("Not enough free space. Waiting for cleanup...");
			if(cleanupSpace())
			{
				vhd->Seek(pos);
				if(vhd->Write(buf, bsize)==0)
				{
					retry=3;
					for(int i=0;i<retry;++i)
					{
						Server->wait(100);
						Server->Log("Retrying writing to VHD file...");
						vhd->Seek(pos);
						if(vhd->Write(buf, bsize)==0)
						{
							Server->Log("Writing to VHD file failed");
						}
						else
						{
							return true;
						}
					}

#ifdef _WIN32
					if(GetLastError()==ERROR_FILE_SYSTEM_LIMITATION)
					{
						ServerLogger::Log(logid, "FATAL: The filesystem is returning the error code ERROR_FILE_SYSTEM_LIMITATION."
							" This may be caused by the file being too fragmented (try defragmenting or freeing space). This can also be caused by the file being compressed and too large. In this case you have to disable NTFS compression."
							" See https://support.microsoft.com/kb/2891967 and https://support.microsoft.com/en-us/kb/967351 for details and instructions to fix this issue.", LL_ERROR);
					}
#endif

					ServerLogger::Log(logid, "FATAL: Writing failed after cleanup. "+os_last_error_str(), LL_ERROR);
					
					ClientMain::sendMailToAdmins("Fatal error occurred during image backup", ServerLogger::getWarningLevelTextLogdata(logid));
					has_error=true;
				}
			}
			else
			{
				has_error=true;
				ServerLogger::Log(logid, "FATAL: NOT ENOUGH free space. Cleanup failed.", LL_ERROR);
				ClientMain::sendMailToAdmins("Fatal error occurred during image backup", ServerLogger::getWarningLevelTextLogdata(logid));
			}
		}
		else
		{			
#ifdef _WIN32
			if (errcode == ERROR_FILE_SYSTEM_LIMITATION)
			{
				ServerLogger::Log(logid, "FATAL: The filesystem is returning the error code ERROR_FILE_SYSTEM_LIMITATION."
					" This may be caused by the file being too fragmented (try defragmenting or freeing space). This can also be caused by the file being compressed and too large. In this case you have to disable NTFS compression."
					" See https://support.microsoft.com/kb/2891967 and https://support.microsoft.com/en-us/kb/967351 for details and instructions to fix this issue.", LL_ERROR);
			}
#endif
			has_error=true;
			ServerLogger::Log(logid, "FATAL: Error writing to VHD-File. "+ errstr, LL_ERROR);
			ClientMain::sendMailToAdmins("Fatal error occurred during image backup", ServerLogger::getWarningLevelTextLogdata(logid));
		}
	}

	return !has_error;
}

char *ServerVHDWriter::getBuffer(void)
{
	if(filebuffer)
	{
		char *buf=bufmgr->getBuffer();
		if(buf!=NULL)
		{
			return buf+sizeof(FileBufferVHDItem);
		}
		else
		{
			return buf;
		}
	}
	else
	{
		return bufmgr->getBuffer();
	}
}

void ServerVHDWriter::writeBuffer(uint64 pos, char *buf, unsigned int bsize)
{
	IScopedLock lock(mutex);
	BufferVHDItem item;
	item.pos=pos;
	item.buf=buf;
	item.bsize=bsize;
	tqueue.push(item);
	cond->notify_all();
}

void ServerVHDWriter::freeBuffer(char *buf)
{
	if(buf==NULL)
		return;

	if(filebuffer)
		bufmgr->releaseBuffer(buf-sizeof(FileBufferVHDItem));
	else
		bufmgr->releaseBuffer(buf);
}

void ServerVHDWriter::doExit(void)
{
	IScopedLock lock(mutex);
	exit=true;
	finish=true;
	cond->notify_all();
}

void ServerVHDWriter::doExitNow(void)
{
	IScopedLock lock(mutex);
	exit=true;
	exit_now=true;
	finish=true;
	cond->notify_all();
}

void ServerVHDWriter::doFinish(void)
{
	IScopedLock lock(mutex);
	finish=true;
	cond->notify_all();
}

size_t ServerVHDWriter::getQueueSize(void)
{
	IScopedLock lock(mutex);
	return tqueue.size();
}

bool ServerVHDWriter::hasError(void)
{
	return has_error;
}

void ServerVHDWriter::setHasError(bool b)
{
	has_error=b;
}

IMutex * ServerVHDWriter::getVHDMutex(void)
{
	return vhd_mutex;
}

IVHDFile* ServerVHDWriter::getVHD(void)
{
	return vhd;
}

bool ServerVHDWriter::cleanupSpace(void)
{
	ServerLogger::Log(logid, "Not enough free space. Cleaning up.", LL_INFO);
	if(!ServerCleanupThread::cleanupSpace(free_space_lim) )
	{
		ServerLogger::Log(logid, "Could not free space for image. NOT ENOUGH FREE SPACE.", LL_ERROR);
		return false;
	}
	return true;
}

void ServerVHDWriter::freeFile(IFile *buf)
{
	filebuf->releaseBuffer(buf);
}

void ServerVHDWriter::writeRetry(IFile *f, char *buf, unsigned int bsize)
{
	unsigned int off=0;
	while( off<bsize && !has_error)
	{
		unsigned int r=f->Write(buf+off, bsize-off);
		off+=r;
		if(off<bsize)
		{
			Server->Log("Error writing to file \""+f->getFilename()+"\". "+os_last_error_str()+". Retrying...", LL_WARNING);
			Server->wait(10000);
		}
	}
}

void ServerVHDWriter::setDoTrim(bool b)
{
	do_trim = b;
}

void ServerVHDWriter::setMbrOffset(int64 offset)
{
	mbr_offset = offset;
}

void ServerVHDWriter::trimmed(_i64 trim_start, _i64 trim_stop)
{
	/*
	* If we do not align trimming to the hash block size
	* we need to recalculate the hash here.
	* Trimming only at hash block size for now.
	*/
	assert(trim_start%vhd_blocksize == 0);
	assert(trim_stop%vhd_blocksize == 0
			|| trim_stop == vhd->getSize() - mbr_offset);

	_i64 block_start = trim_start/vhd_blocksize;
	if(trim_start%vhd_blocksize!=0)
	{
		++block_start;
	}

	_i64 block_end = trim_stop/vhd_blocksize;

	for(;block_start<block_end;++block_start)
	{
		if(hashfile->Seek(block_start*sha_size) )
		{
			if(hashfile->Write(reinterpret_cast<const char*>(zero_hash), sha_size)!=sha_size)
			{
				Server->Log("Error writing to hashfile while trimming.", LL_WARNING);
			}
		}
		else
		{
			Server->Log("Error seeking in hashfile while trimming.", LL_WARNING);
		}
	}

	trimmed_bytes+=trim_stop-trim_start;
}

bool ServerVHDWriter::emptyVHDBlock(int64 empty_start, int64 empty_end)
{
	assert(empty_start%vhd_blocksize == 0);
	assert(empty_end%vhd_blocksize == 0);

	_i64 block_start = empty_start / vhd_blocksize;
	if (empty_start%vhd_blocksize != 0)
	{
		++block_start;
	}

	_i64 block_end = empty_end / vhd_blocksize;

	bool ret = true;
	for (; block_start<block_end; ++block_start)
	{
		if (hashfile->Seek(block_start*sha_size))
		{
			if (hashfile->Write(reinterpret_cast<const char*>(zero_hash), sha_size) != sha_size)
			{
				Server->Log("Error writing to hashfile while setting block to empty.", LL_WARNING);
				ret = false;
			}
		}
		else
		{
			Server->Log("Error seeking in hashfile setting block to empty.", LL_WARNING);
			ret = false;
		}
	}
	return ret;
}

void ServerVHDWriter::setDoMakeFull( bool b )
{
	do_make_full=b;
}

//-------------FilebufferWriter-----------------

ServerFileBufferWriter::ServerFileBufferWriter(ServerVHDWriter *pParent, unsigned int pBlocksize) : parent(pParent), blocksize(pBlocksize)
{
	mutex=Server->createMutex();
	cond=Server->createCondition();
	exit=false;
	exit_now=false;
	written=free_space_lim;
}

ServerFileBufferWriter::~ServerFileBufferWriter(void)
{
	while(!fb_queue.empty())
	{
		parent->freeFile(fb_queue.front());
		fb_queue.pop();
	}
	Server->destroy(mutex);
	Server->destroy(cond);
}

void ServerFileBufferWriter::operator()(void)
{
	char *blockbuf=new char[blocksize+sizeof(FileBufferVHDItem)+1];
	unsigned int blockbuf_size=blocksize+sizeof(FileBufferVHDItem)+1;

	while(!exit_now)
	{
		IFile* tmp;
		bool do_exit;
		bool has_item=false;
		{
			IScopedLock lock(mutex);
			while(fb_queue.empty() && exit==false)
			{
				cond->wait(&lock);
			}
			do_exit=exit;
			if(!fb_queue.empty())
			{
				has_item=true;
				tmp=fb_queue.front();
				fb_queue.pop();
			}
		}

		if(has_item)
		{
			tmp->Seek(0);
			uint64 tpos=0;
			uint64 tsize=tmp->Size();
			char next_type = -1;

			while(tpos<tsize)
			{
				if(exit_now)
					break;

				if(!parent->hasError())
				{
					bool old_method=false;
					if(next_type!=0 || tmp->Read(blockbuf+1, blockbuf_size-1)!= blockbuf_size-1)
					{
						old_method=true;
					}
					else
					{
						FileBufferVHDItem *item=(FileBufferVHDItem*)blockbuf;
						if(blockbuf_size-1==item->bsize+sizeof(FileBufferVHDItem) )
						{
							parent->writeVHD(item->pos, blockbuf+sizeof(FileBufferVHDItem), item->bsize);
							written+=item->bsize;
							tpos+=item->bsize+sizeof(FileBufferVHDItem);
							next_type = blockbuf[blockbuf_size - 1];
						}
						else
						{
							old_method=true;
						}
					}

					if(old_method==true)
					{
						tmp->Seek(tpos);
						FileBufferVHDItem item;
						if(tmp->Read((char*)&item, sizeof(FileBufferVHDItem))!=sizeof(FileBufferVHDItem))
						{
							Server->Log("Error reading FileBufferVHDItem", LL_ERROR);
							exit_now=true;
							parent->setHasError(true);
							break;
						}
						tpos+=sizeof(FileBufferVHDItem);
						if (item.type==1)
						{
							parent->writeVHD(item.pos, NULL, item.bsize);
							next_type = -1;
						}
						else if(item.type==0)
						{
							unsigned int tw = item.bsize;
							if (tpos + tw > tsize)
							{
								Server->Log("Size field is wrong", LL_ERROR);
								exit_now = true;
								parent->setHasError(true);
								break;
							}
							if (tw+1 > blockbuf_size)
							{
								delete[]blockbuf;
								blockbuf = new char[tw + 1 + sizeof(FileBufferVHDItem)];
								blockbuf_size = tw + 1 + sizeof(FileBufferVHDItem);
							}
							unsigned int read = tmp->Read(blockbuf, tw+1);
							if (read < tw)
							{
								Server->Log("Error reading from tmp.f", LL_ERROR);
								exit_now = true;
								parent->setHasError(true);
								break;
							}

							if (read == tw+1)
							{
								next_type = blockbuf[tw];
							}
							else
							{
								next_type = -1;
							}

							parent->writeVHD(item.pos, blockbuf, tw);
							written += tw;
							tpos += item.bsize;
						}
						else
						{
							Server->Log("Wrong type: "+convert((int)item.type), LL_ERROR);
							exit_now = true;
							parent->setHasError(true);
							break;
						}
					}

					if( written>=free_space_lim/2)
					{
						written=0;
						parent->checkFreeSpaceAndCleanup();
					}
				}
				else
				{
					break;
				}
			}
			parent->freeFile(tmp);
		}
		else if(do_exit)
		{
			break;
		}
	}

	delete []blockbuf;
}

void ServerFileBufferWriter::doExit(void)
{
	IScopedLock lock(mutex);
	exit=true;
	cond->notify_all();
}

void ServerFileBufferWriter::doExitNow(void)
{
	IScopedLock lock(mutex);
	exit_now=true;
	exit=true;
	cond->notify_all();
}

void ServerFileBufferWriter::writeBuffer(IFile *buf)
{
	IScopedLock lock(mutex);
	fb_queue.push(buf);
	cond->notify_all();
}

