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

#include "../../Interface/Server.h"
#include "../../stringtools.h"
#include "ntfs.h"
#include <math.h>
#include <memory.h>

#ifndef _WIN32
#define UD_UINT64 0xFFFFFFFFFFFFFFFFULL
#else
#define UD_UINT64 0xFFFFFFFFFFFFFFFF
#endif

class MemFree
{
public:
	MemFree(char *buf) : buf(buf) {}
	~MemFree(void) { delete []buf; }

private:
	char* buf;
};

FSNTFS::FSNTFS(const std::string &pDev, IFSImageFactory::EReadaheadMode read_ahead, bool background_priority, IFsNextBlockCallback* next_block_callback, bool check_mft_mirror, bool fix)
	: Filesystem(pDev, read_ahead, next_block_callback), bitmap(NULL)
{
	init(check_mft_mirror, fix);
	initReadahead(read_ahead, background_priority);
}

FSNTFS::FSNTFS(IFile *pDev, IFSImageFactory::EReadaheadMode read_ahead, bool background_priority, IFsNextBlockCallback* next_block_callback, bool check_mft_mirror, bool fix)
	: Filesystem(pDev, next_block_callback), bitmap(NULL)
{
	init(check_mft_mirror, fix);
	initReadahead(read_ahead, background_priority);
}

