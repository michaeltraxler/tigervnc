/* Copyright (C) 2005 TightVNC Team.  All Rights Reserved.
 *    
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 *
 * TightVNC distribution homepage on the Web: http://www.tightvnc.com/
 *
 */

// -=- FTMsgReader.cxx

#include <vncviewer/FTMsgReader.h>

using namespace rfb;
using namespace rfb::win32;

FTMsgReader::FTMsgReader(rdr::InStream *pIS)
{
  m_pInStream = pIS;
}

FTMsgReader::~FTMsgReader()
{

}

int 
FTMsgReader::readFileListData(FileInfo *pFileInfo)
{
  unsigned char flags = m_pInStream->readU8();
  int numFiles = m_pInStream->readU16();
  int dataSize = m_pInStream->readU16();
  int compressedSize = m_pInStream->readU16();
  
  if (flags & 0x80) {
    return -1;
  } else {
    if (numFiles > 0) {
      char *pFilenames = new char[compressedSize];
      SIZEDATAINFO *pSDI = new SIZEDATAINFO[numFiles];
      for (int i = 0; i < numFiles; i++) {
        pSDI[i].size = m_pInStream->readU32();
        pSDI[i].data = m_pInStream->readU32();
      }
      m_pInStream->readBytes((void *)pFilenames, compressedSize);
      createFileInfo(numFiles, pFileInfo, pSDI, pFilenames);
      delete [] pSDI;
      delete [] pFilenames;
    }
  }
  return numFiles;
}

void * 
FTMsgReader::readFileDownloadData(unsigned int *pSize, unsigned int *pModTime)
{
  unsigned char compressLevel = m_pInStream->readU8();
  int realSize = m_pInStream->readU16();
  int compressedSize = m_pInStream->readU16();

  if ((realSize == 0) && (compressedSize == 0)) {
    *pSize = 0;
    *pModTime = m_pInStream->readU32();
    return NULL;
  } else {
    char *pFile = new char [compressedSize];
    if (pFile == NULL) {
      m_pInStream->skip(compressedSize);
      *pModTime = 0;
      return NULL;
    } else {
      m_pInStream->readBytes(pFile, compressedSize);
      *pSize = compressedSize;
      return pFile;
    }
  }
}

char * 
FTMsgReader::readFileUploadCancel(unsigned int *pReasonSize)
{
  m_pInStream->skip(1);
  return readReasonMsg(pReasonSize);
}

char * 
FTMsgReader::readFileDownloadFailed(unsigned int *pReasonSize)
{
  m_pInStream->skip(1);
  return readReasonMsg(pReasonSize);
}

int 
FTMsgReader::readFileDirSizeData(DWORD64 *pdw64DirSize)
{
  m_pInStream->skip(1);
  unsigned short size16 = m_pInStream->readU16();
  unsigned int size32 = m_pInStream->readU32();
  DWORD64 dw64Size = 0;
  dw64Size = size16;
  dw64Size = (dw64Size << 32) + size32;
  *pdw64DirSize = dw64Size;
  return 1;
}

char * 
FTMsgReader::readFileLastRqstFailed(int *pTypeOfRequest, unsigned int *pReasonSize)
{
  *pTypeOfRequest = m_pInStream->readU8();
  return readReasonMsg(pReasonSize);
}

bool 
FTMsgReader::createFileInfo(unsigned int numFiles, FileInfo *fi, 
                            SIZEDATAINFO *pSDInfo, char *pFilenames)
{
  int pos = 0;
  int size = 0;
  for (unsigned int i = 0; i < numFiles; i++) {
    size = pSDInfo[i].size;
    if (size == -1) {
      fi->add((pFilenames + pos), size, pSDInfo[i].data, FT_ATTR_DIR);
    } else {
      fi->add((pFilenames + pos), size, pSDInfo[i].data, FT_ATTR_FILE);
    }
    pos += strlen(pFilenames + pos) + 1;
  }
  return true;
}

char * 
FTMsgReader::readReasonMsg(unsigned int *pReasonSize)
{
  int reasonLen = m_pInStream->readU16();
  int _reasonLen = reasonLen + 1;
  char *pReason;
  if (reasonLen == 0) {
    *pReasonSize = 0;
    return NULL;
  } else {
    pReason = new char [_reasonLen];
    if (pReason == NULL) {
      m_pInStream->skip(reasonLen);
      *pReasonSize = 0;
      return NULL;
    }
    m_pInStream->readBytes(pReason, reasonLen);
    memset(((char *)pReason+reasonLen), '\0', 1);
    return pReason;
  }
}

