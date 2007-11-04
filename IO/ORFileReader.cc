// ORFileReader.cc

#include "ORFileReader.hh"

#include <string>
#include "ORLogger.hh"

using namespace std;

ORFileReader::ORFileReader(string filename)
{
  if (filename != "") AddFileToProcess(filename);
}

size_t ORFileReader::Read(char* buffer, size_t nBytesMax)
{
  read(buffer, nBytesMax);
  size_t bytesLeft = nBytesMax - gcount();
  if(bytesLeft > 0) {
    Close();
    if(Open()) bytesLeft -= Read(buffer, bytesLeft);
  }
  return nBytesMax-bytesLeft;
}

bool ORFileReader::OpenDataStream()
{
  if(fFileList.size() > 0) {
    ORLog(kDebug) << "OpenDataStream(): opening file " << fFileList[0] << endl;
    open(fFileList[0].c_str());
    if(!OKToRead()) {
      ORLog(kError) << "Could not open file " << fFileList[0] << endl;
      return false;
    }
    fFileList.erase(fFileList.begin());
    return true;
  }
  else {
    ORLog(kDebug) << "OpenDataStream(): no more files to open. " << endl;
    return false;
  }
}