void FSNTFS::init(bool check_mft_mirror, bool fix)
{
	if(has_error)
		return;

	bitmap=NULL;
	sectorsize=512;
	NTFSBootRecord br;
	_u32 rc=sectorRead(0, (char*)&br, sizeof(NTFSBootRecord) );
	if(rc!=sizeof(NTFSBootRecord) )
	{
		has_error=true;
		Server->Log("Error reading boot record", LL_ERROR);
		return;
	}
	if( br.magic[0]!='N' || br.magic[1]!='T' || br.magic[2]!='F' || br.magic[3]!='S' )
	{
		has_error=true;
		Server->Log("NTFS magic wrong", LL_ERROR);
		return;
	}

	sectorsize=br.bytespersector;
	clustersize=sectorsize*br.sectorspercluster;
	drivesize=br.numberofsectors*sectorsize;

	Server->Log("Sectorsize: "+convert(sectorsize), LL_DEBUG);
	Server->Log("Clustersize: "+convert(clustersize), LL_DEBUG);
	Server->Log("ClustersPerMFTNode Offset: "+convert((uint64)&br.mftlcn-(uint64)&br), LL_DEBUG);

	unsigned int mftrecordsize;
	if(br.clusterspermftrecord<0)
	{
		mftrecordsize=1 << (-br.clusterspermftrecord);
	}
	else
	{
		mftrecordsize=br.clusterspermftrecord*clustersize;
	}

	uint64 mftstart=br.mftlcn*clustersize;
	Server->Log("MFTStart: "+convert(br.mftlcn), LL_DEBUG);

	char *mftrecord=new char[mftrecordsize];
	MemFree mftrecord_free(mftrecord);

	rc=sectorRead(mftstart, mftrecord, mftrecordsize);
	if(rc!=mftrecordsize )
	{
		has_error=true;
		Server->Log("Error reading MFTRecord", LL_ERROR);
		return;
	}

	NTFSFileRecord mft;
	memcpy((char*)&mft, mftrecord, sizeof(NTFSFileRecord) );

	if(!applyFixups(mftrecord, mftrecordsize, mftrecord+mft.sequence_offset, mft.sequence_size*2 ) )
	{
		Server->Log("Applying fixups failed", LL_ERROR);
		has_error=true;
		return;
	}
	
	if(mft.magic[0]!='F' || mft.magic[1]!='I' || mft.magic[2]!='L' || mft.magic[3]!='E' )
	{
		has_error=true;
		Server->Log("NTFSFileRecord magic wrong", LL_ERROR);
		return;
	}

	_u32 currpos=0;
	MFTAttribute attr;
	attr.length=mft.attribute_offset;
	do
	{
		currpos+=attr.length;
		memcpy((char*)&attr, mftrecord+currpos, sizeof(MFTAttribute) );
		if(attr.type==0x30 && attr.nonresident==0) //FILENAME
		{
			MFTAttributeFilename fn;
			memcpy((char*)&fn, mftrecord+currpos+attr.attribute_offset, sizeof(MFTAttributeFilename) );
			std::string fn_uc;
			fn_uc.resize(fn.filename_length*2);
			memcpy(&fn_uc[0], mftrecord+currpos+attr.attribute_offset+sizeof(MFTAttributeFilename), fn.filename_length*2);
			Server->Log("Filename="+Server->ConvertFromUTF16(fn_uc) , LL_DEBUG);
		}
		Server->Log("Attribute Type: "+convert(attr.type)+" nonresident="+convert(attr.nonresident)+" length="+convert(attr.length), LL_DEBUG);
	}while( attr.type!=0xFFFFFFFF && attr.type!=0x80);

	if(attr.type==0xFFFFFFFF )
	{
		has_error=true;
		Server->Log("Data attribute not found", LL_ERROR);
		return;
	}

	if(attr.nonresident!=1)
	{
		Server->Log("DATA is resident!! - unexpected", LL_ERROR);
		has_error=true;
		return;
	}

	MFTAttributeNonResident datastream;
	memcpy((char*)&datastream, mftrecord+currpos, sizeof(MFTAttributeNonResident) );
	if(datastream.compression_size!=0)
	{
		Server->Log("MFT Rundata is compressed. Can't handle that", LL_ERROR);
		has_error=true;
		return;
	}

	Runlist mftrunlist(mftrecord+currpos+datastream.run_offset );

	unsigned int bitmap_vcn=(6*mftrecordsize)/clustersize;
	uint64 bitmap_lcn=mftrunlist.getLCN(bitmap_vcn);
	if(bitmap_lcn==UD_UINT64)
	{
		Server->Log("Error mapping VCN to LCN", LL_ERROR);
		has_error=true;
		return;
	}
	uint64 bitmap_pos=bitmap_lcn*clustersize+(6*mftrecordsize)%clustersize;
	char *bitmaprecord=new char[mftrecordsize];
	MemFree bitmaprecord_free(bitmaprecord);

	rc=sectorRead(bitmap_pos, bitmaprecord, mftrecordsize);
	if(rc==0)
	{
		Server->Log("Error reading bitmap MFT entry", LL_DEBUG);
		has_error=true;
		return;
	}

	NTFSFileRecord bitmapf;
	memcpy((char*)&bitmapf, bitmaprecord, sizeof(NTFSFileRecord) );

	if(!applyFixups(bitmaprecord, mftrecordsize, bitmaprecord+bitmapf.sequence_offset, bitmapf.sequence_size*2 ) )
	{
		Server->Log("Applying fixups failed", LL_ERROR);
		has_error=true;
		return;
	}
	
	if(bitmapf.magic[0]!='F' || bitmapf.magic[1]!='I' || bitmapf.magic[2]!='L' || bitmapf.magic[3]!='E' )
	{
		has_error=true;
		Server->Log("NTFSFileRecord magic wrong -2", LL_ERROR);
		return;
	}

	currpos=0;
	attr.length=mft.attribute_offset;
	bool is_bitmap=false;
	do
	{
		currpos+=attr.length;
		memcpy((char*)&attr, bitmaprecord+currpos, sizeof(MFTAttribute) );
		if(attr.type==0x30 && attr.nonresident==0) //FILENAME
		{
			MFTAttributeFilename fn;
			memcpy((char*)&fn, bitmaprecord+currpos+attr.attribute_offset, sizeof(MFTAttributeFilename) );
			std::string fn_uc;
			fn_uc.resize(fn.filename_length*2);
			memcpy(&fn_uc[0], bitmaprecord+currpos+attr.attribute_offset+sizeof(MFTAttributeFilename), fn.filename_length*2);
			Server->Log("Filename="+Server->ConvertFromUTF16(fn_uc) , LL_DEBUG);
			if(Server->ConvertFromUTF16(fn_uc)=="$Bitmap")
			{
				is_bitmap=true;
			}
		}
		Server->Log("Attribute Type: "+convert(attr.type)+" nonresident="+convert(attr.nonresident)+" length="+convert(attr.length), LL_DEBUG);
	}while( attr.type!=0xFFFFFFFF && attr.type!=0x80);

	if(!is_bitmap)
	{
		Server->Log("Filename attribute not found or filename wrong", LL_ERROR);
		has_error=true;
		return;
	}

	if(attr.type!=0x80)
	{
		Server->Log("Data Attribute of Bitmap not found", LL_ERROR);
		has_error=true;
		return;
	}

	if(attr.nonresident!=1)
	{
		Server->Log("DATA is resident!! - unexpected -2", LL_ERROR);
		has_error=true;
		return;
	}

	MFTAttributeNonResident bitmapstream;
	memcpy((char*)&bitmapstream, bitmaprecord+currpos, sizeof(MFTAttributeNonResident) );
	if(bitmapstream.compression_size!=0)
	{
		Server->Log("MFT Rundata is compressed. Can't handle that. -2", LL_ERROR);
		has_error=true;
		return;
	}

	Runlist bitmaprunlist(bitmaprecord+currpos+bitmapstream.run_offset );
	
	Server->Log("Bitmap size="+convert(bitmapstream.real_size), LL_DEBUG);
	bitmap=new unsigned char[(unsigned int)bitmapstream.real_size];
	char *buffer=new char[clustersize];
	MemFree buffer_free(buffer);
	bitmap_pos=0;
	for(uint64 i=bitmapstream.starting_vnc;i<=bitmapstream.last_vnc;++i)
	{
		uint64 lcn=bitmaprunlist.getLCN(i);
		if(lcn==UD_UINT64)
		{
			Server->Log("Error mapping VCN->LCN. -2", LL_ERROR);
			has_error=true;
			return;
		}
		dev->Seek(lcn*clustersize);
		rc=dev->Read(buffer, clustersize);
		if(rc!=clustersize)
		{
			Server->Log("Error reading cluster "+convert(lcn)+" code: 529", LL_ERROR);
			has_error=true;
			return;
		}
		memcpy(&bitmap[bitmap_pos], buffer, (size_t)(std::min)(bitmapstream.real_size-bitmap_pos, (uint64)clustersize) );
		bitmap_pos+=(std::min)(bitmapstream.real_size-bitmap_pos, (uint64)clustersize);
	}

	if(check_mft_mirror)
	{
		if(!checkMFTMirror(mftrecordsize, mftrunlist, mft, true) )
		{
			Server->Log("MFT mirror check failed", LL_ERROR);
			has_error=true;
			return;
		}
	}
}

