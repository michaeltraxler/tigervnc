//  Copyright (C) 2003-2004 Dennis Syrovatsky. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.

#ifndef FILEINFO_H__
#define FILEINFO_H__

#include "FileTransferTypes.h"

class FileInfo  
{
public:
	void add(FileInfo *pFI);
	void add(FILEINFO *pFIStruct);
	void add(char *pName, unsigned int size, unsigned int data, unsigned int flags);

	char *getNameAt(unsigned int number);
	
	bool setNameAt(unsigned int number, char *pName);

	unsigned int getSizeAt(unsigned int number);
	unsigned int getDataAt(unsigned int number);
	unsigned int getFlagsAt(unsigned int number);

	FILEINFO *getFullDataAt(unsigned int number);
	
	bool setSizeAt(unsigned int number, unsigned int value);
	bool setDataAt(unsigned int number, unsigned int value);
	bool setFlagsAt(unsigned int number, unsigned int value);

	bool deleteAt(unsigned int number);
	
	unsigned int getNumEntries();
	
	void sort();
	void free();
	
	FileInfo();
	~FileInfo();

private:
	FILEINFO *m_pEntries;
	unsigned int m_numEntries;
};

#endif // FILEINFO_H__