FSNTFS::~FSNTFS(void)
{
	delete [] bitmap;
}

_u32 FSNTFS::sectorRead(int64 pos, char *buffer, _u32 bsize)
{
	int64 rpos=pos-pos%sectorsize;
	dev->Seek(rpos);
	_u32 rbsize=(_u32)(pos-rpos)+bsize;
	rbsize=rbsize+(sectorsize-rbsize%sectorsize);
	char *rbuf=new char[rbsize];
	_u32 read=dev->Read(rbuf, rbsize);
	if(read!=rbsize && read<(pos-rpos)+bsize)
	{
		return 0;
	}
	memcpy(buffer, &rbuf[pos-rpos], bsize);
	delete [] rbuf;
	return bsize;
}

bool FSNTFS::applyFixups(char *data, size_t datasize, char* fixups, size_t fixups_size)
{
	unsigned int num_fixups=(unsigned int)datasize/sectorsize;
	if(num_fixups>(fixups_size-2)/2)
	{
		Server->Log("Number of fixups wrong!", LL_ERROR);
		return false;
	}
	char seq_number[2];
	memcpy(seq_number, fixups, 2);
	
	size_t t=0;
	for(size_t i=2;i<fixups_size;i+=2,++t)
	{
		if( data[(t+1)*sectorsize-2]!=seq_number[0] || data[(t+1)*sectorsize-1]!=seq_number[1] )
		{
			Server->Log("Cluster corrupted. Stopping. (Testing fixup failed)", LL_ERROR);
			return false;
		}
		data[(t+1)*sectorsize-2]=fixups[i];
		data[(t+1)*sectorsize-1]=fixups[i+1];
	}

	return true;
}

int64 FSNTFS::getBlocksize(void)
{
	return clustersize;
}

int64 FSNTFS::getSize(void)
{
	return drivesize;
}

const unsigned char * FSNTFS::getBitmap(void)
{
	return bitmap;
}

bool FSNTFS::checkMFTMirror(unsigned int mftrecordsize, Runlist &mftrunlist, NTFSFileRecord &mft, bool fix)
{
	unsigned int mirr_vcn=(1*mftrecordsize)/clustersize;
	uint64 mirr_lcn=mftrunlist.getLCN(mirr_vcn);
	if(mirr_lcn==UD_UINT64)
	{
		Server->Log("Error mapping VCN to LCN", LL_ERROR);
		return false;
	}
	uint64 mirr_pos=mirr_lcn*clustersize+(1*mftrecordsize)%clustersize;
	char *mirrrecord=new char[mftrecordsize];
	MemFree mirrrecord_free(mirrrecord);

	_u32 rc=sectorRead(mirr_pos, mirrrecord, mftrecordsize);
	if(rc==0)
	{
		Server->Log("Error reading bitmap MFT entry", LL_DEBUG);
		return false;
	}

	NTFSFileRecord mftmirr;
	memcpy((char*)&mftmirr, mirrrecord, sizeof(NTFSFileRecord) );

	if(!applyFixups(mirrrecord, mftrecordsize, mirrrecord+mftmirr.sequence_offset, mftmirr.sequence_size*2 ) )
	{
		Server->Log("Applying fixups failed", LL_ERROR);
		return false;
	}
	
	if(mftmirr.magic[0]!='F' || mftmirr.magic[1]!='I' || mftmirr.magic[2]!='L' || mftmirr.magic[3]!='E' )
	{
		has_error=true;
		return false;
	}

	_u32 currpos=0;
	MFTAttribute attr;
	attr.length=mft.attribute_offset;
	bool is_mftmirr=false;
	do
	{
		currpos+=attr.length;
		memcpy((char*)&attr, mirrrecord+currpos, sizeof(MFTAttribute) );
		if(attr.type==0x30 && attr.nonresident==0) //FILENAME
		{
			MFTAttributeFilename fn;
			memcpy((char*)&fn, mirrrecord+currpos+attr.attribute_offset, sizeof(MFTAttributeFilename) );
			std::string fn_uc;
			fn_uc.resize(fn.filename_length*2);
			memcpy(&fn_uc[0], mirrrecord+currpos+attr.attribute_offset+sizeof(MFTAttributeFilename), fn.filename_length*2);
			Server->Log("Filename="+Server->ConvertFromUTF16(fn_uc) , LL_DEBUG);
			if(Server->ConvertFromUTF16(fn_uc)=="$MFTMirr")
			{
				is_mftmirr=true;
			}
		}
		Server->Log("Attribute Type: "+convert(attr.type)+" nonresident="+convert(attr.nonresident)+" length="+convert(attr.length), LL_DEBUG);
	}while( attr.type!=0xFFFFFFFF && attr.type!=0x80);

	if(!is_mftmirr)
	{
		Server->Log("Filename attribute not found or filename wrong", LL_ERROR);
		return false;
	}

	if(attr.type!=0x80)
	{
		Server->Log("Data Attribute of MftMirr not found", LL_ERROR);
		return false;
	}

	if(attr.nonresident!=1)
	{
		Server->Log("DATA is resident!! - unexpected -2", LL_ERROR);
		return false;
	}

	MFTAttributeNonResident mftmirrstream;
	memcpy((char*)&mftmirrstream, mirrrecord+currpos, sizeof(MFTAttributeNonResident) );
	if(mftmirrstream.compression_size!=0)
	{
		Server->Log("MFT Rundata is compressed. Can't handle that. -2", LL_ERROR);
		return false;
	}

	Runlist mirrrunlist(mirrrecord+currpos+mftmirrstream.run_offset );
	
	unsigned char *mftmirr_data=new unsigned char[(unsigned int)mftmirrstream.real_size];
	MemFree mftmirr_data_free(reinterpret_cast<char*>(mftmirr_data));
	char *buffer=new char[clustersize];
	MemFree buffer_free(buffer);
	mirr_pos=0;
	for(uint64 i=mftmirrstream.starting_vnc;i<=mftmirrstream.last_vnc;++i)
	{
		uint64 lcn=mirrrunlist.getLCN(i);
		if(lcn==UD_UINT64)
		{
			Server->Log("Error mapping VCN->LCN. -2", LL_ERROR);
			return false;
		}
		dev->Seek(lcn*clustersize);
		rc=dev->Read(buffer, clustersize);
		if(rc!=clustersize)
		{
			Server->Log("Error reading cluster "+convert(lcn)+" code: 529", LL_ERROR);
			return false;
		}
		memcpy(&mftmirr_data[mirr_pos], buffer, (size_t)(std::min)(mftmirrstream.real_size-mirr_pos, (uint64)clustersize) );
		mirr_pos+=(std::min)(mftmirrstream.real_size-mirr_pos, (uint64)clustersize);
	}

	bool has_fix=false;

	for(uint64 i=0;i<mftmirrstream.real_size/mftrecordsize;++i)
	{
		uint64 currfile_vcn=(i*mftrecordsize)/clustersize;
		uint64 currfile_lcn=mftrunlist.getLCN(currfile_vcn);
		if(currfile_lcn==UD_UINT64)
		{
			Server->Log("Error mapping VCN to LCN", LL_ERROR);
			return false;
		}
		uint64 currfile_pos=currfile_lcn*clustersize+(1*mftrecordsize)%clustersize;
		char *currfilerecord=new char[mftrecordsize];
		MemFree currfilerecord_free(currfilerecord);

		_u32 rc=sectorRead(currfile_pos, currfilerecord, mftrecordsize);
		if(rc==0)
		{
			Server->Log("Error reading currfile MFT entry", LL_DEBUG);
			return false;
		}

		NTFSFileRecord *A=(NTFSFileRecord *)currfilerecord;
		NTFSFileRecord *B=(NTFSFileRecord *)(mftmirr_data+i*mftrecordsize);

		if(A->lsn!=B->lsn)
		{
			Server->Log("MFT file record in MFT mirror differs in file record "+convert(i)+": Logfile sequence number differs", LL_WARNING);
		}

		if(memcmp(currfilerecord, mftmirr_data+i*mftrecordsize, mftrecordsize)!=0)
		{
			Server->Log("MFT file record in MFT mirror differs in file record "+convert(i), LL_WARNING);
			if(fix)
			{
				memcpy(mftmirr_data+i*mftrecordsize, currfilerecord, mftrecordsize);
				has_fix=true;
			}
			else
			{
				return false;
			}
		}
	}

	if(has_fix)
	{
		mirr_pos=0;
		for(uint64 i=mftmirrstream.starting_vnc;i<=mftmirrstream.last_vnc;++i)
		{
			uint64 lcn=mirrrunlist.getLCN(i);
			if(lcn==UD_UINT64)
			{
				Server->Log("Error mapping VCN->LCN. -2", LL_ERROR);
				return false;
			}
			dev->Seek(lcn*clustersize);
			rc=dev->Write((const char*)(mftmirr_data+i*clustersize), clustersize);
			if(rc!=clustersize)
			{
				Server->Log("Error writing cluster "+convert(lcn)+" code: 652", LL_WARNING);
				return false;
			}
		}

		Server->Log("Fixed MFT mirror", LL_ERROR);
	}

	return true;
}

//-------------- RUNLIST -----------------

Runlist::Runlist(char *pData) : data(pData)
{
	reset();
}

void Runlist::reset(void)
{
	pos=data;
}

bool Runlist::getNext(RunlistItem &item)
{
	char f=*pos;
	if(f==0)
		return false;

	char offset_size=f >> 4;
	char length_size=f &  0x0F;
	item.length=0;
	item.offset=0;
	memcpy(&item.length, pos+1, length_size);

	bool is_signed=(*(pos+1+length_size+offset_size-1) & 0x80)>0;
	memcpy(&item.offset, pos+1+length_size, offset_size);

	if(is_signed)
	{
		char * ar=(char*)&item.offset;
		ar[offset_size-1]=ar[offset_size-1] & 0x7F;
		item.offset*=-1;
	}

	pos+=1+offset_size+length_size;
	return true;
}

uint64 Runlist::getSizeInClusters(void)
{
	reset();
	RunlistItem item;
	uint64 size=0;
	while(getNext(item))
	{
		size+=item.length;
	}
	return size;
}

uint64 Runlist::getLCN(uint64 vcn)
{
	reset();
	RunlistItem item;
	uint64 lcn=0;
	uint64 coffset=0;
	while(getNext(item))
	{
		lcn+=item.offset;

		if(coffset<=vcn && coffset+item.length>vcn )
		{
			return lcn+(vcn-coffset);
		}

		coffset+=item.length;
	}
	return UD_UINT64;
}

